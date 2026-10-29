#include "Capture.h"
#include "Image.h"
#include "Util.h"
#include "Lang.h"
#include <vector>
#include <algorithm>

namespace {

struct Mon {
    HWND hwnd = nullptr;
    RECT rc{};                  // virtuelle Bildschirmkoordinaten (physische Pixel)
    HBITMAP bmp = nullptr;      // eingefrorener Inhalt
    HDC dc = nullptr;           // bmp dauerhaft ausgewaehlt
    HGDIOBJ old = nullptr;
    HBITMAP back = nullptr;     // Back-Buffer fuer flackerfreies Zeichnen
};

const wchar_t* kClass = L"MultiSnipOverlay";
const wchar_t* kBadgeClass = L"MultiSnipBadge";
const int kMinSize = 4;
const UINT_PTR kEscTimer = 1;           // Fenster-Timer im Overlay: pollt Esc als Fallback
const int kBadgeHideMs = 350;           // Countdown-Karte so frueh entfernen, dass sie nicht mit eingefroren wird

std::vector<Mon> g_mons;
bool g_active = false;
bool g_waiting = false;                 // Verzoegerung laeuft, noch kein Overlay
bool g_finishing = false;
bool g_selecting = false;
POINT g_start{}, g_cur{};
CaptureMode g_mode = CaptureMode::Append;
int g_replaceIdx = -1;
CaptureDoneFn g_done = nullptr;
void* g_ctx = nullptr;
HINSTANCE g_hInst = nullptr;
bool g_cursor = false;
UINT_PTR g_timer = 0;
ULONGLONG g_deadline = 0;
HWND g_badge = nullptr;
int g_badgeSecs = -1;
HFONT g_font = nullptr;
HFONT g_iconFont = nullptr;
bool g_classRegistered = false;

void CreateFonts()
{
    if (g_font) return;
    g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_iconFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        FontExists(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets");
}

Mon* FindMon(HWND h)
{
    for (auto& m : g_mons) if (m.hwnd == h) return &m;
    return nullptr;
}

bool IsOverlay(HWND h) { return FindMon(h) != nullptr; }

RECT NormRect(POINT a, POINT b)
{
    RECT r{ std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x) + 1, std::max(a.y, b.y) + 1 };
    return r;
}

void InvalidateAll()
{
    for (auto& m : g_mons) if (m.hwnd) InvalidateRect(m.hwnd, NULL, FALSE);
}

void DestroyBadge()
{
    if (g_badge) { HWND b = g_badge; g_badge = nullptr; DestroyWindow(b); }
}

// Gemeinsame Aufraeum-Routine fuer Erfolg und Abbruch.
void Cleanup()
{
    if (g_timer) { KillTimer(NULL, g_timer); g_timer = 0; }
    DestroyBadge();
    for (auto& m : g_mons) {
        HWND h = m.hwnd;
        m.hwnd = nullptr;
        if (h) DestroyWindow(h);
        if (m.dc) { SelectObject(m.dc, m.old); DeleteDC(m.dc); }
        if (m.bmp) DeleteObject(m.bmp);
        if (m.back) DeleteObject(m.back);
    }
    g_mons.clear();
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    if (g_iconFont) { DeleteObject(g_iconFont); g_iconFont = nullptr; }
    g_selecting = false;
    g_waiting = false;
}

void Finish(HBITMAP result, int w, int h)
{
    CaptureDoneFn done = g_done;
    void* ctx = g_ctx;
    CaptureMode mode = g_mode;
    int idx = g_replaceIdx;
    Cleanup();
    g_active = false;
    g_finishing = false;
    if (done) done(result, w, h, mode, idx, ctx);
}

void Cancel()
{
    if (g_finishing || !g_active) return;
    g_finishing = true;
    Finish(nullptr, 0, 0);
}

void Grab(const RECT& sel)
{
    if (g_finishing) return;
    g_finishing = true;
    int w = RW(sel), h = RH(sel);
    HBITMAP result = Image_CreateDib(w, h);
    if (!result) { Finish(nullptr, 0, 0); return; }
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ old = SelectObject(dc, result);
    RECT all{ 0, 0, w, h };
    FillSolid(dc, all, RGB(0, 0, 0));
    for (auto& m : g_mons) {
        RECT piece{};
        if (!IntersectRect(&piece, &sel, &m.rc)) continue;
        BitBlt(dc, piece.left - sel.left, piece.top - sel.top, RW(piece), RH(piece),
            m.dc, piece.left - m.rc.left, piece.top - m.rc.top, SRCCOPY);
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    Finish(result, w, h);
}

void PaintOverlay(Mon& m, HDC hdc)
{
    int w = RW(m.rc), h = RH(m.rc);
    if (!m.back) m.back = CreateCompatibleBitmap(hdc, w, h);
    HDC bdc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(bdc, m.back);

    BitBlt(bdc, 0, 0, w, h, m.dc, 0, 0, SRCCOPY);
    RECT all{ 0, 0, w, h };
    FillAlpha(bdc, all, RGB(0, 0, 0), 96);                  // Rest leicht abdunkeln

    if (g_selecting) {
        RECT sel = NormRect(g_start, g_cur);
        RECT local = sel;
        OffsetRect(&local, -m.rc.left, -m.rc.top);
        RECT clip{};
        if (IntersectRect(&clip, &local, &all)) {
            BitBlt(bdc, clip.left, clip.top, RW(clip), RH(clip), m.dc, clip.left, clip.top, SRCCOPY);
        }
        // heller Rahmen (1px dunkel aussen, 1px weiss innen)
        RECT fr = local; InflateRect(&fr, 1, 1);
        FrameRectColor(bdc, fr, RGB(40, 40, 40), 1);
        FrameRectColor(bdc, local, RGB(255, 255, 255), 1);
        // Groessenangabe
        wchar_t txt[64];
        swprintf_s(txt, L"%d x %d", RW(sel), RH(sel));
        int tw = TextWidth(bdc, g_font, txt) + 12;
        int th = 22;
        int tx = local.left, ty = local.bottom + 4;
        if (ty + th > h) ty = local.top - th - 4;
        if (ty < 0) ty = local.top + 4;
        if (tx + tw > w) tx = w - tw;
        if (tx < 0) tx = 0;
        RECT tr{ tx, ty, tx + tw, ty + th };
        DrawRoundBox(bdc, tr, RGB(30, 30, 30), RGB(255, 255, 255), 4);
        DrawTextIn(bdc, tr, txt, g_font, RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else {
        // Fluent-Hinweiskarte mit Icon (Segoe Fluent Icons / MDL2 "Crop")
        std::wstring hint = TR(S_CAPTURE_HINT);
        int tw = TextWidth(bdc, g_font, hint) + 24 + 34;
        RECT tr{ (w - tw) / 2, 28, (w + tw) / 2, 28 + 40 };
        DrawRoundBox(bdc, tr, RGB(251, 251, 251), RGB(229, 229, 229), 8);
        DrawTextIn(bdc, MakeRect(tr.left + 12, tr.top, tr.left + 40, tr.bottom), L"\xE7A8", g_iconFont, RGB(0, 103, 192), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextIn(bdc, MakeRect(tr.left + 44, tr.top, tr.right - 12, tr.bottom), hint, g_font, RGB(27, 27, 27), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    BitBlt(hdc, 0, 0, w, h, bdc, 0, 0, SRCCOPY);
    SelectObject(bdc, ob);
    DeleteDC(bdc);
}

POINT ScreenPt(HWND h, LPARAM lp)
{
    POINT p{ (short)LOWORD(lp), (short)HIWORD(lp) };
    ClientToScreen(h, &p);
    return p;
}

LRESULT CALLBACK OverlayProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        if (Mon* m = FindMon(h)) PaintOverlay(*m, hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_CROSS));
        return TRUE;
    case WM_LBUTTONDOWN:
        if (g_finishing) return 0;
        SetCapture(h);
        g_selecting = true;
        g_start = g_cur = ScreenPt(h, lp);
        InvalidateAll();
        return 0;
    case WM_MOUSEMOVE:
        if (g_selecting && !g_finishing) {
            g_cur = ScreenPt(h, lp);
            InvalidateAll();
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_selecting && !g_finishing) {
            g_cur = ScreenPt(h, lp);
            g_selecting = false;
            ReleaseCapture();
            RECT sel = NormRect(g_start, g_cur);
            if (RW(sel) < kMinSize || RH(sel) < kMinSize) Cancel();   // zu kleine Auswahl
            else Grab(sel);
        }
        return 0;
    case WM_RBUTTONDOWN:
        Cancel();
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) Cancel();
        return 0;
    case WM_TIMER:
        // Fallback, falls das Overlay keinen Tastaturfokus bekommen hat: Esc direkt abfragen.
        if (wp == kEscTimer && !g_finishing && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) Cancel();
        return 0;
    case WM_KILLFOCUS:
        if (!g_finishing && g_active && !IsOverlay((HWND)wp)) Cancel();  // Fokusverlust
        return 0;
    case WM_CLOSE:
        Cancel();
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---- Countdown-Karte waehrend der Verzoegerung (nicht aktivierbar, oben mittig)

void PaintBadge(HWND h, HDC hdc)
{
    RECT rc; GetClientRect(h, &rc);
    CreateFonts();
    DrawRoundBox(hdc, rc, RGB(251, 251, 251), RGB(200, 200, 200), 8);
    std::wstring txt = TRF(S_COUNTDOWN_FMT, std::max(0, g_badgeSecs));
    DrawTextIn(hdc, MakeRect(rc.left + 12, rc.top, rc.left + 40, rc.bottom), L"\xE916", g_iconFont, RGB(0, 103, 192), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextIn(hdc, MakeRect(rc.left + 44, rc.top, rc.right - 12, rc.bottom), txt, g_font, RGB(27, 27, 27), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

LRESULT CALLBACK BadgeProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC m = CreateCompatibleDC(hdc);
        HBITMAP b = CreateCompatibleBitmap(hdc, RW(rc), RH(rc));
        HGDIOBJ o = SelectObject(m, b);
        PaintBadge(h, m);
        BitBlt(hdc, 0, 0, RW(rc), RH(rc), m, 0, 0, SRCCOPY);
        SelectObject(m, o); DeleteObject(b); DeleteDC(m);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
        Cancel();                                   // Klick auf die Karte bricht ab
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        return TRUE;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void ShowBadge(int secs)
{
    if (g_badge) {
        if (secs != g_badgeSecs) { g_badgeSecs = secs; InvalidateRect(g_badge, NULL, FALSE); }
        return;
    }
    g_badgeSecs = secs;
    CreateFonts();
    HDC sdc = GetDC(NULL);
    int tw = TextWidth(sdc, g_font, TRF(S_COUNTDOWN_FMT, 99)) + 24 + 34;
    ReleaseDC(NULL, sdc);
    MONITORINFO mi{}; mi.cbSize = sizeof(mi);
    POINT origin{ 0, 0 };
    GetMonitorInfoW(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &mi);
    int x = (mi.rcWork.left + mi.rcWork.right - tw) / 2, y = mi.rcWork.top + 24;
    g_badge = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kBadgeClass, L"MultiSnip",
        WS_POPUP, x, y, tw, 40, NULL, NULL, g_hInst, NULL);
    if (g_badge) {
        // Aus Bildschirmaufnahmen ausblenden (Win10 2004+); zusaetzlich wird die Karte vor dem Einfrieren entfernt.
        typedef BOOL(WINAPI* SetAffinityFn)(HWND, DWORD);
        if (HMODULE u32 = GetModuleHandleW(L"user32.dll"))
            if (auto fn = (SetAffinityFn)GetProcAddress(u32, "SetWindowDisplayAffinity")) fn(g_badge, 0x11 /*WDA_EXCLUDEFROMCAPTURE*/);
        ShowWindow(g_badge, SW_SHOWNOACTIVATE);
    }
}

BOOL CALLBACK MonProc(HMONITOR hm, HDC, LPRECT, LPARAM)
{
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hm, &mi)) {
        Mon m;
        m.rc = mi.rcMonitor;
        if (RW(m.rc) > 0 && RH(m.rc) > 0) g_mons.push_back(m);
    }
    return TRUE;
}

void Freeze()
{
    HDC sdc = GetDC(NULL);
    for (auto& m : g_mons) {
        int w = RW(m.rc), h = RH(m.rc);
        m.bmp = CreateCompatibleBitmap(sdc, w, h);
        m.dc = CreateCompatibleDC(sdc);
        m.old = SelectObject(m.dc, m.bmp);
        BitBlt(m.dc, 0, 0, w, h, sdc, m.rc.left, m.rc.top, SRCCOPY | CAPTUREBLT);
    }
    ReleaseDC(NULL, sdc);

    if (g_cursor) {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) && ci.hCursor) {
            ICONINFO ii{};
            if (GetIconInfo(ci.hCursor, &ii)) {
                for (auto& m : g_mons) {
                    if (!PtIn(m.rc, ci.ptScreenPos)) continue;
                    DrawIconEx(m.dc, ci.ptScreenPos.x - m.rc.left - (int)ii.xHotspot,
                        ci.ptScreenPos.y - m.rc.top - (int)ii.yHotspot,
                        ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
                    break;
                }
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
        }
    }
}

// SetForegroundWindow gelingt nur dem Vordergrundprozess. Nach Verstecken des
// Hauptfensters und einer Verzoegerung ist das laengst ein anderer; deshalb
// haengen wir uns kurz an dessen Eingabe-Thread.
void ForceForeground(HWND h)
{
    HWND fg = GetForegroundWindow();
    DWORD fgThread = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD me = GetCurrentThreadId();
    bool attached = fgThread && fgThread != me && AttachThreadInput(fgThread, me, TRUE);
    SetForegroundWindow(h);
    SetActiveWindow(h);
    SetFocus(h);
    if (attached) AttachThreadInput(fgThread, me, FALSE);
}

void RegisterClasses()
{
    if (g_classRegistered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(NULL, IDC_CROSS);
    RegisterClassW(&wc);
    WNDCLASSW bc{};
    bc.lpfnWndProc = BadgeProc;
    bc.hInstance = g_hInst;
    bc.lpszClassName = kBadgeClass;
    bc.hCursor = LoadCursorW(NULL, IDC_HAND);
    RegisterClassW(&bc);
    g_classRegistered = true;
}

void CreateOverlays()
{
    CreateFonts();
    for (auto& m : g_mons) {
        m.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClass, L"MultiSnip Capture",
            WS_POPUP, m.rc.left, m.rc.top, RW(m.rc), RH(m.rc), NULL, NULL, g_hInst, NULL);
    }
    for (auto& m : g_mons) if (m.hwnd) ShowWindow(m.hwnd, SW_SHOWNA);
    if (!g_mons.empty() && g_mons[0].hwnd) {
        ForceForeground(g_mons[0].hwnd);
        SetTimer(g_mons[0].hwnd, kEscTimer, 50, NULL);
    }
}

void CALLBACK DelayProc(HWND, UINT, UINT_PTR id, DWORD)
{
    if (g_timer != id) { KillTimer(NULL, id); return; }
    if (!g_active || g_finishing) { KillTimer(NULL, id); g_timer = 0; return; }
    ULONGLONG now = GetTickCount64();
    LONGLONG remaining = (LONGLONG)(g_deadline - now);
    if (remaining > kBadgeHideMs) {
        if (remaining >= 1000) ShowBadge((int)((remaining + 999) / 1000));
        else DestroyBadge();
        return;
    }
    if (g_badge) { DestroyBadge(); return; }            // naechster Tick friert ein (Karte ist dann weg)
    if (remaining > 0) return;
    KillTimer(NULL, id);
    g_timer = 0;
    g_waiting = false;
    g_mons.clear();
    EnumDisplayMonitors(NULL, NULL, MonProc, 0);
    if (g_mons.empty()) { Cancel(); return; }
    Freeze();
    CreateOverlays();
}

} // namespace

bool Capture_IsActive() { return g_active; }
bool Capture_IsWaiting() { return g_active && g_waiting; }
void Capture_Cancel() { Cancel(); }

void Capture_Begin(HINSTANCE hInst, int delayMs, bool includeCursor,
    CaptureMode mode, int replaceIndex, CaptureDoneFn done, void* ctx)
{
    if (g_active) return;
    g_active = true;
    g_waiting = true;
    g_finishing = false;
    g_selecting = false;
    g_hInst = hInst;
    g_cursor = includeCursor;
    g_mode = mode;
    g_replaceIdx = replaceIndex;
    g_done = done;
    g_ctx = ctx;
    RegisterClasses();
    // Mindestens kurz warten, damit der DWM das ausgeblendete Hauptfenster entfernt hat.
    int delay = std::max(delayMs, 250);
    g_deadline = GetTickCount64() + (ULONGLONG)delay;
    g_timer = SetTimer(NULL, 0, 100, DelayProc);
    if (!g_timer) { g_active = false; g_waiting = false; if (done) done(nullptr, 0, 0, mode, replaceIndex, ctx); }
    else if (delay >= 1000) ShowBadge((delay + 999) / 1000);
}
