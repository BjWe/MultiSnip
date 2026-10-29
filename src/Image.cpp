#include "Image.h"
#include "Util.h"
#include <algorithm>
#include <objidl.h>
// gdiplus-Header verwenden min/max als Makros; mit NOMINMAX brauchen sie die std-Varianten.
using std::min;
using std::max;
#include <gdiplus.h>

static ULONG_PTR g_gdipToken = 0;
static CLSID g_pngClsid{};
static bool g_haveEncoder = false;

bool Image_Init()
{
    Gdiplus::GdiplusStartupInput in;
    if (Gdiplus::GdiplusStartup(&g_gdipToken, &in, NULL) != Gdiplus::Ok) return false;
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    Gdiplus::ImageCodecInfo* ici = (Gdiplus::ImageCodecInfo*)buf.data();
    Gdiplus::GetImageEncoders(num, size, ici);
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(ici[i].MimeType, L"image/png") == 0) { g_pngClsid = ici[i].Clsid; g_haveEncoder = true; break; }
    }
    return g_haveEncoder;
}

void Image_Shutdown()
{
    if (g_gdipToken) Gdiplus::GdiplusShutdown(g_gdipToken);
    g_gdipToken = 0;
}

HBITMAP Image_CreateDib(int w, int h, void** bits)
{
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* p = nullptr;
    HBITMAP b = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &p, NULL, 0);
    if (bits) *bits = p;
    return b;
}

HBITMAP Image_LoadPng(const std::wstring& path, int* w, int* h)
{
    Gdiplus::Bitmap bm(path.c_str());
    if (bm.GetLastStatus() != Gdiplus::Ok) return nullptr;
    int W = (int)bm.GetWidth(), H = (int)bm.GetHeight();
    if (W <= 0 || H <= 0) return nullptr;
    HBITMAP dib = Image_CreateDib(W, H);
    if (!dib) return nullptr;
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ old = SelectObject(dc, dib);
    RECT r{ 0, 0, W, H };
    FillSolid(dc, r, RGB(255, 255, 255));
    {
        Gdiplus::Graphics g(dc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(&bm, 0, 0, W, H);
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    if (w) *w = W;
    if (h) *h = H;
    return dib;
}

bool Image_PngSize(const std::wstring& path, int* w, int* h)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    unsigned char hdr[24]{};
    DWORD rd = 0;
    bool ok = ReadFile(f, hdr, 24, &rd, NULL) && rd == 24;
    CloseHandle(f);
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (!ok || memcmp(hdr, sig, 8) != 0 || memcmp(hdr + 12, "IHDR", 4) != 0) return false;
    auto be = [&](int o) { return ((unsigned)hdr[o] << 24) | ((unsigned)hdr[o + 1] << 16) | ((unsigned)hdr[o + 2] << 8) | hdr[o + 3]; };
    unsigned W = be(16), H = be(20);
    if (W == 0 || H == 0 || W > 0x7FFF0000u || H > 0x7FFF0000u) return false;
    if (w) *w = (int)W;
    if (h) *h = (int)H;
    return true;
}

bool Image_SavePng(HBITMAP bmp, const std::wstring& path)
{
    if (!bmp || !g_haveEncoder) return false;
    Gdiplus::Bitmap* bm = Gdiplus::Bitmap::FromHBITMAP(bmp, NULL);
    if (!bm) return false;
    std::wstring tmp = path + L".tmp";
    Gdiplus::Status st = bm->Save(tmp.c_str(), &g_pngClsid, NULL);
    delete bm;
    if (st != Gdiplus::Ok) { DeleteFileW(tmp.c_str()); SetLastError(ERROR_WRITE_FAULT); return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD e = GetLastError();
        DeleteFileW(tmp.c_str());
        SetLastError(e);
        return false;
    }
    return true;
}

// Gesamthoehe = Summe der Snippet-Hoehen + vertikales Padding x Anzahl Luecken;
// Gesamtbreite = breitestes Snippet + horizontales Padding als Rand (beidseitig).
Layout Image_ComputeLayout(const Tab& t, bool includeHidden)
{
    Layout L;
    int maxW = 0;
    for (auto& s : t.snippets) {
        if (!s->visible && !includeHidden) continue;
        if (s->w <= 0 || s->h <= 0) continue;           // Groesse unbekannt (Datei nicht lesbar)
        maxW = std::max(maxW, s->VW());
    }
    int y = 0;
    for (auto& s : t.snippets) {
        if (!s->visible && !includeHidden) continue;
        if (s->w <= 0 || s->h <= 0) continue;
        if (!L.items.empty()) y += t.padY;
        L.items.push_back({ s.get(), t.padX, y });
        y += s->VH();
    }
    L.width = L.items.empty() ? 0 : maxW + 2 * t.padX;
    L.height = y;
    return L;
}

// BitBlt/ExtTextOut lassen den Alphakanal auf 0. Manche Programme deuten ein
// 32-bpp-CF_DIB dann als komplett transparent; deshalb Alpha auf 255 setzen.
void Image_FixAlpha(HBITMAP dib)
{
    GdiFlush();
    DIBSECTION ds{};
    if (GetObjectW(dib, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits && ds.dsBm.bmBitsPixel == 32) {
        BYTE* p = (BYTE*)ds.dsBm.bmBits;
        size_t n = (size_t)ds.dsBm.bmWidthBytes * (size_t)ds.dsBm.bmHeight;
        for (size_t i = 3; i < n; i += 4) p[i] = 0xFF;
    }
}

HBITMAP Image_RenderSnippet(const Snippet& s, bool withAnnotations)
{
    if (!s.bmp || s.w <= 0 || s.h <= 0) return nullptr;
    RECT v = s.View();
    HBITMAP dib = Image_CreateDib(s.VW(), s.VH());
    if (!dib) return nullptr;
    HDC dc = CreateCompatibleDC(NULL), src = CreateCompatibleDC(NULL);
    HGDIOBJ old = SelectObject(dc, dib), o = SelectObject(src, s.bmp);
    BitBlt(dc, 0, 0, s.VW(), s.VH(), src, v.left, v.top, SRCCOPY);
    SelectObject(src, o);
    if (withAnnotations) Annot_RenderView(dc, s, 1.0, 0, 0, v);
    SelectObject(dc, old);
    DeleteDC(src);
    DeleteDC(dc);
    Image_FixAlpha(dib);
    return dib;
}

HBITMAP Image_BuildComposite(const Tab& t, int* w, int* h)
{
    Layout L = Image_ComputeLayout(t, false);
    if (L.items.empty() || L.width <= 0 || L.height <= 0) return nullptr;
    HBITMAP dib = Image_CreateDib(L.width, L.height);
    if (!dib) return nullptr;
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ old = SelectObject(dc, dib);
    RECT all{ 0, 0, L.width, L.height };
    FillSolid(dc, all, t.fill);
    HDC src = CreateCompatibleDC(NULL);
    for (auto& it : L.items) {
        if (!it.s->bmp) continue;
        RECT v = it.s->View();
        HGDIOBJ o = SelectObject(src, it.s->bmp);
        BitBlt(dc, it.x, it.y, it.s->VW(), it.s->VH(), src, v.left, v.top, SRCCOPY);
        SelectObject(src, o);
        Annot_RenderView(dc, *it.s, 1.0, it.x, it.y, v);     // sichtbare Ebenen in Originalaufloesung, auf den Zuschnitt begrenzt
    }
    DeleteDC(src);
    SelectObject(dc, old);
    DeleteDC(dc);
    Image_FixAlpha(dib);
    if (w) *w = L.width;
    if (h) *h = L.height;
    return dib;
}

static HGLOBAL MakePngGlobal(HBITMAP bmp)
{
    if (!g_haveEncoder) return nullptr;
    Gdiplus::Bitmap* bm = Gdiplus::Bitmap::FromHBITMAP(bmp, NULL);
    if (!bm) return nullptr;
    IStream* st = nullptr;
    HGLOBAL result = nullptr;
    if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &st))) {
        if (bm->Save(st, &g_pngClsid, NULL) == Gdiplus::Ok) {
            STATSTG stat{};
            if (SUCCEEDED(st->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0 && stat.cbSize.QuadPart < 0x7FFFFFFF) {
                SIZE_T n = (SIZE_T)stat.cbSize.QuadPart;
                result = GlobalAlloc(GMEM_MOVEABLE, n);
                if (result) {
                    void* dst = GlobalLock(result);
                    LARGE_INTEGER zero{};
                    st->Seek(zero, STREAM_SEEK_SET, NULL);
                    ULONG rd = 0;
                    st->Read(dst, (ULONG)n, &rd);
                    GlobalUnlock(result);
                    if (rd != n) { GlobalFree(result); result = nullptr; }
                }
            }
        }
        st->Release();
    }
    delete bm;
    return result;
}

