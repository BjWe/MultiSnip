#include "Model.h"
#include "Image.h"
#include "Lang.h"
#include <shlobj.h>
#include <objbase.h>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <map>

AppData g_app;
bool g_suppressSaveErrors = false;

// ---------------------------------------------------------------- Snippet/Tab

Snippet::~Snippet() { Unload(); }

void Snippet::SetBitmap(HBITMAP b, int width, int height)
{
    if (bmp && bmp != b) DeleteObject(bmp);
    bmp = b; w = width; h = height;
    loadFailed = false;
    FreeCache();
}

void Snippet::FreeCache()
{
    if (cache) DeleteObject(cache);
    cache = nullptr; cacheZoom = 0; cacheW = cacheH = 0; cacheCrop = RECT{};
}

void Snippet::Unload()
{
    if (bmp) DeleteObject(bmp);
    bmp = nullptr;
    FreeCache();
}

std::wstring Tab::Dir() const { return g_app.root + L"\\" + guid; }
std::wstring Tab::IniPath() const { return Dir() + L"\\tab.ini"; }
int Tab::VisibleCount() const
{
    int n = 0;
    for (auto& s : snippets) if (s->visible) n++;
    return n;
}

// ---------------------------------------------------------------- Helfer

std::wstring NewGuid()
{
    GUID g{};
    CoCreateGuid(&g);
    wchar_t buf[64]{};
    StringFromGUID2(g, buf, 64);
    std::wstring s(buf);                // {XXXXXXXX-...}
    if (s.size() > 2) s = s.substr(1, s.size() - 2);
    return s;
}

void ShowSaveError(const std::wstring& what, DWORD err)
{
    if (g_suppressSaveErrors) return;
    std::wstring msg = TRF(S_SAVE_ERR_FMT, what.c_str(), (unsigned)err, g_app.root.c_str());
    MessageBoxW(NULL, msg.c_str(), TR(S_SAVE_ERR_TITLE),
        MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

static COLORREF ParseColor(const std::wstring& s, COLORREF def)
{
    if (s.size() != 6) return def;
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(s.c_str(), &end, 16);
    if (!end || *end) return def;
    return RGB((v >> 16) & 255, (v >> 8) & 255, v & 255);
}
static std::wstring ColorStr(COLORREF c)
{
    wchar_t b[16];
    swprintf_s(b, L"%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return b;
}
static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static std::wstring Lower(std::wstring s) { for (auto& c : s) c = towlower(c); return s; }
static void Trim(std::wstring& s)
{
    size_t b = 0; while (b < s.size() && iswspace(s[b])) b++;
    size_t e = s.size(); while (e > b && iswspace(s[e - 1])) e--;
    s = s.substr(b, e - b);
}

// Eigener INI-Leser: die ganze Datei wird einmal gelesen und geparst. Die
// Profile-API parst die Datei bei JEDEM Aufruf neu und begrenzt Werte auf die
// Puffergroesse; beides war bei vielen Snippets/Formen untragbar.
class IniFile {
    std::map<std::wstring, std::map<std::wstring, std::wstring>> secs;   // Schluessel kleingeschrieben
public:
    bool Load(const std::wstring& path)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz{};
        GetFileSizeEx(h, &sz);
        if (sz.QuadPart > 64 * 1024 * 1024) { CloseHandle(h); return false; }
        std::vector<unsigned char> raw((size_t)sz.QuadPart);
        DWORD rd = 0;
        if (!raw.empty() && !ReadFile(h, raw.data(), (DWORD)raw.size(), &rd, NULL)) { CloseHandle(h); return false; }
        CloseHandle(h);
        raw.resize(rd);
        std::wstring text;
        if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
            text.assign((const wchar_t*)(raw.data() + 2), (raw.size() - 2) / 2);
        }
        else {
            UINT cp = CP_ACP;
            size_t off = 0;
            if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) { cp = CP_UTF8; off = 3; }
            int n = MultiByteToWideChar(cp, 0, (const char*)raw.data() + off, (int)(raw.size() - off), NULL, 0);
            text.resize(n);
            if (n) MultiByteToWideChar(cp, 0, (const char*)raw.data() + off, (int)(raw.size() - off), &text[0], n);
        }
        Parse(text);
        return true;
    }
    void Parse(const std::wstring& text)
    {
        std::wstring cur;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t end = text.find(L'\n', pos);
            if (end == std::wstring::npos) end = text.size();
            std::wstring line = text.substr(pos, end - pos);
            pos = end + 1;
            Trim(line);
            if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
            if (line[0] == L'[') {
                size_t r = line.find(L']');
                cur = Lower(line.substr(1, r == std::wstring::npos ? std::wstring::npos : r - 1));
                Trim(cur);
                continue;
            }
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring key = line.substr(0, eq), val = line.substr(eq + 1);
            Trim(key); Trim(val);
            secs[cur][Lower(key)] = val;
        }
    }
    std::wstring Get(const wchar_t* sec, const wchar_t* key, const std::wstring& def) const
    {
        auto s = secs.find(Lower(sec));
        if (s == secs.end()) return def;
        auto k = s->second.find(Lower(key));
        return k == s->second.end() ? def : k->second;
    }
    int GetInt(const wchar_t* sec, const wchar_t* key, int def) const
    {
        std::wstring v = Get(sec, key, L"");
        if (v.empty()) return def;
        wchar_t* end = nullptr;
        long n = wcstol(v.c_str(), &end, 10);
        return (end && end != v.c_str()) ? (int)n : def;
    }
};

