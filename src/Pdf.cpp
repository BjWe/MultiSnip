#include "Pdf.h"
#include "Image.h"
#include "resource.h"
#include <algorithm>
#include <objidl.h>
#include <objbase.h>
// gdiplus-Header verwenden min/max als Makros; mit NOMINMAX brauchen sie die std-Varianten.
using std::min;
using std::max;
#include <gdiplus.h>

// ---------------------------------------------------------------------------
// Ein PDF ist eine Folge numerierter Objekte, danach eine Tabelle mit deren
// Byte-Offsets. Hier entstehen pro Seite drei Objekte (Seite, Inhaltsstrom,
// Bild), dazu Katalog und Seitenbaum.
//
// Die Bilder werden verlustfrei eingebettet. PDF kennt unter /FlateDecode
// dieselben Zeilenfilter wie PNG (/Predictor 15), darum laesst sich der
// IDAT-Datenstrom eines PNG unveraendert uebernehmen - es wird weder neu
// kodiert noch eine Kompressionsbibliothek gebraucht. Dafuer muss das PNG
// 8 Bit, Farbtyp 2 (RGB) und uninterlaced sein; deshalb geht eine
// 24-bpp-Kopie in den GDI+-Encoder. Liefert er doch etwas anderes, greift
// ein Rueckfallweg mit unkomprimierten Deflate-Bloecken.
//
// Die GDI+-Initialisierung liegt in Image.cpp; den PNG-Encoder sucht dieses
// Modul selbst, damit es sich ohne das uebrige Programm testen laesst.
// ---------------------------------------------------------------------------

namespace {

const double kPtPerPx = 72.0 / 96.0;    // Bildschirmpixel in PDF-Punkte
const double kA4W = 595.276;            // 210 mm
const double kA4H = 841.890;            // 297 mm
const double kMargin = 28.35;           // 10 mm Rand im A4-Modus
const double kMaxPage = 14400.0;        // PDF-Grenze fuer Seitenmasse (200 Zoll)

// Zahlen mit hoechstens zwei Nachkommastellen. Nicht printf: das haengt am
// Locale und wuerde unter deutscher Einstellung ein Komma schreiben.
std::string Num(double v)
{
    bool neg = v < 0;
    if (neg) v = -v;
    long long hundredths = (long long)(v * 100.0 + 0.5);
    std::string s = std::to_string(hundredths / 100);
    long long f = hundredths % 100;
    if (f) {
        s += '.';
        s += (char)('0' + f / 10);
        if (f % 10) s += (char)('0' + f % 10);
    }
    return neg ? "-" + s : s;
}

// ------------------------------------------------------- Metadaten-Hilfsmittel

std::string Int2(int v) { return std::string(1, (char)('0' + v / 10 % 10)) + (char)('0' + v % 10); }

// Zeitstempel in beiden Schreibweisen, aus derselben Uhrzeit: PDF/A verlangt,
// dass /Info und XMP uebereinstimmen.
struct Stamp {
    std::string pdf;                    // D:YYYYMMDDHHmmSS+HH'mm'
    std::string xmp;                    // YYYY-MM-DDTHH:mm:ss+HH:mm
};

Stamp Now()
{
    SYSTEMTIME lt{}, ut{};
    GetLocalTime(&lt);
    GetSystemTime(&ut);

    // Zonenversatz aus der Differenz beider Uhren: das trifft die Sommerzeit
    // ohne sie selbst beurteilen zu muessen.
    FILETIME lf{}, uf{};
    long long mins = 0;
    if (SystemTimeToFileTime(&lt, &lf) && SystemTimeToFileTime(&ut, &uf)) {
        ULARGE_INTEGER a{}, b{};
        a.LowPart = lf.dwLowDateTime; a.HighPart = lf.dwHighDateTime;
        b.LowPart = uf.dwLowDateTime; b.HighPart = uf.dwHighDateTime;
        // 1 Minute = 6e8 Einheiten von 100 ns, halbe Minute = 3e8 zum Runden.
        long long diff = (long long)a.QuadPart - (long long)b.QuadPart;
        mins = (diff + (diff >= 0 ? 300000000LL : -300000000LL)) / 600000000LL;
    }
    char sign = mins < 0 ? '-' : '+';
    long long am = mins < 0 ? -mins : mins;
    std::string oh = Int2((int)(am / 60)), om = Int2((int)(am % 60));

    std::string y = std::to_string(lt.wYear);
    std::string date = y + Int2(lt.wMonth) + Int2(lt.wDay);
    std::string time = Int2(lt.wHour) + Int2(lt.wMinute) + Int2(lt.wSecond);

    Stamp s;
    s.pdf = "D:" + date + time + sign + oh + "'" + om + "'";
    s.xmp = y + "-" + Int2(lt.wMonth) + "-" + Int2(lt.wDay) + "T" +
            Int2(lt.wHour) + ":" + Int2(lt.wMinute) + ":" + Int2(lt.wSecond) +
            sign + oh + ":" + om;
    return s;
}

std::string Utf8(const std::wstring& s)
{
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return std::string();
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, NULL, NULL);
    return out;
}