bool Image_CopyToClipboard(HWND owner, HBITMAP bmp, int w, int h)
{
    if (!bmp || w <= 0 || h <= 0) return false;
    // CF_DIB (32bpp, bottom-up)
    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(bih);
    bih.biWidth = w;
    bih.biHeight = h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    SIZE_T imgBytes = (SIZE_T)w * h * 4;
    bih.biSizeImage = (DWORD)imgBytes;
    HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, sizeof(bih) + imgBytes);
    if (!hDib) return false;
    {
        BYTE* p = (BYTE*)GlobalLock(hDib);
        memcpy(p, &bih, sizeof(bih));
        BITMAPINFO bi{};
        bi.bmiHeader = bih;
        HDC dc = GetDC(NULL);
        GetDIBits(dc, bmp, 0, h, p + sizeof(bih), &bi, DIB_RGB_COLORS);
        ReleaseDC(NULL, dc);
        GlobalUnlock(hDib);
    }
    HGLOBAL hPng = MakePngGlobal(bmp);

    bool opened = false;
    for (int i = 0; i < 10 && !opened; i++) {
        opened = OpenClipboard(owner) != FALSE;
        if (!opened) Sleep(20);
    }
    if (!opened) { GlobalFree(hDib); if (hPng) GlobalFree(hPng); return false; }
    EmptyClipboard();
    bool ok = SetClipboardData(CF_DIB, hDib) != NULL;
    if (!ok) GlobalFree(hDib);
    if (hPng) {
        UINT fmt = RegisterClipboardFormatW(L"PNG");
        if (!fmt || !SetClipboardData(fmt, hPng)) GlobalFree(hPng);
    }
    CloseClipboard();
    return ok;
}