// Atomarer INI-Schreiber: Inhalt wird im Speicher aufgebaut, in einem Zug als
// UTF-16 (BOM) in .tmp geschrieben und dann umbenannt. Abschnitte behalten die
// Reihenfolge des ersten Set-Aufrufs.
class IniWriter {
    std::wstring tmp, dst;
    std::vector<std::pair<std::wstring, std::wstring>> sections;   // Name, Inhalt (Zeilen)
    DWORD err = 0;
    std::wstring* Section(const wchar_t* sec)
    {
        for (auto& s : sections) if (s.first == sec) return &s.second;
        sections.push_back({ sec, L"" });
        return &sections.back().second;
    }
public:
    explicit IniWriter(const std::wstring& path) : tmp(path + L".tmp"), dst(path) {}
    void Set(const wchar_t* sec, const wchar_t* key, const std::wstring& val)
    {
        std::wstring v = val;
        for (auto& c : v) if (c == L'\r' || c == L'\n') c = L' ';   // Zeilenumbrueche wuerden das Format brechen
        std::wstring* s = Section(sec);
        *s += key; *s += L'='; *s += v; *s += L"\r\n";
    }
    void SetInt(const wchar_t* sec, const wchar_t* key, int v) { Set(sec, key, std::to_wstring(v)); }
    bool Commit()
    {
        std::wstring out;
        for (auto& s : sections) { out += L"["; out += s.first; out += L"]\r\n"; out += s.second; out += L"\r\n"; }
        HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { err = GetLastError(); return false; }
        unsigned char bom[2] = { 0xFF, 0xFE };
        DWORD wr = 0;
        bool ok = WriteFile(h, bom, 2, &wr, NULL) != FALSE;
        if (ok && !out.empty()) ok = WriteFile(h, out.data(), (DWORD)(out.size() * sizeof(wchar_t)), &wr, NULL) != FALSE;
        if (ok) ok = FlushFileBuffers(h) != FALSE;
        if (!ok) err = GetLastError();
        CloseHandle(h);
        if (ok && !MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            ok = false; err = GetLastError();
        }
        if (!ok) DeleteFileW(tmp.c_str());
        return ok;
    }
    DWORD Error() const { return err; }
};

bool Model_HotkeyValid(UINT mod, UINT vk)
{
    if (vk == 0 || vk > 0xFE) return false;
    bool special = (vk >= VK_F1 && vk <= VK_F24) || vk == VK_SNAPSHOT;
    if (mod == 0 && !special) return false;
    if (mod == MOD_ALT && (vk == VK_F4 || vk == VK_TAB || vk == VK_ESCAPE)) return false;   // Systemkombinationen
    if (vk == VK_LWIN || vk == VK_RWIN || vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU) return false;
    return true;
}