std::string XmlEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default:
            // Steuerzeichen sind in XML 1.0 nicht erlaubt.
            if ((unsigned char)c >= 0x20 || c == '\t') out += c;
            break;
        }
    }
    return out;
}

// Textstring als UTF-16BE-Hexfolge mit BOM. Damit sind Umlaute und alles
// andere erledigt, ohne Klammern und Backslashes zu maskieren.
std::string PdfTextString(const std::wstring& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out = "<FEFF";
    for (wchar_t c : s) {
        unsigned v = (unsigned short)c;
        out += hex[(v >> 12) & 0xF];
        out += hex[(v >> 8) & 0xF];
        out += hex[(v >> 4) & 0xF];
        out += hex[v & 0xF];
    }
    out += ">";
    return out;
}

std::string Producer() { return std::string("MultiSnip ") + APP_VERSION_STR; }

// Nur ASCII, darum genuegt das Verbreitern der Zeichen.
std::wstring ProducerW()
{
    std::string s = Producer();
    return std::wstring(s.begin(), s.end());
}

// --------------------------------------------------------------- sRGB-Profil

// Das Profil wird nicht mitgeliefert, sondern aus dem Farbprofilordner von
// Windows gelesen: so haengt am Repository keine Fremddatei mit eigener
// Lizenzlage. GetColorDirectoryW statt eines festen Pfades, damit es auch im
// 32-Bit-Prozess und bei verschobenem Windows stimmt.
std::wstring ColorDir()
{
    std::wstring dir;
    HMODULE m = LoadLibraryW(L"mscms.dll");
    if (m) {
        typedef BOOL (WINAPI *PFN)(PCWSTR, PWSTR, PDWORD);
        PFN get = (PFN)GetProcAddress(m, "GetColorDirectoryW");
        if (get) {
            wchar_t buf[MAX_PATH]{};
            DWORD cb = sizeof(buf);
            if (get(NULL, buf, &cb)) dir = buf;
        }
        FreeLibrary(m);
    }
    if (dir.empty()) {
        wchar_t win[MAX_PATH]{};
        if (GetWindowsDirectoryW(win, MAX_PATH)) {
            // %windir%\System32\spool ist von der WOW64-Umleitung ausgenommen,
            // der Pfad stimmt also in beiden Bitbreiten.
            dir = std::wstring(win) + L"\\System32\\spool\\drivers\\color";
        }
    }
    return dir;
}

bool ReadFileBytes(const std::wstring& path, std::string& out, DWORD maxBytes)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart <= (LONGLONG)maxBytes;
    if (ok) {
        out.resize((size_t)size.QuadPart);
        DWORD rd = 0;
        ok = ReadFile(f, &out[0], (DWORD)out.size(), &rd, NULL) && rd == out.size();
    }
    DWORD err = ok ? 0 : GetLastError();
    CloseHandle(f);
    if (!ok) SetLastError(err ? err : ERROR_FILE_NOT_FOUND);
    return ok;
}

bool LoadSrgbProfile(std::string& out)
{
    std::wstring dir = ColorDir();
    if (dir.empty()) { SetLastError(ERROR_NOT_FOUND); return false; }
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';

    // Der erste Name ist der von Windows mitgelieferte; die anderen sind
    // Schreibweisen, die auf aelteren oder angepassten Systemen vorkommen.
    static const wchar_t* kNames[] = {
        L"sRGB Color Space Profile.icm",
        L"sRGB_v4_ICC_preference.icc",
        L"sRGB.icm",
    };
    for (const wchar_t* n : kNames) {
        if (ReadFileBytes(dir + n, out, 4u << 20)) return true;
    }
    SetLastError(ERROR_NOT_FOUND);         // eigener Code, damit die UI es erklaeren kann
    return false;
}

// XMP-Paket mit der PDF/A-Kennung. Inhalt muss zu /Info passen.
std::string XmpPacket(const std::wstring& title, const Stamp& st)
{
    std::string t = XmlEscape(Utf8(title));
    std::string p = XmlEscape(Producer());
    std::string s;
    s += "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
    s += "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n";
    s += " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n";
    s += "  <rdf:Description rdf:about=\"\" xmlns:pdfaid=\"http://www.aiim.org/pdfa/ns/id/\">\n";
    s += "   <pdfaid:part>1</pdfaid:part>\n";
    s += "   <pdfaid:conformance>B</pdfaid:conformance>\n";
    s += "  </rdf:Description>\n";
    s += "  <rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
    s += "   <dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">" + t + "</rdf:li></rdf:Alt></dc:title>\n";
    s += "  </rdf:Description>\n";
    s += "  <rdf:Description rdf:about=\"\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">\n";
    s += "   <xmp:CreatorTool>" + p + "</xmp:CreatorTool>\n";
    s += "   <xmp:CreateDate>" + st.xmp + "</xmp:CreateDate>\n";
    s += "   <xmp:ModifyDate>" + st.xmp + "</xmp:ModifyDate>\n";
    s += "  </rdf:Description>\n";
    s += "  <rdf:Description rdf:about=\"\" xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\">\n";
    s += "   <pdf:Producer>" + p + "</pdf:Producer>\n";
    s += "  </rdf:Description>\n";
    s += " </rdf:RDF>\n";
    s += "</x:xmpmeta>\n";
    // Auffuellen, damit Werkzeuge das Paket an Ort und Stelle aendern koennen.
    for (int i = 0; i < 20; i++) s += "                                                                           \n";
    s += "<?xpacket end=\"w\"?>\n";
    return s;
}

// Dokumentkennung: zwei gleiche Haelften, wie bei einer frischen Datei ueblich.
std::string DocId()
{
    static const char* hex = "0123456789ABCDEF";
    GUID g{};
    std::string out = "<";
    if (CoCreateGuid(&g) == S_OK) {
        const unsigned char* p = (const unsigned char*)&g;
        for (int i = 0; i < 16; i++) { out += hex[p[i] >> 4]; out += hex[p[i] & 0xF]; }
    }
    else {
        FILETIME ft{};
        GetSystemTimeAsFileTime(&ft);
        const unsigned char* p = (const unsigned char*)&ft;
        for (int i = 0; i < 16; i++) { out += hex[p[i % 8] >> 4]; out += hex[p[i % 8] & 0xF]; }
    }
    out += ">";
    return out;
}

// ------------------------------------------------------------------ PNG lesen

struct PngData {
    int w = 0, h = 0;
    int bitDepth = 0, colorType = 0, interlace = 0;
    std::string idat;                   // alle IDAT-Bloecke hintereinander = ein zlib-Strom
};

unsigned Be32(const unsigned char* p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

bool ParsePng(const std::string& png, PngData& out)
{
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (png.size() < 8 + 25 || memcmp(png.data(), sig, 8) != 0) return false;

    size_t pos = 8;
    bool haveIhdr = false;
    while (pos + 8 <= png.size()) {
        const unsigned char* p = (const unsigned char*)png.data() + pos;
        unsigned len = Be32(p);
        if (len > png.size() || pos + 12 + (size_t)len > png.size()) return false;
        const char* type = (const char*)p + 4;
        const unsigned char* data = p + 8;

        if (memcmp(type, "IHDR", 4) == 0) {
            if (len < 13) return false;
            out.w = (int)Be32(data);
            out.h = (int)Be32(data + 4);
            out.bitDepth = data[8];
            out.colorType = data[9];
            out.interlace = data[12];
            haveIhdr = true;
        }
        else if (memcmp(type, "IDAT", 4) == 0) {
            out.idat.append((const char*)data, len);
        }
        else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + (size_t)len;        // Laenge + Typ + Daten + CRC
    }
    return haveIhdr && out.w > 0 && out.h > 0 && !out.idat.empty();
}

// ------------------------------------------------------- Bitmap als PNG (24 bpp)

bool PngEncoderClsid(CLSID& out)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    Gdiplus::ImageCodecInfo* ici = (Gdiplus::ImageCodecInfo*)buf.data();
    if (Gdiplus::GetImageEncoders(num, size, ici) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(ici[i].MimeType, L"image/png") == 0) { out = ici[i].Clsid; return true; }
    }
    return false;
}