// ---------------------------------------------------------------- Init/Load

static std::wstring KnownFolder(int csidl)
{
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path))) return L"";
    return path;
}

bool Model_Init()
{
    // Bilddaten gehoeren nach %LocalAppData%: Roaming-Profile und Sync-Clients
    // wuerden sonst alle Screenshots bei jeder Anmeldung uebertragen.
    std::wstring local = KnownFolder(CSIDL_LOCAL_APPDATA);
    std::wstring roaming = KnownFolder(CSIDL_APPDATA);
    if (local.empty() && roaming.empty()) return false;
    std::wstring newRoot = local.empty() ? L"" : local + L"\\MultiSnip";
    std::wstring oldRoot = roaming.empty() ? L"" : roaming + L"\\MultiSnip";
    auto isDir = [](const std::wstring& p) {
        DWORD a = p.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    };
    if (!newRoot.empty() && !isDir(newRoot) && isDir(oldRoot)) {
        // Migration: alten Ordner verschieben (gleiches Laufwerk: reines Umbenennen).
        if (!MoveFileExW(oldRoot.c_str(), newRoot.c_str(), 0)) {
            g_app.root = oldRoot;               // andernfalls weiter am alten Ort arbeiten
            return true;
        }
    }
    g_app.root = newRoot.empty() ? oldRoot : newRoot;
    CreateDirectoryW(g_app.root.c_str(), NULL);
    return isDir(g_app.root);
}

static std::wstring GlobalIni() { return g_app.root + L"\\MultiSnip.ini"; }

static void LoadGlobal()
{
    IniFile f;
    f.Load(GlobalIni());
    Defaults d; // fest eincodierte Notfall-Vorgaben
    Defaults& o = g_app.def;
    o.fill = ParseColor(f.Get(L"Defaults", L"FillColor", ColorStr(d.fill)), d.fill);
    o.padX = Clamp(f.GetInt(L"Defaults", L"PadX", d.padX), 0, 500);
    o.padY = Clamp(f.GetInt(L"Defaults", L"PadY", d.padY), 0, 500);
    o.delayMs = Clamp(f.GetInt(L"Defaults", L"DelayMs", d.delayMs), 0, 30000);
    o.autoCopy = Clamp(f.GetInt(L"Defaults", L"AutoCopy", d.autoCopy), 0, 2);
    o.numberScope = Clamp(f.GetInt(L"Defaults", L"NumberScope", d.numberScope), 0, 1);
    o.captureCursor = f.GetInt(L"Defaults", L"CaptureCursor", d.captureCursor) != 0;
    o.hotMod = (UINT)f.GetInt(L"Defaults", L"HotkeyMod", (int)d.hotMod) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
    o.hotVk = (UINT)f.GetInt(L"Defaults", L"HotkeyKey", (int)d.hotVk);
    if (!Model_HotkeyValid(o.hotMod, o.hotVk)) { o.hotMod = d.hotMod; o.hotVk = d.hotVk; }
    o.dashboard = f.GetInt(L"Defaults", L"Dashboard", d.dashboard) != 0;
    o.noAskDelete = f.GetInt(L"Defaults", L"NoAskDelete", d.noAskDelete) != 0;
    o.language = Clamp(f.GetInt(L"Defaults", L"Language", 0), 0, 3);
    Lang_Set((Lang)o.language);
    o.theme = Clamp(f.GetInt(L"Defaults", L"Theme", 0), 0, 2);

    WinState& s = g_app.st;
    s.lastTab = f.Get(L"State", L"LastTab", L"");
    s.x = f.GetInt(L"State", L"WinX", 0);
    s.y = f.GetInt(L"State", L"WinY", 0);
    s.w = f.GetInt(L"State", L"WinW", 0);
    s.h = f.GetInt(L"State", L"WinH", 0);
    s.maximized = f.GetInt(L"State", L"Maximized", 0) != 0;
    s.dpi = Clamp(f.GetInt(L"State", L"WinDpi", 0), 0, 1000);
}