bool EncodePng24(HBITMAP bmp, int w, int h, std::string& out)
{
    CLSID clsid{};
    if (!PngEncoderClsid(clsid)) return false;

    Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromHBITMAP(bmp, NULL);
    if (!src) return false;

    // 24-bpp-Zwischenbild: so schreibt der Encoder Farbtyp 2 statt RGBA, und
    // nur das laesst sich unveraendert als /DeviceRGB einbetten.
    bool ok = false;
    {
        Gdiplus::Bitmap dst(w, h, PixelFormat24bppRGB);
        if (dst.GetLastStatus() == Gdiplus::Ok) {
            {
                Gdiplus::Graphics g(&dst);
                g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
                ok = g.DrawImage(src, 0, 0, w, h) == Gdiplus::Ok;
            }
            if (ok) {
                ok = false;
                IStream* st = nullptr;
                if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &st))) {
                    if (dst.Save(st, &clsid, NULL) == Gdiplus::Ok) {
                        STATSTG stat{};
                        if (SUCCEEDED(st->Stat(&stat, STATFLAG_NONAME)) &&
                            stat.cbSize.QuadPart > 0 && stat.cbSize.QuadPart < 0x7FFFFFFF) {
                            size_t n = (size_t)stat.cbSize.QuadPart;
                            out.resize(n);
                            LARGE_INTEGER zero{};
                            st->Seek(zero, STREAM_SEEK_SET, NULL);
                            ULONG rd = 0;
                            ok = SUCCEEDED(st->Read(&out[0], (ULONG)n, &rd)) && rd == n;
                        }
                    }
                    st->Release();
                }
            }
        }
    }
    delete src;
    return ok;
}

// ------------------------------------------------- Rueckfallweg ohne Kompression

void Adler32(const std::string& d, std::string& out)
{
    unsigned a = 1, b = 0;
    for (unsigned char c : d) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    unsigned v = (b << 16) | a;
    out += (char)((v >> 24) & 0xFF);
    out += (char)((v >> 16) & 0xFF);
    out += (char)((v >> 8) & 0xFF);
    out += (char)(v & 0xFF);
}

// Gueltiger zlib-Strom aus "stored"-Bloecken: kostet nichts an Rechenzeit und
// braucht keine Bibliothek, komprimiert dafuer auch nicht.
std::string StoredDeflate(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    out += (char)0x78;
    out += (char)0x01;                  // zlib: Deflate, 32k-Fenster, keine Kompression
    size_t pos = 0;
    do {
        size_t n = min<size_t>(65535, raw.size() - pos);
        bool last = pos + n >= raw.size();
        out += (char)(last ? 1 : 0);
        out += (char)(n & 0xFF);
        out += (char)((n >> 8) & 0xFF);
        out += (char)(~n & 0xFF);
        out += (char)((~n >> 8) & 0xFF);
        out.append(raw, pos, n);
        pos += n;
    } while (pos < raw.size());
    Adler32(raw, out);
    return out;
}

// Rohe RGB-Zeilen von oben nach unten. GetDIBits wandelt dabei gleich auf
// 24 bpp; uebrig bleibt das Vertauschen von B und R und die Zeilenpolsterung.
bool RawRgb(HBITMAP bmp, int w, int h, std::string& out)
{
    if (w <= 0 || h <= 0) return false;
    size_t stride = ((size_t)w * 3 + 3) & ~(size_t)3;
    std::vector<BYTE> buf(stride * (size_t)h);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;         // negativ = top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC dc = GetDC(NULL);
    int got = GetDIBits(dc, bmp, 0, h, buf.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    if (got != h) return false;

    out.resize((size_t)w * 3 * (size_t)h);
    for (int y = 0; y < h; y++) {
        const BYTE* s = buf.data() + stride * (size_t)y;
        char* d = &out[(size_t)w * 3 * (size_t)y];
        for (int x = 0; x < w; x++) {
            d[x * 3 + 0] = (char)s[x * 3 + 2];
            d[x * 3 + 1] = (char)s[x * 3 + 1];
            d[x * 3 + 2] = (char)s[x * 3 + 0];
        }
    }
    return true;
}

// --------------------------------------------------------------- Seitengeometrie

struct PageGeom {
    double pw = 0, ph = 0;              // Seite in Punkten
    double ix = 0, iy = 0;              // linke untere Ecke des Bildes
    double iw = 0, ih = 0;              // gezeichnete Bildgroesse
};

PageGeom Geometry(int w, int h, PdfPaper paper)
{
    PageGeom g;
    if (paper == PdfPaper::A4) {
        bool landscape = w > h;         // breite Ausschnitte quer, sonst hoch
        g.pw = landscape ? kA4H : kA4W;
        g.ph = landscape ? kA4W : kA4H;
        double availW = g.pw - 2 * kMargin;
        double availH = g.ph - 2 * kMargin;
        double s = min(availW / w, availH / h);
        g.iw = w * s;
        g.ih = h * s;
        g.ix = (g.pw - g.iw) / 2;
        g.iy = (g.ph - g.ih) / 2;
    }
    else {
        g.pw = w * kPtPerPx;
        g.ph = h * kPtPerPx;
        // Sehr lange Stapel sprengen die PDF-Grenze; dann gleichmaessig kleiner.
        double s = 1.0;
        if (g.pw > kMaxPage) s = min(s, kMaxPage / g.pw);
        if (g.ph > kMaxPage) s = min(s, kMaxPage / g.ph);
        g.pw *= s;
        g.ph *= s;
        g.iw = g.pw;
        g.ih = g.ph;
    }
    return g;
}

// ------------------------------------------------------------------ PDF bauen

// Objektnummern werden zuerst vergeben und erst danach geschrieben: der
// Katalog verweist auf Objekte, die hinter ihm in der Datei stehen, und die
// Querverweistabelle bildet Nummern auf Offsets ab - die Reihenfolge in der
// Datei ist dafuer gleichgueltig.
struct Writer {
    std::string buf;
    std::vector<size_t> offsets;        // Offset von Objekt 1..n, 0 = ungeschrieben

    int Reserve() { offsets.push_back(0); return (int)offsets.size(); }
    void Begin(int id)
    {
        offsets[(size_t)id - 1] = buf.size();
        buf += std::to_string(id) + " 0 obj\n";
    }
    void End() { buf += "endobj\n"; }
    void Put(const std::string& s) { buf += s; }
    static std::string Ref(int id) { return std::to_string(id) + " 0 R"; }
};

struct PageImage {
    int w = 0, h = 0;
    std::string stream;                 // fertige Streamdaten
    bool predictor = false;             // true = PNG-Zeilenfilter (/Predictor 15)
};

bool PrepareImage(HBITMAP bmp, PageImage& out)
{
    BITMAP bm{};
    if (GetObjectW(bmp, sizeof(bm), &bm) != sizeof(bm)) return false;
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0) return false;
    out.w = bm.bmWidth;
    out.h = bm.bmHeight;

    std::string png;
    PngData pd;
    if (EncodePng24(bmp, out.w, out.h, png) && ParsePng(png, pd) &&
        pd.w == out.w && pd.h == out.h &&
        pd.bitDepth == 8 && pd.colorType == 2 && pd.interlace == 0) {
        out.stream = std::move(pd.idat);
        out.predictor = true;
        return true;
    }

    // Der Encoder hat etwas anderes geliefert: unkomprimiert einbetten.
    std::string raw;
    if (!RawRgb(bmp, out.w, out.h, raw)) return false;
    out.stream = StoredDeflate(raw);
    out.predictor = false;
    return true;
}

bool WriteFileAtomic(const std::wstring& path, const std::string& bytes)
{
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    size_t pos = 0;
    while (pos < bytes.size() && ok) {
        DWORD chunk = (DWORD)min<size_t>(1u << 20, bytes.size() - pos);
        DWORD wrote = 0;
        ok = WriteFile(f, bytes.data() + pos, chunk, &wrote, NULL) && wrote == chunk;
        pos += wrote;
    }
    if (ok) ok = FlushFileBuffers(f) != FALSE;
    DWORD err = ok ? 0 : GetLastError();
    CloseHandle(f);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        SetLastError(err ? err : ERROR_WRITE_FAULT);
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = GetLastError();
        DeleteFileW(tmp.c_str());
        SetLastError(err);
        return false;
    }
    return true;
}

} // namespace