static std::unique_ptr<Tab> LoadTab(const std::wstring& guid)
{
    auto t = std::make_unique<Tab>();
    t->guid = guid;
    IniFile f;
    if (!f.Load(t->IniPath())) return nullptr;

    t->title = f.Get(L"Tab", L"Title", L"Tab");
    t->order = f.GetInt(L"Tab", L"Order", 0);
    t->fill = ParseColor(f.Get(L"Tab", L"FillColor", L""), g_app.def.fill);
    t->padX = Clamp(f.GetInt(L"Tab", L"PadX", g_app.def.padX), 0, 500);
    t->padY = Clamp(f.GetInt(L"Tab", L"PadY", g_app.def.padY), 0, 500);
    t->delayMs = Clamp(f.GetInt(L"Tab", L"DelayMs", g_app.def.delayMs), 0, 30000);
    t->autoCopy = Clamp(f.GetInt(L"Tab", L"AutoCopy", g_app.def.autoCopy), 0, 2);
    t->numberScope = Clamp(f.GetInt(L"Tab", L"NumberScope", g_app.def.numberScope), 0, 1);
    t->scroll = std::max(0, f.GetInt(L"Tab", L"Scroll", 0));

    int count = Clamp(f.GetInt(L"Snippets", L"Count", 0), 0, 100000);
    struct Entry { int pos; std::unique_ptr<Snippet> s; };
    std::vector<Entry> entries;
    for (int i = 0; i < count; i++) {
        wchar_t sec[32];
        swprintf_s(sec, L"Snippet%d", i);
        std::wstring file = f.Get(sec, L"File", L"");
        if (file.empty() || file.find_first_of(L"\\/") != std::wstring::npos || file == L"." || file == L"..") continue;
        auto s = std::make_unique<Snippet>();
        s->file = file;
        s->visible = f.GetInt(sec, L"Visible", 1) != 0;
        int pos = f.GetInt(sec, L"Position", i);
        // Nur die Groesse aus dem PNG-Header lesen; die Bitmap wird erst beim
        // Aktivieren des Tabs geladen. Ein Snippet, dessen Datei gerade nicht
        // lesbar ist, bleibt erhalten und wird spaeter erneut versucht.
        int w = 0, h = 0;
        if (Image_PngSize(t->Dir() + L"\\" + file, &w, &h)) { s->w = w; s->h = h; }
        else s->loadFailed = true;
        // Zuschnitt "l,t,r,b"; wird auf die Bildgroesse begrenzt, Unsinn wird verworfen
        std::wstring cs = f.Get(sec, L"Crop", L"");
        if (!cs.empty()) {
            int v[4]{}; int n = 0; size_t cp = 0;
            while (n < 4 && cp <= cs.size()) {
                size_t e = cs.find(L',', cp); if (e == std::wstring::npos) e = cs.size();
                v[n++] = _wtoi(cs.substr(cp, e - cp).c_str()); cp = e + 1;
            }
            RECT c{ v[0], v[1], v[2], v[3] };
            if (s->w > 0) { c.left = Clamp(c.left, 0, s->w); c.right = Clamp(c.right, 0, s->w); c.top = Clamp(c.top, 0, s->h); c.bottom = Clamp(c.bottom, 0, s->h); }
            if (n == 4 && c.right - c.left >= 4 && c.bottom - c.top >= 4 && !(c.left == 0 && c.top == 0 && c.right == s->w && c.bottom == s->h)) s->crop = c;
        }
        Model_LoadAnnot(*t, *s);
        entries.push_back({ pos, std::move(s) });
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.pos < b.pos; });
    for (auto& e : entries) t->snippets.push_back(std::move(e.s));
    return t;
}

void Model_LoadTabBitmaps(Tab& t)
{
    for (auto& s : t.snippets) {
        if (s->bmp) continue;
        int w = 0, h = 0;
        HBITMAP b = Image_LoadPng(t.Dir() + L"\\" + s->file, &w, &h);
        if (b) s->SetBitmap(b, w, h);
        else s->loadFailed = true;
    }
    t.loaded = true;
}

void Model_UnloadTabBitmaps(Tab& t)
{
    for (auto& s : t.snippets) if (!s->unsaved) s->Unload();   // ungesicherte Bitmaps nie wegwerfen
    t.loaded = false;
}