bool Pdf_WriteBitmaps(const std::vector<HBITMAP>& pages, const PdfOptions& o, const std::wstring& path)
{
    if (pages.empty()) { SetLastError(ERROR_INVALID_PARAMETER); return false; }

    // Das Farbprofil zuerst holen: ohne OutputIntent gibt es kein PDF/A, und
    // dann soll die Arbeit gar nicht erst anfangen.
    std::string iccData;
    if (o.pdfA && !LoadSrgbProfile(iccData)) return false;

    std::wstring title = o.title.empty() ? L"MultiSnip" : o.title;

    std::vector<PageImage> imgs(pages.size());
    for (size_t i = 0; i < pages.size(); i++) {
        if (!pages[i] || !PrepareImage(pages[i], imgs[i])) {
            SetLastError(ERROR_INVALID_DATA);
            return false;
        }
    }

    std::vector<PageGeom> geoms(imgs.size());
    for (size_t i = 0; i < imgs.size(); i++) geoms[i] = Geometry(imgs[i].w, imgs[i].h, o.paper);

    // Erst alle Objektnummern vergeben, dann schreiben.
    Writer w;
    int catalog = w.Reserve();
    int tree = w.Reserve();
    int info = w.Reserve();
    int xmp = o.pdfA ? w.Reserve() : 0;
    int icc = o.pdfA ? w.Reserve() : 0;
    std::vector<int> pageIds(imgs.size()), contentIds(imgs.size()), imageIds(imgs.size());
    for (size_t i = 0; i < imgs.size(); i++) {
        pageIds[i] = w.Reserve();
        contentIds[i] = w.Reserve();
        imageIds[i] = w.Reserve();
    }

    Stamp st = Now();
    std::string id = DocId();
    std::wstring prod = ProducerW();

    w.buf += "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";       // Binaer-Kommentar: Datei gilt als binaer

    // Beim Oeffnen die lange Einzelseite auf Breite einpassen (/FitH am obersten
    // Rand), bei A4-Seiten die ganze Seite (/Fit).
    std::string openAction = o.pages == PdfPages::Single
        ? "[" + Writer::Ref(pageIds[0]) + " /FitH " + Num(geoms[0].ph) + "]"
        : "[" + Writer::Ref(pageIds[0]) + " /Fit]";

    w.Begin(catalog);
    {
        std::string cat = "<< /Type /Catalog /Pages " + Writer::Ref(tree) +
                          " /PageLayout /OneColumn"
                          " /ViewerPreferences << /DisplayDocTitle true >>"
                          " /OpenAction " + openAction;
        if (o.pdfA) {
            cat += " /Metadata " + Writer::Ref(xmp) +
                   " /OutputIntents [ << /Type /OutputIntent /S /GTS_PDFA1"
                   " /OutputConditionIdentifier (sRGB IEC61966-2.1)"
                   " /RegistryName (http://www.color.org)"
                   " /Info (sRGB IEC61966-2.1)"
                   " /DestOutputProfile " + Writer::Ref(icc) + " >> ]";
        }
        cat += " >>\n";
        w.Put(cat);
    }
    w.End();

    std::string kids;
    for (size_t i = 0; i < imgs.size(); i++) {
        if (i) kids += " ";
        kids += Writer::Ref(pageIds[i]);
    }
    w.Begin(tree);
    w.Put("<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(imgs.size()) + " >>\n");
    w.End();

    // /Info und XMP muessen dieselben Angaben tragen, sonst beanstanden
    // PDF/A-Pruefer den Widerspruch.
    w.Begin(info);
    w.Put("<< /Title " + PdfTextString(title) +
          " /Creator " + PdfTextString(prod) +
          " /Producer " + PdfTextString(prod) +
          " /CreationDate (" + st.pdf + ")"
          " /ModDate (" + st.pdf + ") >>\n");
    w.End();

    if (o.pdfA) {
        // Das XMP-Paket bleibt unkomprimiert und ohne Filter: PDF/A verlangt,
        // dass es ohne Dekodieren lesbar ist.
        std::string packet = XmpPacket(title, st);
        w.Begin(xmp);
        w.Put("<< /Type /Metadata /Subtype /XML /Length " + std::to_string(packet.size()) + " >>\nstream\n");
        w.Put(packet);
        w.Put("\nendstream\n");
        w.End();

        w.Begin(icc);
        w.Put("<< /N 3 /Length " + std::to_string(iccData.size()) + " >>\nstream\n");
        w.Put(iccData);
        w.Put("\nendstream\n");
        w.End();
    }

    for (size_t i = 0; i < imgs.size(); i++) {
        const PageImage& im = imgs[i];
        const PageGeom& g = geoms[i];

        w.Begin(pageIds[i]);
        w.Put("<< /Type /Page /Parent " + Writer::Ref(tree) +
              " /MediaBox [0 0 " + Num(g.pw) + " " + Num(g.ph) + "]"
              " /Resources << /XObject << /Im0 " + Writer::Ref(imageIds[i]) + " >> >>"
              " /Contents " + Writer::Ref(contentIds[i]) + " >>\n");
        w.End();

        // Die Bildmatrix spannt das Einheitsquadrat auf die Zielflaeche auf;
        // Zeile 0 des Bildes landet dabei oben.
        std::string content = "q " + Num(g.iw) + " 0 0 " + Num(g.ih) + " " +
                              Num(g.ix) + " " + Num(g.iy) + " cm /Im0 Do Q\n";
        w.Begin(contentIds[i]);
        w.Put("<< /Length " + std::to_string(content.size()) + " >>\nstream\n");
        w.Put(content);
        w.Put("endstream\n");
        w.End();

        std::string parms;
        if (im.predictor) {
            parms = " /DecodeParms << /Predictor 15 /Colors 3 /BitsPerComponent 8 /Columns " +
                    std::to_string(im.w) + " >>";
        }
        w.Begin(imageIds[i]);
        w.Put("<< /Type /XObject /Subtype /Image /Width " + std::to_string(im.w) +
              " /Height " + std::to_string(im.h) +
              " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode" + parms +
              " /Length " + std::to_string(im.stream.size()) + " >>\nstream\n");
        w.Put(im.stream);
        w.Put("\nendstream\n");
        w.End();
    }

    // Querverweistabelle: je Eintrag genau 20 Byte.
    size_t xref = w.buf.size();
    w.buf += "xref\n0 " + std::to_string(w.offsets.size() + 1) + "\n";
    w.buf += "0000000000 65535 f \n";
    for (size_t off : w.offsets) {
        std::string s = std::to_string(off);
        w.buf += std::string(10 - min<size_t>(10, s.size()), '0') + s + " 00000 n \n";
    }
    w.buf += "trailer\n<< /Size " + std::to_string(w.offsets.size() + 1) +
             " /Root " + Writer::Ref(catalog) +
             " /Info " + Writer::Ref(info) +
             " /ID [" + id + " " + id + "] >>\n";
    w.buf += "startxref\n" + std::to_string(xref) + "\n%%EOF\n";

    return WriteFileAtomic(path, w.buf);
}

bool Pdf_ExportTab(const Tab& t, const PdfOptions& o, const std::wstring& path, int* pageCount)
{
    std::vector<HBITMAP> pages;
    bool ok = false;
    if (pageCount) *pageCount = 0;

    PdfOptions opt = o;
    if (opt.title.empty()) opt.title = t.title;

    if (o.pages == PdfPages::Single) {
        int w = 0, h = 0;
        HBITMAP c = Image_BuildComposite(t, &w, &h);
        if (c) pages.push_back(c);
    }
    else {
        for (auto& s : t.snippets) {
            if (!s->visible || s->w <= 0 || s->h <= 0) continue;
            HBITMAP b = Image_RenderSnippet(*s);
            if (b) pages.push_back(b);
        }
    }

    if (pages.empty()) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    else {
        ok = Pdf_WriteBitmaps(pages, opt, path);
        if (ok && pageCount) *pageCount = (int)pages.size();
    }

    // Fehlercode vor dem Aufraeumen sichern: DeleteObject setzt ihn zurueck.
    DWORD err = ok ? 0 : GetLastError();
    for (HBITMAP b : pages) DeleteObject(b);
    if (!ok) SetLastError(err);
    return ok;
}