void Model_LoadAll()
{
    LoadGlobal();
    g_app.tabs.clear();
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((g_app.root + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fd.cFileName[0] == L'.') continue;
            std::wstring name = fd.cFileName;
            if (name.size() != 36) continue;    // GUID ohne Klammern
            try {
                auto t = LoadTab(name);
                if (t) g_app.tabs.push_back(std::move(t));
            }
            catch (...) { /* ein kaputter Tab reisst die anderen nicht mit */ }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::stable_sort(g_app.tabs.begin(), g_app.tabs.end(),
        [](const std::unique_ptr<Tab>& a, const std::unique_ptr<Tab>& b) {
            if (a->order != b->order) return a->order < b->order;
            return a->title < b->title;
        });
    for (size_t i = 0; i < g_app.tabs.size(); i++) g_app.tabs[i]->order = (int)i;
}

// ---------------------------------------------------------------- Save

bool Model_SaveGlobal()
{
    IniWriter w(GlobalIni());
    const Defaults& d = g_app.def;
    w.Set(L"Defaults", L"FillColor", ColorStr(d.fill));
    w.SetInt(L"Defaults", L"PadX", d.padX);
    w.SetInt(L"Defaults", L"PadY", d.padY);
    w.SetInt(L"Defaults", L"DelayMs", d.delayMs);
    w.SetInt(L"Defaults", L"AutoCopy", d.autoCopy);
    w.SetInt(L"Defaults", L"NumberScope", d.numberScope);
    w.SetInt(L"Defaults", L"CaptureCursor", d.captureCursor ? 1 : 0);
    w.SetInt(L"Defaults", L"HotkeyMod", (int)d.hotMod);
    w.SetInt(L"Defaults", L"HotkeyKey", (int)d.hotVk);
    w.SetInt(L"Defaults", L"Dashboard", d.dashboard ? 1 : 0);
    w.SetInt(L"Defaults", L"NoAskDelete", d.noAskDelete ? 1 : 0);
    w.SetInt(L"Defaults", L"Language", d.language);
    w.SetInt(L"Defaults", L"Theme", d.theme);
    const WinState& s = g_app.st;
    w.Set(L"State", L"LastTab", s.lastTab);
    w.SetInt(L"State", L"WinX", s.x);
    w.SetInt(L"State", L"WinY", s.y);
    w.SetInt(L"State", L"WinW", s.w);
    w.SetInt(L"State", L"WinH", s.h);
    w.SetInt(L"State", L"Maximized", s.maximized ? 1 : 0);
    w.SetInt(L"State", L"WinDpi", s.dpi);
    if (!w.Commit()) { ShowSaveError(GlobalIni(), w.Error()); return false; }
    return true;
}

bool Model_SaveTab(const Tab& t)
{
    // Frueher fehlgeschlagene PNG-Schreibvorgaenge still erneut versuchen (z. B. Platte war voll)
    for (auto& s : t.snippets)
        if (s->unsaved && s->bmp && Image_SavePng(s->bmp, t.Dir() + L"\\" + s->file)) s->unsaved = false;
    IniWriter w(t.IniPath());
    w.Set(L"Tab", L"Title", t.title);
    w.SetInt(L"Tab", L"Order", t.order);
    w.Set(L"Tab", L"FillColor", ColorStr(t.fill));
    w.SetInt(L"Tab", L"PadX", t.padX);
    w.SetInt(L"Tab", L"PadY", t.padY);
    w.SetInt(L"Tab", L"DelayMs", t.delayMs);
    w.SetInt(L"Tab", L"AutoCopy", t.autoCopy);
    w.SetInt(L"Tab", L"NumberScope", t.numberScope);
    w.SetInt(L"Tab", L"Scroll", t.scroll);
    w.SetInt(L"Snippets", L"Count", (int)t.snippets.size());
    for (size_t i = 0; i < t.snippets.size(); i++) {
        wchar_t sec[32];
        swprintf_s(sec, L"Snippet%d", (int)i);
        w.Set(sec, L"File", t.snippets[i]->file);
        w.SetInt(sec, L"Visible", t.snippets[i]->visible ? 1 : 0);
        w.SetInt(sec, L"Position", (int)i);     // aus dem Listenindex abgeleitet
        const Snippet& s = *t.snippets[i];
        if (s.HasCrop()) {
            wchar_t c[64];
            swprintf_s(c, L"%d,%d,%d,%d", (int)s.crop.left, (int)s.crop.top, (int)s.crop.right, (int)s.crop.bottom);
            w.Set(sec, L"Crop", c);
        }
    }
    if (!w.Commit()) { ShowSaveError(t.IniPath(), w.Error()); return false; }
    return true;
}

Tab* Model_NewTab()
{
    auto t = std::make_unique<Tab>();
    t->guid = NewGuid();
    if (!CreateDirectoryW(t->Dir().c_str(), NULL)) {
        ShowSaveError(t->Dir(), GetLastError());
        return nullptr;
    }
    int maxOrder = -1;
    for (auto& x : g_app.tabs) maxOrder = std::max(maxOrder, x->order);
    t->order = maxOrder + 1;
    // Titel "Tab N" mit der kleinsten noch nicht vergebenen Nummer
    for (int n = 1; ; n++) {
        std::wstring cand = TRF(S_TAB_N_FMT, n);
        bool used = false;
        for (auto& x : g_app.tabs) if (x->title == cand) { used = true; break; }
        if (!used) { t->title = cand; break; }
    }
    t->fill = g_app.def.fill;
    t->padX = g_app.def.padX;
    t->padY = g_app.def.padY;
    t->delayMs = g_app.def.delayMs;
    t->autoCopy = g_app.def.autoCopy;
    t->numberScope = g_app.def.numberScope;
    t->loaded = true;                           // leer, also vollstaendig "geladen"
    Tab* raw = t.get();
    g_app.tabs.push_back(std::move(t));
    Model_SaveTab(*raw);
    return raw;
}

void Model_DeleteTab(size_t idx)
{
    if (idx >= g_app.tabs.size()) return;
    std::wstring dir = g_app.tabs[idx]->Dir();
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            DeleteFileW((dir + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
    g_app.tabs.erase(g_app.tabs.begin() + idx);   // Destruktoren geben Bitmaps frei
    for (size_t i = 0; i < g_app.tabs.size(); i++) {
        if (g_app.tabs[i]->order != (int)i) { g_app.tabs[i]->order = (int)i; Model_SaveTab(*g_app.tabs[i]); }
    }
}

bool Model_SaveSnippetPng(const Tab& t, const Snippet& s)
{
    std::wstring path = t.Dir() + L"\\" + s.file;
    if (!Image_SavePng(s.bmp, path)) { ShowSaveError(path, GetLastError()); return false; }
    return true;
}

void Model_DeleteSnippet(Tab& t, size_t idx)
{
    if (idx >= t.snippets.size()) return;
    DeleteFileW((t.Dir() + L"\\" + t.snippets[idx]->file).c_str());
    DeleteFileW((t.Dir() + L"\\" + t.snippets[idx]->file + L".ann.ini").c_str());
    t.snippets.erase(t.snippets.begin() + idx);   // Destruktor gibt Bitmap frei
}

std::wstring Model_NewSnippetFile(const Tab& t)
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    for (int n = 0; n < 1000; n++) {
        wchar_t name[64];
        if (n == 0) swprintf_s(name, L"snip_%04d%02d%02d_%02d%02d%02d_%03d.png",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        else swprintf_s(name, L"snip_%04d%02d%02d_%02d%02d%02d_%03d_%d.png",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, n);
        if (GetFileAttributesW((t.Dir() + L"\\" + name).c_str()) == INVALID_FILE_ATTRIBUTES) return name;
    }
    return L"snip_" + NewGuid() + L".png";
}

// ---------------------------------------------------------------- Annotationen
// Datei: <png>.ann.ini
//   [Layers] Count=n
//   [LayerI] Name, Visible, Count
//   [LayerI.ShapeJ] Type, Color, Width, Number, Text, Points=x,y;x,y;...

static std::wstring AnnPath(const Tab& t, const Snippet& s) { return t.Dir() + L"\\" + s.file + L".ann.ini"; }

static std::wstring PointsStr(const std::vector<AnnPoint>& pts)
{
    std::wstring out;
    out.reserve(pts.size() * 14);
    wchar_t b[48];
    for (size_t i = 0; i < pts.size(); i++) {
        swprintf_s(b, L"%s%.1f,%.1f", i ? L";" : L"", pts[i].x, pts[i].y);
        out += b;
    }
    return out;
}

static std::vector<AnnPoint> ParsePoints(const std::wstring& s)
{
    std::vector<AnnPoint> pts;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t end = s.find(L';', pos);
        if (end == std::wstring::npos) end = s.size();
        std::wstring pair = s.substr(pos, end - pos);
        size_t comma = pair.find(L',');
        if (comma != std::wstring::npos) {
            AnnPoint p;
            p.x = (float)_wtof(pair.substr(0, comma).c_str());
            p.y = (float)_wtof(pair.substr(comma + 1).c_str());
            pts.push_back(p);
        }
        pos = end + 1;
    }
    return pts;
}

void Model_LoadAnnot(const Tab& t, Snippet& s)
{
    s.layers.clear();
    IniFile f;
    if (!f.Load(AnnPath(t, s))) return;
    int layerCount = Clamp(f.GetInt(L"Layers", L"Count", 0), 0, 1000);
    for (int i = 0; i < layerCount; i++) {
        wchar_t sec[48];
        swprintf_s(sec, L"Layer%d", i);
        Layer L;
        L.name = f.Get(sec, L"Name", TRF(S_LAYER_N_FMT, i + 1));
        L.visible = f.GetInt(sec, L"Visible", 1) != 0;
        int n = Clamp(f.GetInt(sec, L"Count", 0), 0, 100000);
        for (int j = 0; j < n; j++) {
            wchar_t ssec[64];
            swprintf_s(ssec, L"Layer%d.Shape%d", i, j);
            Shape sh;
            int type = f.GetInt(ssec, L"Type", 0);
            if (type < 0 || type > (int)ShapeType::Number) continue;
            sh.type = (ShapeType)type;
            sh.color = ParseColor(f.Get(ssec, L"Color", L"E81123"), RGB(232, 17, 35));
            sh.width = Clamp(f.GetInt(ssec, L"Width", 3), 1, 64);
            sh.number = f.GetInt(ssec, L"Number", 0);
            sh.text = f.Get(ssec, L"Text", L"");
            sh.pts = ParsePoints(f.Get(ssec, L"Points", L""));
            if (sh.pts.empty()) continue;
            L.shapes.push_back(std::move(sh));
        }
        s.layers.push_back(std::move(L));
    }
}

bool Model_SaveAnnot(const Tab& t, const Snippet& s)
{
    std::wstring f = AnnPath(t, s);
    bool any = false;
    for (auto& L : s.layers) if (!L.shapes.empty() || s.layers.size() > 1) { any = true; break; }
    if (!any) { DeleteFileW(f.c_str()); return true; }
    IniWriter w(f);
    w.SetInt(L"Layers", L"Count", (int)s.layers.size());
    for (size_t i = 0; i < s.layers.size(); i++) {
        const Layer& L = s.layers[i];
        wchar_t sec[48];
        swprintf_s(sec, L"Layer%d", (int)i);
        w.Set(sec, L"Name", L.name);
        w.SetInt(sec, L"Visible", L.visible ? 1 : 0);
        w.SetInt(sec, L"Count", (int)L.shapes.size());
        for (size_t j = 0; j < L.shapes.size(); j++) {
            const Shape& sh = L.shapes[j];
            wchar_t ssec[64];
            swprintf_s(ssec, L"Layer%d.Shape%d", (int)i, (int)j);
            w.SetInt(ssec, L"Type", (int)sh.type);
            w.Set(ssec, L"Color", ColorStr(sh.color));
            w.SetInt(ssec, L"Width", sh.width);
            w.SetInt(ssec, L"Number", sh.number);
            w.Set(ssec, L"Text", sh.text);
            w.Set(ssec, L"Points", PointsStr(sh.pts));
        }
    }
    if (!w.Commit()) { ShowSaveError(f, w.Error()); return false; }
    return true;
}
