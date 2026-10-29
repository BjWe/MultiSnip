// MultiSnip - stapelndes Snipping-Tool. Hauptfenster, komplett selbst gezeichnet.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cwctype>
#include "Model.h"
#include "Image.h"
#include "Capture.h"
#include "Util.h"
#include "Lang.h"
#include "Ocr.h"
#include "resource.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <unordered_map>

// ---------------------------------------------------------------- Konstanten

#define WM_TRAYICON   (WM_APP + 1)
#define WM_OCR_RESULT (WM_APP + 2)
#define ICO_OCR       L"\xE8D2"
enum { ID_HOTKEY = 1, IDT_AUTOSCROLL = 2, IDT_FLASH = 3 };
enum { IDM_SNIP = 100, IDM_SHOW, IDM_EXIT, IDM_DELAY_BASE = 120, IDM_AUTOCOPY_BASE = 130, IDM_TAB_BASE = 200 };
// Auswahl im Verzoegerungsmenue (Split-Button und Tray); Menue-ID = IDM_DELAY_BASE + Index
static const int kDelayChoices[] = { 0, 500, 1000, 2000, 3000, 5000, 10000 };
static const int kDelayCount = (int)(sizeof(kDelayChoices) / sizeof(kDelayChoices[0]));
#define ICO_CHEVRON  L"\xE70D"

// Fluent-Design-Farben (Windows 11), hell und dunkel. Die CLR_*-Namen bleiben
// als Makros bestehen und zeigen auf die aktive Palette.
struct Theme {
    COLORREF bg, layer, card, cardHot, stroke, stroke2, stroke3;
    COLORREF text, text2, text3, accent, accentHot, onAccent, subtleHot, subtleHot2, danger;
    COLORREF selBg;        // Markierung beim Inline-Umbenennen
    COLORREF captureBg;    // Hotkey-Erfassung aktiv
};
static const Theme kLight = {
    RGB(243, 243, 243), RGB(249, 249, 249), RGB(251, 251, 251), RGB(246, 246, 246),
    RGB(229, 229, 229), RGB(220, 220, 220), RGB(190, 190, 190),
    RGB(27, 27, 27), RGB(96, 96, 96), RGB(160, 160, 160),
    RGB(0, 103, 192), RGB(25, 117, 197), RGB(255, 255, 255),
    RGB(234, 234, 234), RGB(222, 222, 222), RGB(196, 43, 28),
    RGB(204, 228, 247), RGB(255, 250, 225) };
static const Theme kDark = {
    RGB(32, 32, 32), RGB(39, 39, 39), RGB(45, 45, 45), RGB(52, 52, 52),
    RGB(58, 58, 58), RGB(68, 68, 68), RGB(110, 110, 110),
    RGB(255, 255, 255), RGB(200, 200, 200), RGB(125, 125, 125),
    RGB(96, 205, 255), RGB(140, 220, 255), RGB(0, 0, 0),
    RGB(54, 54, 54), RGB(66, 66, 66), RGB(255, 112, 100),
    RGB(40, 80, 110), RGB(70, 62, 30) };
static Theme g_th = kLight;
static bool g_dark = false;
#define CLR_BG          g_th.bg
#define CLR_LAYER       g_th.layer
#define CLR_CARD        g_th.card
#define CLR_CARD_HOT    g_th.cardHot
#define CLR_STROKE      g_th.stroke
#define CLR_STROKE2     g_th.stroke2
#define CLR_STROKE3     g_th.stroke3
#define CLR_TEXT        g_th.text
#define CLR_TEXT2       g_th.text2
#define CLR_TEXT3       g_th.text3
#define CLR_ACCENT      g_th.accent
#define CLR_ACCENT_HOT  g_th.accentHot
#define CLR_ON_ACCENT   g_th.onAccent
#define CLR_SUBTLE_HOT  g_th.subtleHot
#define CLR_SUBTLE_HOT2 g_th.subtleHot2
#define CLR_DANGER      g_th.danger

// Glyphen aus "Segoe Fluent Icons" (Win11) bzw. "Segoe MDL2 Assets" (Win10) - gleiche Codepunkte.
#define ICO_HOME     L"\xE80F"
#define ICO_ADD      L"\xE710"
#define ICO_MINUS    L"\xE738"
#define ICO_CLOSE    L"\xE8BB"
#define ICO_SETTINGS L"\xE713"
#define ICO_COPY     L"\xE8C8"
#define ICO_SAVE     L"\xE74E"
#define ICO_RENAME   L"\xE8AC"
#define ICO_SNIP     L"\xE7A8"
#define ICO_REPLACE  L"\xE72C"
#define ICO_DELETE   L"\xE74D"
#define ICO_CHECK    L"\xE73E"
#define ICO_ACCEPT   L"\xE8FB"
#define ICO_ZOOMIN   L"\xE8A3"
#define ICO_ZOOMOUT  L"\xE71F"
#define ICO_HIDDEN   L"\xE7B3"
#define ICO_BACK     L"\xE72B"
#define ICO_INFO     L"\xE946"
#define ICO_COLOR    L"\xE790"
#define ICO_KEYBOARD L"\xE765"
#define ICO_TIMER    L"\xE916"
#define ICO_MOUSE    L"\xE962"
#define ICO_WARN     L"\xE7BA"
#define ICO_FOLDER   L"\xE838"
#define ICO_GRIP     L"\xE700"
#define ICO_IMAGE    L"\xEB9F"
#define ICO_PADX     L"\xE8A4"
#define ICO_PADY     L"\xE8A4"

enum class Page { Main, Settings, About, Edit };
#define ICO_EDIT     L"\xE70F"
#define ICO_ABOUT    L"\xE946"
#define ICO_LINK     L"\xE71B"
#define ICO_CODE     L"\xE943"
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
static const wchar_t* kAppVersion = WIDEN(APP_VERSION_STR);
static const wchar_t* kGitHubUrl = L"https://github.com/BjWe/MultiSnip";

enum class HK {
    None, Tab, TabClose, TabAdd, Dash,
    Snip, SnipDelay, Copy, Save, Rename, Settings,
    ZoomOut, ZoomIn, ZoomReset,
    SChk, SDel, SRep, SBody, SCopy, SOcr, DashNew,
    SetBack, SetToggle, SetMinus, SetPlus, SetColor, SetHotkey, SetResetAsk, SetLang, SetAutoCopy, SetNumScope,
    About, AboutGitHub,
    SEdit, Canvas, Tool, Color, Width, Undo, Redo, EdDone, EdDeleteSel,
    LayerSel, LayerVis, LayerAdd, LayerDel, LayerUp, LayerDown, EdCropReset, EdNumMinus, EdNumPlus,
    ScrollThumb, ScrollTrack, ScrollHThumb, ScrollHTrack, TabMore, SetTheme
};
struct Zone { RECT r; HK kind; int a; };
struct Row { RECT strip, img; int top, bottom; };

// Einstellungs-IDs fuer generische Zonen
enum { SET_DEF_FILL = 0, SET_DEF_PADX, SET_DEF_PADY, SET_DELAY, SET_CURSOR, SET_DASH, SET_TAB_FILL, SET_TAB_PADX, SET_TAB_PADY, SET_TAB_DELAY };

// ---------------------------------------------------------------- Zustand

static HINSTANCE g_hInst = nullptr;
static HWND g_hwnd = nullptr;
static int g_dpi = 96;
static HFONT g_font = nullptr, g_fontB = nullptr, g_fontH = nullptr, g_fontSub = nullptr;
static HFONT g_fontIcon = nullptr, g_fontIconS = nullptr, g_fontIconL = nullptr, g_fontIconXL = nullptr;
static const wchar_t* g_iconFace = nullptr;
static int g_active = -1;                 // -1 = Dashboard
static Page g_page = Page::Main;
static double g_zoom = 1.0;
static HBITMAP g_back = nullptr;
static int g_backW = 0, g_backH = 0;
static std::vector<Zone> g_zones;
static HK g_hoverKind = HK::None;
static int g_hoverA = -1;
// Tastaturfokus: eine Zone (Art + Index), sichtbar nach Tab/Pfeiltasten, unsichtbar nach Mausklick
static HK g_focusKind = HK::None;
static int g_focusA = -1;
static bool g_focusVisible = false;
static int g_contentH = 0;
static RECT g_rcContent{};
static std::vector<Row> g_rows;
static int g_x0 = 0, g_sheetW = 0;
static RECT g_thumb{};
static RECT g_hthumb{};                   // horizontaler Scrollbalken
static bool g_hthumbDrag = false;
static int g_hthumbGrab = 0;
static int g_maxScrollX = 0;
static int g_tabFirst = 0;                // erster sichtbarer Tab, wenn die Leiste ueberlaeuft
static int g_modal = 0;                   // > 0: modaler Dialog offen, kein Capture starten
static int g_loadedTab = -2;              // Tab, dessen Bitmaps zuletzt im Speicher gehalten wurden
// Drag
static bool g_mouseDown = false;
static int g_pressSnip = -1;
static POINT g_pressPt{};
static bool g_dragging = false;
static int g_dropIdx = -1;
static int g_autoDir = 0;
static POINT g_lastMouse{};
static bool g_thumbDrag = false;
static int g_thumbGrab = 0;
// Inline-Umbenennen
static int g_editTab = -1;
static std::wstring g_editText;
static bool g_editSelectAll = false;      // erster Tastendruck ersetzt den ganzen Titel
// Hotkey
static bool g_hotkeyCapture = false;
static bool g_hotkeyRegistered = false;
// Tray
static NOTIFYICONDATAW g_nid{};
static HICON g_icon = nullptr, g_trayIcon = nullptr;
static UINT g_taskbarCreated = 0;
// Capture
static bool g_wasVisible = true;
// Statusmeldung
static std::wstring g_flash;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static Tab* CurTab()
{
    Tab* t = (g_active >= 0 && g_active < (int)g_app.tabs.size()) ? g_app.tabs[g_active].get() : nullptr;
    // Nur der aktive Tab haelt seine Bitmaps im Speicher; alle anderen werden beim Wechsel freigegeben.
    if (t && !t->loaded) Model_LoadTabBitmaps(*t);
    if (g_loadedTab != g_active) {
        for (int i = 0; i < (int)g_app.tabs.size(); i++)
            if (i != g_active && g_app.tabs[i]->loaded) Model_UnloadTabBitmaps(*g_app.tabs[i]);
        g_loadedTab = g_active;
    }
    return t;
}
// Modaler Dialog: solange er offen ist, startet kein Capture (Hotkey, Tray, Knopf).
struct ModalScope { ModalScope() { g_modal++; } ~ModalScope() { g_modal--; } };
static void Redraw() { if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }
static void CommitRename(bool apply);

// Helles/dunkles Design: 0 = Windows-Einstellung, 1 = hell, 2 = dunkel.
static bool SystemPrefersDark()
{
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &sz) == ERROR_SUCCESS) return v == 0;
    return false;
}
static void ApplyTheme()
{
    int mode = g_app.def.theme;
    g_dark = (mode == 2) || (mode == 0 && SystemPrefersDark());
    g_th = g_dark ? kDark : kLight;
    if (g_hwnd) {
        BOOL dark = g_dark ? TRUE : FALSE;          // Titelleiste passend faerben (Win10 1809+ / Win11)
        if (FAILED(DwmSetWindowAttribute(g_hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark))))
            DwmSetWindowAttribute(g_hwnd, 19, &dark, sizeof(dark));
        Redraw();
    }
}

// Texteingabe-Helfer fuer Inline-Felder: Surrogatpaare bleiben zusammen, Einfuegen aus der Zwischenablage.
static void TextBackspace(std::wstring& s)
{
    if (s.empty()) return;
    bool pair = s.size() >= 2 && IS_LOW_SURROGATE(s.back()) && IS_HIGH_SURROGATE(s[s.size() - 2]);
    s.erase(s.size() - (pair ? 2 : 1));
}
static void TextAppend(std::wstring& s, wchar_t ch, size_t maxLen)
{
    if (s.size() < maxLen || (IS_LOW_SURROGATE(ch) && s.size() <= maxLen)) s.push_back(ch);
}
static std::wstring ClipboardLine()
{
    std::wstring out;
    if (!OpenClipboard(g_hwnd)) return out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) {
            for (; *p; p++) { if (*p == L'\r' || *p == L'\n' || *p == L'\t') break; if (*p >= 32) out.push_back(*p); }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}
static void Flash(const std::wstring& s)
{
    g_flash = s;
    SetTimer(g_hwnd, IDT_FLASH, 2500, NULL);
    Redraw();
}

// ---------------------------------------------------------------- Hotkey-Namen

static std::wstring KeyName(UINT vk)
{
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lp = (LONG)(sc << 16);
    switch (vk) {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN: case VK_PRIOR: case VK_NEXT:
    case VK_END: case VK_HOME: case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
    case VK_SNAPSHOT:
        lp |= 1 << 24; break;
    }
    wchar_t b[64]{};
    if (GetKeyNameTextW(lp, b, 64) > 0) return b;
    wchar_t c[16];
    swprintf_s(c, L"0x%02X", vk);
    return c;
}
static std::wstring HotkeyName(UINT mod, UINT vk)
{
    std::wstring s;
    if (mod & MOD_CONTROL) s += TR(S_MOD_CTRL);
    if (mod & MOD_ALT) s += TR(S_MOD_ALT);
    if (mod & MOD_SHIFT) s += TR(S_MOD_SHIFT);
    if (mod & MOD_WIN) s += TR(S_MOD_WIN);
    return s + KeyName(vk);
}

// ---------------------------------------------------------------- Fonts / DPI

// Icon-Schriften: primaere Familie plus Ausweichfamilie in denselben vier Groessen.
static HFONT g_fontIconAlt[4] = {};
static const wchar_t* g_iconFaceAlt = nullptr;
static std::unordered_map<wchar_t, int> g_glyphSrc;   // 0 = primaer, 1 = Ausweichschrift, 2 = Textersatz

static void CreateFonts()
{
    for (HFONT* f : { &g_font, &g_fontB, &g_fontH, &g_fontSub, &g_fontIcon, &g_fontIconS, &g_fontIconL, &g_fontIconXL,
                      &g_fontIconAlt[0], &g_fontIconAlt[1], &g_fontIconAlt[2], &g_fontIconAlt[3] }) {
        if (*f) DeleteObject(*f);
        *f = nullptr;
    }
    if (!g_iconFace) {
        // Segoe Fluent Icons (Windows 11) mit Fallback auf Segoe MDL2 Assets (Windows 10): gleiche Codepunkte.
        bool fluent = FontExists(L"Segoe Fluent Icons"), mdl2 = FontExists(L"Segoe MDL2 Assets");
        g_iconFace = fluent ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
        g_iconFaceAlt = (fluent && mdl2) ? L"Segoe MDL2 Assets" : nullptr;
    }
    g_glyphSrc.clear();
    auto mk = [](int pt, int weight, const wchar_t* face) {
        return CreateFontW(-MulDiv(pt, g_dpi, 72), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    };
    g_font = mk(9, FW_NORMAL, L"Segoe UI");
    g_fontB = mk(9, FW_SEMIBOLD, L"Segoe UI");
    g_fontH = mk(15, FW_SEMIBOLD, L"Segoe UI");
    g_fontSub = mk(8, FW_NORMAL, L"Segoe UI");
    g_fontIconS = mk(9, FW_NORMAL, g_iconFace);
    g_fontIcon = mk(12, FW_NORMAL, g_iconFace);
    g_fontIconL = mk(15, FW_NORMAL, g_iconFace);
    g_fontIconXL = mk(30, FW_NORMAL, g_iconFace);
    if (g_iconFaceAlt) {
        g_fontIconAlt[0] = mk(9, FW_NORMAL, g_iconFaceAlt);
        g_fontIconAlt[1] = mk(12, FW_NORMAL, g_iconFaceAlt);
        g_fontIconAlt[2] = mk(15, FW_NORMAL, g_iconFaceAlt);
        g_fontIconAlt[3] = mk(30, FW_NORMAL, g_iconFaceAlt);
    }
}

// Textersatz fuer Glyphen, die in keiner Icon-Schrift vorhanden sind (aeltere Windows-10-Builds).
static const wchar_t* GlyphFallbackText(wchar_t cp)
{
    switch (cp) {
    case 0xE8BB: return L"\x2715";  // Close
    case 0xE710: case 0xE8A3: return L"+";
    case 0xE738: case 0xE71F: return L"\x2212";
    case 0xE713: return L"\x2699";  // Settings
    case 0xE80F: return L"\x2302";  // Home
    case 0xE74D: return L"\x2715";  // Delete
    case 0xE73E: case 0xE8FB: return L"\x2713";
    case 0xE70F: return L"\x270E";  // Edit
    case 0xE72C: return L"\x21BB";  // Refresh
    case 0xE946: return L"i";
    case 0xE72B: return L"\x2190";  // Back
    case 0xE7A8: return L"\x2702";  // Crop/Snip
    case 0xE8C8: return L"\x2398";  // Copy
    case 0xE74E: return L"\x25A3";  // Save
    case 0xE7BA: return L"\x26A0";  // Warning
    case 0xE700: return L"\x2261";  // Grip
    case 0xE70D: return L"\x25BE";  // Chevron down
    case 0xE712: return L"\x2026";  // More
    case 0xE7A7: return L"\x21B6";  // Undo
    case 0xE7A6: return L"\x21B7";  // Redo
    case 0xE74A: return L"\x2191";
    case 0xE74B: return L"\x2193";
    default: return L"\x2022";
    }
}

// ---------------------------------------------------------------- Zeichnen: Bausteine (Fluent-Stil)

static void Zone_Add(const RECT& r, HK k, int a = -1) { g_zones.push_back({ r, k, a }); }
static bool IsHot(HK k, int a = -1) { return (g_hoverKind == k && g_hoverA == a) || (g_focusVisible && g_focusKind == k && g_focusA == a); }

// Glyph aus dem Icon-Font zentriert zeichnen.
static void DrawIcon(HDC dc, const RECT& r, const wchar_t* glyph, COLORREF c, HFONT f = nullptr)
{
    if (!f) f = g_fontIcon;
    if (!glyph || !*glyph) return;
    wchar_t cp = glyph[0];
    int src = 0;
    if (cp >= 0xE000 && cp <= 0xF8FF) {               // nur Private-Use-Glyphen pruefen
        auto it = g_glyphSrc.find(cp);
        if (it == g_glyphSrc.end()) {
            if (GlyphExists(dc, f, cp)) src = 0;
            else if (g_fontIconAlt[1] && GlyphExists(dc, g_fontIconAlt[1], cp)) src = 1;
            else src = 2;
            g_glyphSrc[cp] = src;
        }
        else src = it->second;
    }
    if (src == 1) {
        int i = (f == g_fontIconS) ? 0 : (f == g_fontIconL) ? 2 : (f == g_fontIconXL) ? 3 : 1;
        if (g_fontIconAlt[i]) f = g_fontIconAlt[i];
    }
    else if (src == 2) {
        DrawTextIn(dc, r, GlyphFallbackText(cp), (f == g_fontIconXL) ? g_fontH : g_fontB, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    DrawTextIn(dc, r, glyph, f, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

enum class BtnStyle { Standard, Accent, Subtle };

// Fluent-Button: optional Icon links, Text rechts davon. Ohne Text = reiner Icon-Button.
static void DrawButton(HDC dc, const RECT& r, const wchar_t* icon, const std::wstring& text,
    bool hot, BtnStyle style, bool enabled = true)
{
    COLORREF fill, border, tc;
    switch (style) {
    case BtnStyle::Accent:
        fill = hot ? CLR_ACCENT_HOT : CLR_ACCENT; border = fill; tc = CLR_ON_ACCENT; break;
    case BtnStyle::Subtle:
        fill = hot ? CLR_SUBTLE_HOT : CLR_BG; border = fill; tc = enabled ? CLR_TEXT : CLR_TEXT3; break;
    default:
        fill = hot ? CLR_CARD_HOT : CLR_CARD; border = CLR_STROKE2; tc = enabled ? CLR_TEXT : CLR_TEXT3; break;
    }
    if (!enabled) { fill = CLR_BG; border = CLR_STROKE; }
    if (style != BtnStyle::Subtle || hot) DrawRoundBox(dc, r, fill, border, S(4));
    if (style == BtnStyle::Standard && enabled) FillSolid(dc, MakeRect(r.left + S(4), r.bottom - 1, r.right - S(4), r.bottom), CLR_STROKE2);
    int x = r.left;
    if (icon && *icon) {
        int iw = text.empty() ? RW(r) : S(32);
        DrawIcon(dc, MakeRect(x, r.top, x + iw, r.bottom), icon, tc);
        x += text.empty() ? iw : S(28);
    }
    if (!text.empty()) {
        RECT tr{ x + (icon && *icon ? S(2) : S(12)), r.top, r.right - S(12), r.bottom };
        DrawTextIn(dc, tr, text, g_font, tc, (icon && *icon ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

static void DrawCheck(HDC dc, const RECT& r, bool on, bool hot)
{
    if (on) {
        DrawRoundBox(dc, r, hot ? CLR_ACCENT_HOT : CLR_ACCENT, hot ? CLR_ACCENT_HOT : CLR_ACCENT, S(4));
        DrawIcon(dc, r, ICO_CHECK, CLR_ON_ACCENT, g_fontIconS);
    }
    else {
        DrawRoundBox(dc, r, hot ? CLR_SUBTLE_HOT : CLR_CARD, CLR_STROKE3, S(4));
    }
}

static void DrawToggle(HDC dc, const RECT& r, bool on, bool hot)
{
    COLORREF fill = on ? (hot ? CLR_ACCENT_HOT : CLR_ACCENT) : (hot ? CLR_SUBTLE_HOT : CLR_CARD);
    COLORREF border = on ? fill : CLR_STROKE3;
    DrawRoundBox(dc, r, fill, border, RH(r) / 2);
    int d = RH(r) - S(8);
    if (hot) d += S(2);
    int kx = on ? r.right - S(4) - d : r.left + S(4);
    int ky = r.top + (RH(r) - d) / 2;
    RECT knob{ kx, ky, kx + d, ky + d };
    COLORREF kc = on ? CLR_ON_ACCENT : CLR_TEXT2;
    DrawRoundBox(dc, knob, kc, kc, d / 2);
}

// Card-Hintergrund (Fluent "Layer"/"Card": helle Fläche mit feinem Strich).
static void DrawCard(HDC dc, const RECT& r, bool hot = false)
{
    DrawRoundBox(dc, r, hot ? CLR_CARD_HOT : CLR_CARD, CLR_STROKE, S(6));
}

// ---------------------------------------------------------------- Zeichnen: Bereiche

static int TabBarH() { return S(44); }
static int ToolbarH() { return S(48); }
static int StatusH() { return S(30); }
static int SbW() { return S(14); }

static void DrawTabBar(HDC dc, int cw)
{
    RECT bar{ 0, 0, cw, TabBarH() };
    FillSolid(dc, bar, CLR_BG);
    int x = S(8), top = S(8), bottom = TabBarH();
    const int radius = S(7);
    auto drawTabShape = [&](const RECT& r, bool act, bool hot) {
        if (act) {
            // Gewaehlter Tab: Layer-Farbe, oben gerundet, unten offen zum Inhalt.
            RECT ext = r; ext.bottom += radius * 2;
            HRGN clip = CreateRectRgn(r.left, r.top, r.right, r.bottom);
            SelectClipRgn(dc, clip); DeleteObject(clip);
            DrawRoundBox(dc, ext, CLR_LAYER, CLR_STROKE, radius);
            SelectClipRgn(dc, NULL);
            FillSolid(dc, MakeRect(r.left + 1, r.bottom - 1, r.right - 1, r.bottom), CLR_LAYER);
        }
        else if (hot) {
            RECT hr = r; hr.bottom -= S(4);
            DrawRoundBox(dc, hr, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(5));
        }
    };

    if (g_app.def.dashboard) {
        RECT r{ x, top, x + S(44), bottom };
        bool act = (g_active == -1);
        drawTabShape(r, act, IsHot(HK::Dash));
        DrawIcon(dc, MakeRect(r.left, r.top, r.right, r.bottom - S(2)), ICO_HOME, act ? CLR_ACCENT : CLR_TEXT2);
        Zone_Add(r, HK::Dash);
        x += S(48);
    }
    // Breiten bestimmen. Passt nicht alles, werden die Tabs erst gleichmaessig
    // geschrumpft; reicht das nicht, zeigt die Leiste ab g_tabFirst so viele wie
    // moeglich (den aktiven immer) und bietet rechts ein Menue mit allen Tabs.
    const int n = (int)g_app.tabs.size();
    std::vector<int> widths(n);
    int total = 0;
    for (int i = 0; i < n; i++) {
        std::wstring title = (g_editTab == i) ? g_editText : g_app.tabs[i]->title;
        int tw = TextWidth(dc, (i == g_active) ? g_fontB : g_font, title);
        widths[i] = std::min(std::max(tw + S(52), S(100)), S(260));
        total += widths[i] + S(2);
    }
    const int avail = cw - S(48) - x;                  // bis zum "+"
    bool overflow = total > avail;
    if (overflow) {
        double f = (double)avail / std::max(1, total);
        int t2 = 0;
        for (int i = 0; i < n; i++) { widths[i] = std::max(S(76), (int)(widths[i] * f)); t2 += widths[i] + S(2); }
        overflow = t2 > avail;
    }
    int first = 0, last = n - 1;
    if (overflow) {
        const int availT = avail - S(40);               // Platz fuer den Menue-Knopf
        g_tabFirst = std::max(0, std::min(g_tabFirst, n - 1));
        if (g_active >= 0 && g_active < g_tabFirst) g_tabFirst = g_active;
        auto fits = [&](int from, int to) { int s = 0; for (int i = from; i <= to; i++) s += widths[i] + S(2); return s <= availT; };
        if (g_active >= g_tabFirst) while (g_tabFirst < g_active && !fits(g_tabFirst, g_active)) g_tabFirst++;
        first = g_tabFirst;
        last = first;
        int s = 0;
        for (int i = first; i < n; i++) { s += widths[i] + S(2); if (s > availT) break; last = i; }
    }
    else g_tabFirst = 0;
    for (int i = first; i <= last; i++) {
        Tab& t = *g_app.tabs[i];
        bool act = (i == g_active);
        bool editing = (g_editTab == i);
        std::wstring title = editing ? g_editText : t.title;
        int w = widths[i];
        RECT r{ x, top, x + w, bottom };
        drawTabShape(r, act, IsHot(HK::Tab, i));
        RECT tr{ r.left + S(12), r.top, r.right - S(34), r.bottom - S(2) };
        if (editing) {
            RECT er{ r.left + S(8), r.top + S(6), r.right - S(32), r.bottom - S(6) };
            DrawRoundBox(dc, er, CLR_CARD, CLR_ACCENT, S(4));
            FillSolid(dc, MakeRect(er.left + S(4), er.bottom - S(2), er.right - S(4), er.bottom - 1), CLR_ACCENT);
            tr = MakeRect(er.left + S(6), er.top, er.right - S(2), er.bottom);
            int textW = std::min(TextWidth(dc, g_font, title), RW(tr));
            if (g_editSelectAll && !title.empty())
                FillSolid(dc, MakeRect(tr.left, tr.top + S(3), tr.left + textW, tr.bottom - S(3)), g_th.selBg);
            DrawTextIn(dc, tr, title, g_font, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            int cx = tr.left + textW;
            FillSolid(dc, MakeRect(cx, tr.top + S(4), cx + 1, tr.bottom - S(4)), CLR_TEXT);
        }
        else {
            DrawTextIn(dc, tr, title, act ? g_fontB : g_font, act ? CLR_TEXT : CLR_TEXT2,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        RECT xr{ r.right - S(30), r.top + S(8), r.right - S(8), r.bottom - S(10) };
        bool xhot = IsHot(HK::TabClose, i);
        if (xhot) DrawRoundBox(dc, xr, CLR_SUBTLE_HOT2, CLR_SUBTLE_HOT2, S(4));
        if (act || xhot || IsHot(HK::Tab, i)) DrawIcon(dc, xr, ICO_CLOSE, xhot ? CLR_TEXT : CLR_TEXT2, g_fontIconS);
        Zone_Add(xr, HK::TabClose, i);
        Zone_Add(r, HK::Tab, i);
        x += w + S(2);
    }
    if (overflow) {
        RECT more{ x + S(2), top + S(4), x + S(2) + S(34), bottom - S(6) };
        if (IsHot(HK::TabMore)) DrawRoundBox(dc, more, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(5));
        DrawIcon(dc, more, L"\xE712", CLR_TEXT2);          // "More" (drei Punkte)
        Zone_Add(more, HK::TabMore);
        x = more.right;
    }
    RECT plus{ x + S(2), top + S(4), x + S(2) + S(34), bottom - S(6) };
    if (IsHot(HK::TabAdd)) DrawRoundBox(dc, plus, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(5));
    DrawIcon(dc, plus, ICO_ADD, CLR_TEXT2);
    Zone_Add(plus, HK::TabAdd);
    FillSolid(dc, MakeRect(0, bottom - 1, cw, bottom), CLR_STROKE);
}

static int AddCmdButton(HDC dc, int x, int y, const wchar_t* icon, const std::wstring& text, HK k, BtnStyle style, bool enabled = true)
{
    int w = TextWidth(dc, g_font, text) + S(28) + S(14);
    RECT r{ x, y, x + w, y + S(32) };
    DrawButton(dc, r, icon, text, IsHot(k), style, enabled);
    if (enabled) Zone_Add(r, k);
    return x + w + S(4);
}

static std::wstring DelayText(int ms);

// Kompakter Spinner fuer die CommandBar: Label, [-] Wert [+]. Gibt das rechte Ende zurueck.
static int DrawMiniSpinner(HDC dc, int x, int y, const wchar_t* icon, const std::wstring& label, int id, const std::wstring& value)
{
    int h = S(32);
    RECT ir{ x, y, x + S(26), y + h };
    DrawIcon(dc, ir, icon, CLR_TEXT2, g_fontIconS);
    int lw = TextWidth(dc, g_fontSub, label);
    RECT lr{ ir.right, y, ir.right + lw + S(4), y + h };
    DrawTextIn(dc, lr, label, g_fontSub, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT m{ lr.right, y, lr.right + S(26), y + h };
    RECT v{ m.right, y + S(3), m.right + S(44), y + h - S(3) };
    RECT p{ v.right, y, v.right + S(26), y + h };
    DrawButton(dc, m, ICO_MINUS, L"", IsHot(HK::SetMinus, id), BtnStyle::Subtle);
    DrawRoundBox(dc, v, CLR_CARD, CLR_STROKE2, S(4));
    DrawTextIn(dc, v, value, g_font, CLR_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawButton(dc, p, ICO_ADD, L"", IsHot(HK::SetPlus, id), BtnStyle::Subtle);
    Zone_Add(m, HK::SetMinus, id);
    Zone_Add(p, HK::SetPlus, id);
    return p.right;
}

static void DrawToolbar(HDC dc, int cw)
{
    RECT bar{ 0, TabBarH(), cw, TabBarH() + ToolbarH() };
    FillSolid(dc, bar, CLR_LAYER);
    int x = S(10), y = TabBarH() + S(8);
    Tab* t = CurTab();
    bool hasTab = t != nullptr;

    // Split-Button: links "Neuer Snip" (Capture), rechts Chevron (Verzoegerungsmenue des Tabs)
    {
        int delay = t ? t->delayMs : g_app.def.delayMs;
        std::wstring label = TR(S_NEW_SNIP);
        if (delay > 0) label += L"  ·  " + DelayText(delay);
        int mainW = TextWidth(dc, g_font, label) + S(28) + S(14);
        RECT whole{ x, y, x + mainW + S(30), y + S(32) };
        bool hotMain = IsHot(HK::Snip), hotArrow = IsHot(HK::SnipDelay);
        DrawRoundBox(dc, whole, (hotMain || hotArrow) ? CLR_ACCENT_HOT : CLR_ACCENT, CLR_ACCENT, S(4));
        RECT mainR{ whole.left, whole.top, whole.left + mainW, whole.bottom };
        if (hotMain) {
            HRGN clip = CreateRectRgnIndirect(&mainR); SelectClipRgn(dc, clip); DeleteObject(clip);
            DrawRoundBox(dc, whole, CLR_ACCENT, CLR_ACCENT, S(4));
            SelectClipRgn(dc, NULL);
        }
        DrawIcon(dc, MakeRect(mainR.left, mainR.top, mainR.left + S(32), mainR.bottom), ICO_SNIP, CLR_ON_ACCENT);
        DrawTextIn(dc, MakeRect(mainR.left + S(30), mainR.top, mainR.right - S(8), mainR.bottom), label, g_font, CLR_ON_ACCENT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        FillSolid(dc, MakeRect(mainR.right, whole.top + S(6), mainR.right + 1, whole.bottom - S(6)), CLR_ON_ACCENT);
        RECT arrow{ mainR.right + 1, whole.top, whole.right, whole.bottom };
        DrawIcon(dc, arrow, ICO_CHEVRON, CLR_ON_ACCENT, g_fontIconS);
        Zone_Add(mainR, HK::Snip);
        Zone_Add(arrow, HK::SnipDelay);
        x = whole.right + S(10);
    }
    x = AddCmdButton(dc, x, y, ICO_COPY, TR(S_COPY), HK::Copy, BtnStyle::Subtle, hasTab);
    x = AddCmdButton(dc, x, y, ICO_SAVE, TR(S_SAVE), HK::Save, BtnStyle::Subtle, hasTab);
    x = AddCmdButton(dc, x, y, ICO_RENAME, TR(S_RENAME), HK::Rename, BtnStyle::Subtle, hasTab);

    // rechts: Einstellungen, davor die Tab-eigenen Werte (Fuellfarbe, Rand, Abstand)
    RECT r{ cw - S(10) - S(36), y, cw - S(10), y + S(32) };
    DrawButton(dc, r, ICO_SETTINGS, L"", IsHot(HK::Settings), BtnStyle::Subtle);
    Zone_Add(r, HK::Settings);
    RECT ab{ r.left - S(2) - S(36), y, r.left - S(2), y + S(32) };
    DrawButton(dc, ab, ICO_ABOUT, L"", IsHot(HK::About), BtnStyle::Subtle);
    Zone_Add(ab, HK::About);
    r = ab;
    if (t) {
        // Breite der Gruppe vorab bestimmen, damit sie rechtsbuendig sitzt
        int wSp = S(26) + TextWidth(dc, g_fontSub, TR(S_MARGIN)) + S(4) + S(26) + S(44) + S(26);
        int wSp2 = S(26) + TextWidth(dc, g_fontSub, TR(S_GAP)) + S(4) + S(26) + S(44) + S(26);
        int groupW = S(36) + S(6) + wSp + S(8) + wSp2;
        int gx = r.left - S(14) - groupW;
        if (gx > x + S(4)) {
            // Fuellfarbe
            RECT cr{ gx, y, gx + S(36), y + S(32) };
            bool chot = IsHot(HK::SetColor, SET_TAB_FILL);
            DrawRoundBox(dc, cr, chot ? CLR_CARD_HOT : CLR_CARD, CLR_STROKE2, S(4));
            RECT sw{ cr.left + S(8), cr.top + S(8), cr.right - S(8), cr.bottom - S(8) };
            DrawRoundBox(dc, sw, t->fill, CLR_STROKE3, S(3));
            Zone_Add(cr, HK::SetColor, SET_TAB_FILL);
            int sx = cr.right + S(6);
            sx = DrawMiniSpinner(dc, sx, y, ICO_PADX, TR(S_MARGIN), SET_TAB_PADX, std::to_wstring(t->padX));
            sx = DrawMiniSpinner(dc, sx + S(8), y, ICO_PADY, TR(S_GAP), SET_TAB_PADY, std::to_wstring(t->padY));
            FillSolid(dc, MakeRect(gx - S(8), y + S(6), gx - S(8) + 1, y + S(26)), CLR_STROKE2);
        }
    }
    FillSolid(dc, MakeRect(0, bar.bottom - 1, cw, bar.bottom), CLR_STROKE);
}

// Snippet skaliert zeichnen. Verkleinerungen werden einmal mit HALFTONE in einen
// Cache gerechnet (teuer) und danach nur noch kopiert; Vergroesserungen
// replizieren Pixel. Sonst kostete jede Hover-Aenderung ein komplettes Resampling.
static void BlitSnippet(HDC dc, Snippet& s, int x, int y, double z)
{
    if (!s.bmp) return;
    const RECT v = s.View();
    const int sw = s.VW(), sh = s.VH();
    int w = std::max(1, (int)(sw * z)), h = std::max(1, (int)(sh * z));
    HDC src = CreateCompatibleDC(dc);
    if (z == 1.0) {
        HGDIOBJ o = SelectObject(src, s.bmp);
        BitBlt(dc, x, y, sw, sh, src, v.left, v.top, SRCCOPY);
        SelectObject(src, o);
    }
    else if (z > 1.0) {
        HGDIOBJ o = SelectObject(src, s.bmp);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, x, y, w, h, src, v.left, v.top, sw, sh, SRCCOPY);
        SelectObject(src, o);
    }
    else {
        if (!s.cache || s.cacheZoom != z || s.cacheW != w || s.cacheH != h || !EqualRect(&s.cacheCrop, &v)) {
            s.FreeCache();
            s.cache = Image_CreateDib(w, h);
            if (s.cache) {
                HDC cdc = CreateCompatibleDC(dc);
                HGDIOBJ co = SelectObject(cdc, s.cache);
                HGDIOBJ o = SelectObject(src, s.bmp);
                SetStretchBltMode(cdc, HALFTONE);
                SetBrushOrgEx(cdc, 0, 0, NULL);
                StretchBlt(cdc, 0, 0, w, h, src, v.left, v.top, sw, sh, SRCCOPY);
                SelectObject(src, o);
                SelectObject(cdc, co);
                DeleteDC(cdc);
                s.cacheZoom = z; s.cacheW = w; s.cacheH = h; s.cacheCrop = v;
            }
        }
        if (s.cache) {
            HGDIOBJ o = SelectObject(src, s.cache);
            BitBlt(dc, x, y, w, h, src, 0, 0, SRCCOPY);
            SelectObject(src, o);
        }
        else {
            HGDIOBJ o = SelectObject(src, s.bmp);
            SetStretchBltMode(dc, HALFTONE);
            SetBrushOrgEx(dc, 0, 0, NULL);
            StretchBlt(dc, x, y, w, h, src, v.left, v.top, sw, sh, SRCCOPY);
            SelectObject(src, o);
        }
    }
    DeleteDC(src);
}
// Vollbild ohne Zuschnitt (Editor)
static void BlitSnippetFull(HDC dc, Snippet& s, int x, int y, double z)
{
    RECT saved = s.crop;
    s.crop = RECT{};
    BlitSnippet(dc, s, x, y, z);
    s.crop = saved;
}

static int ComputeRows(Tab* t, const RECT& rc)
{
    g_rows.clear();
    Layout L = Image_ComputeLayout(*t, true);
    double z = g_zoom;
    int STRIP = S(36);
    g_sheetW = std::max(S(320), (int)(L.width * z));
    int areaW = RW(rc) - SbW();
    g_maxScrollX = std::max(0, g_sheetW + 2 * S(16) - areaW);
    t->scrollX = std::max(0, std::min(t->scrollX, g_maxScrollX));
    g_x0 = rc.left + std::max(S(16), (areaW - g_sheetW) / 2) - t->scrollX;
    int y = rc.top + S(16) - t->scroll;
    // Das Layout kennt nur Snippets mit bekannter Groesse; nicht lesbare bekommen eine Platzhalterzeile.
    size_t li = 0;
    for (auto& sp : t->snippets) {
        Snippet& s = *sp;
        Row r{};
        r.top = y;
        r.strip = MakeRect(g_x0, y, g_x0 + g_sheetW, y + STRIP);
        y += STRIP;
        int w, h, ix;
        if (li < L.items.size() && L.items[li].s == &s) {
            w = std::max(1, (int)(s.VW() * z)); h = std::max(1, (int)(s.VH() * z));
            ix = g_x0 + (int)(L.items[li].x * z);
            li++;
        }
        else { w = g_sheetW; h = S(56); ix = g_x0; }
        r.img = MakeRect(ix, y, ix + w, y + h);
        y += h;
        r.bottom = y + (int)(t->padY * z);
        y = r.bottom + S(12);
        g_rows.push_back(r);
    }
    return y + t->scroll - rc.top + S(8) + (g_maxScrollX > 0 ? SbW() : 0);
}

static void ClampScroll(Tab* t)
{
    if (!t) return;
    int maxScroll = std::max(0, g_contentH - RH(g_rcContent));
    t->scroll = std::max(0, std::min(t->scroll, maxScroll));
}

static void DrawStack(HDC dc, Tab* t, const RECT& rc)
{
    FillSolid(dc, rc, CLR_BG);
    if (t->snippets.empty()) {
        g_contentH = 0; g_rows.clear();
        RECT ir{ rc.left, (rc.top + rc.bottom) / 2 - S(70), rc.right, (rc.top + rc.bottom) / 2 - S(20) };
        DrawIcon(dc, ir, ICO_SNIP, CLR_TEXT3, g_fontIconXL);
        std::wstring hint = TRF(S_EMPTY_TAB_FMT, HotkeyName(g_app.def.hotMod, g_app.def.hotVk).c_str());
        RECT tr{ rc.left + S(40), ir.bottom, rc.right - S(40), ir.bottom + S(60) };
        DrawTextIn(dc, tr, hint, g_font, CLR_TEXT2, DT_CENTER | DT_NOPREFIX);
        return;
    }
    const size_t zoneStart = g_zones.size();   // Stapel-Zonen werden am Ende auf rc beschnitten
    g_contentH = ComputeRows(t, rc);
    int before = t->scroll;
    ClampScroll(t);
    if (before != t->scroll) g_contentH = ComputeRows(t, rc);

    HRGN clip = CreateRectRgnIndirect(&rc);
    SelectClipRgn(dc, clip);
    DeleteObject(clip);

    double z = g_zoom;
    for (int i = 0; i < (int)g_rows.size(); i++) {
        Row& r = g_rows[i];
        Snippet& s = *t->snippets[i];
        if (r.bottom + S(20) < rc.top || r.top > rc.bottom) continue;

        // Karte um Kopfzeile + Blatt
        RECT card{ g_x0 - S(1), r.top, g_x0 + g_sheetW + S(1), r.bottom + S(1) };
        bool dragMe = g_dragging && g_pressSnip == i;
        DrawCard(dc, card);
        // Blatt in Tab-Fuellfarbe (Bild + vertikaler Abstand)
        RECT sheet{ g_x0, r.strip.bottom, g_x0 + g_sheetW, r.bottom };
        FillSolid(dc, sheet, t->fill);
        if (s.bmp) {
            BlitSnippet(dc, s, r.img.left, r.img.top, z);
            Annot_RenderView(dc, s, z, r.img.left, r.img.top, s.View());
        }
        else {
            // Datei fehlt oder ist gerade nicht lesbar: Eintrag bleibt erhalten, wird beim naechsten Tab-Wechsel erneut versucht.
            RECT pr = r.img; InflateRect(&pr, -S(8), -S(4));
            DrawIcon(dc, MakeRect(pr.left, pr.top, pr.left + S(28), pr.bottom), ICO_WARN, CLR_DANGER);
            DrawTextIn(dc, MakeRect(pr.left + S(32), pr.top, pr.right, pr.bottom), TRF(S_FILE_MISSING_FMT, s.file.c_str()),
                g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (!s.visible) {
            FillAlpha(dc, sheet, CLR_BG, 200);
            RECT ir{ r.img.left, r.img.top, r.img.right, r.img.bottom };
            if (RH(ir) > S(60)) {
                RECT gi{ ir.left, (ir.top + ir.bottom) / 2 - S(28), ir.right, (ir.top + ir.bottom) / 2 - S(4) };
                DrawIcon(dc, gi, ICO_HIDDEN, CLR_TEXT3, g_fontIconL);
                RECT gt{ ir.left, gi.bottom, ir.right, gi.bottom + S(20) };
                DrawTextIn(dc, gt, TR(S_HIDDEN), g_fontB, CLR_TEXT2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            else DrawTextIn(dc, ir, TR(S_HIDDEN), g_fontB, CLR_TEXT2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        if (dragMe) FrameRectColor(dc, card, CLR_ACCENT, S(2));

        // Kopfzeile
        int cy = (r.strip.top + r.strip.bottom) / 2;
        RECT grip{ r.strip.left + S(6), cy - S(10), r.strip.left + S(26), cy + S(10) };
        DrawIcon(dc, grip, ICO_GRIP, CLR_TEXT3, g_fontIconS);
        RECT chk{ grip.right + S(4), cy - S(10), grip.right + S(4) + S(20), cy + S(10) };
        DrawCheck(dc, chk, s.visible, IsHot(HK::SChk, i));
        Zone_Add(MakeRect(chk.left - S(4), r.strip.top, chk.right + S(4), r.strip.bottom), HK::SChk, i);
        wchar_t lab[96];
        swprintf_s(lab, L"#%d", i + 1);
        RECT lr{ chk.right + S(12), r.strip.top, chk.right + S(12) + S(40), r.strip.bottom };
        DrawTextIn(dc, lr, lab, g_fontB, s.visible ? CLR_TEXT : CLR_TEXT3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (s.w > 0 && s.HasCrop()) swprintf_s(lab, L"%d × %d px  (%d × %d)", s.VW(), s.VH(), s.w, s.h);
        else if (s.w > 0) swprintf_s(lab, L"%d × %d px", s.w, s.h); else wcscpy_s(lab, L"?");
        int labw = TextWidth(dc, g_fontB, L"#" + std::to_wstring(i + 1)) + S(10);
        RECT lr2{ lr.left + labw, r.strip.top, r.strip.right - S(194), r.strip.bottom };
        DrawTextIn(dc, lr2, lab, g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT xr{ r.strip.right - S(40), cy - S(14), r.strip.right - S(6), cy + S(14) };
        bool xhot = IsHot(HK::SDel, i);
        if (xhot) DrawRoundBox(dc, xr, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(4));
        DrawIcon(dc, xr, ICO_DELETE, xhot ? CLR_DANGER : CLR_TEXT2);
        RECT rr{ xr.left - S(36), cy - S(14), xr.left - S(2), cy + S(14) };
        DrawButton(dc, rr, ICO_REPLACE, L"", IsHot(HK::SRep, i), BtnStyle::Subtle);
        RECT er{ rr.left - S(36), cy - S(14), rr.left - S(2), cy + S(14) };
        DrawButton(dc, er, ICO_EDIT, L"", IsHot(HK::SEdit, i), BtnStyle::Subtle);
        // Nur diesen Ausschnitt (mit Ebenen) in die Zwischenablage
        RECT cr{ er.left - S(36), cy - S(14), er.left - S(2), cy + S(14) };
        DrawButton(dc, cr, ICO_COPY, L"", IsHot(HK::SCopy, i), BtnStyle::Subtle);
        // Texterkennung (Windows-OCR) -> Dialog mit editierbarem Text
        RECT orr{ cr.left - S(36), cy - S(14), cr.left - S(2), cy + S(14) };
        DrawButton(dc, orr, ICO_OCR, L"", IsHot(HK::SOcr, i), BtnStyle::Subtle);
        // Zonen in Leserichtung (Tab-Reihenfolge): Text, Kopieren, Bearbeiten, Ersetzen, Loeschen
        Zone_Add(orr, HK::SOcr, i);
        Zone_Add(cr, HK::SCopy, i);
        Zone_Add(er, HK::SEdit, i);
        Zone_Add(rr, HK::SRep, i);
        Zone_Add(xr, HK::SDel, i);
        Zone_Add(MakeRect(card.left, card.top, card.right, card.bottom), HK::SBody, i);
    }

    if (g_dragging && g_dropIdx >= 0 && !g_rows.empty()) {
        int y = (g_dropIdx < (int)g_rows.size()) ? g_rows[g_dropIdx].top - S(7) : g_rows.back().bottom + S(6);
        RECT m{ g_x0 - S(4), y - S(2), g_x0 + g_sheetW + S(4), y + S(2) };
        DrawRoundBox(dc, m, CLR_ACCENT, CLR_ACCENT, S(2));
    }

    // Karten koennen ueber den sichtbaren Bereich hinausragen; ihre Klickzonen duerfen
    // Tab-Leiste, CommandBar, Statusleiste und die Scrollbalken nicht verdecken.
    RECT inner = rc;
    bool vbar = g_contentH > RH(rc), hbar = g_maxScrollX > 0;
    if (vbar) inner.right -= SbW();
    if (hbar) inner.bottom -= SbW();
    for (size_t i = zoneStart; i < g_zones.size(); i++) {
        RECT clipped{};
        if (IntersectRect(&clipped, &g_zones[i].r, &inner)) g_zones[i].r = clipped;
        else g_zones[i].r = RECT{};
    }

    g_thumb = RECT{};
    if (vbar) {
        RECT track{ rc.right - SbW(), rc.top, rc.right, inner.bottom };
        int len = std::max(S(28), (int)((long long)RH(track) * RH(rc) / g_contentH));
        int maxScroll = g_contentH - RH(rc);
        int pos = track.top + S(2) + (int)((long long)t->scroll * (RH(track) - S(4) - len) / maxScroll);
        bool fat = g_thumbDrag || IsHot(HK::ScrollThumb);
        int tw = fat ? S(6) : S(3);
        g_thumb = MakeRect(track.right - S(5) - tw, pos, track.right - S(5), pos + len);
        DrawRoundBox(dc, g_thumb, fat ? CLR_TEXT2 : CLR_STROKE3, fat ? CLR_TEXT2 : CLR_STROKE3, tw / 2);
        Zone_Add(MakeRect(track.right - S(14), pos, track.right, pos + len), HK::ScrollThumb);
        Zone_Add(track, HK::ScrollTrack);
    }
    g_hthumb = RECT{};
    if (hbar) {
        RECT track{ rc.left, rc.bottom - SbW(), inner.right, rc.bottom };
        int totalW = g_sheetW + 2 * S(16);
        int len = std::max(S(28), (int)((long long)RW(track) * RW(track) / totalW));
        int pos = track.left + S(2) + (int)((long long)t->scrollX * (RW(track) - S(4) - len) / g_maxScrollX);
        bool fat = g_hthumbDrag || IsHot(HK::ScrollHThumb);
        int th = fat ? S(6) : S(3);
        g_hthumb = MakeRect(pos, track.bottom - S(5) - th, pos + len, track.bottom - S(5));
        DrawRoundBox(dc, g_hthumb, fat ? CLR_TEXT2 : CLR_STROKE3, fat ? CLR_TEXT2 : CLR_STROKE3, th / 2);
        Zone_Add(MakeRect(pos, track.bottom - S(14), pos + len, track.bottom), HK::ScrollHThumb);
        Zone_Add(track, HK::ScrollHTrack);
    }
    SelectClipRgn(dc, NULL);
}

static void DrawDashboard(HDC dc, const RECT& rc)
{
    FillSolid(dc, rc, CLR_BG);
    int x = rc.left + S(32), y = rc.top + S(28);
    int cardW = std::min(RW(rc) - S(64), S(820));
    // Hero
    RECT hi{ x, y, x + S(48), y + S(48) };
    DrawIcon(dc, hi, ICO_SNIP, CLR_ACCENT, g_fontIconXL);
    DrawTextIn(dc, MakeRect(hi.right + S(14), y, x + cardW, y + S(26)), TR(S_WELCOME), g_fontH, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextIn(dc, MakeRect(hi.right + S(14), y + S(26), x + cardW, y + S(48)),
        TR(S_TAGLINE), g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += S(68);

    struct Step { const wchar_t* icon; std::wstring text; };
    std::wstring hk = HotkeyName(g_app.def.hotMod, g_app.def.hotVk);
    Step steps[] = {
        { ICO_ADD,      TR(S_STEP1) },
        { ICO_SNIP,     TRF(S_STEP2_FMT, hk.c_str()) },
        { ICO_CHECK,    TR(S_STEP3) },
        { ICO_GRIP,     TR(S_STEP4) },
        { ICO_EDIT,     TR(S_STEP5) },
        { ICO_COPY,     TR(S_STEP6) },
        { ICO_ZOOMIN,   TR(S_STEP7) },
        { ICO_SETTINGS, TR(S_STEP8) },
    };
    RECT card{ x, y, x + cardW, y };
    int innerW = cardW - S(72);
    int cy = y + S(10);
    HGDIOBJ of = SelectObject(dc, g_font);
    std::vector<int> heights;
    for (auto& st : steps) {
        RECT calc{ 0, 0, innerW, 0 };
        DrawTextW(dc, st.text.c_str(), -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        heights.push_back(std::max(S(24), RH(calc)));
    }
    SelectObject(dc, of);
    int total = S(20);
    for (int h : heights) total += h + S(12);
    card.bottom = y + total;
    DrawCard(dc, card);
    for (size_t i = 0; i < heights.size(); i++) {
        RECT ir{ x + S(16), cy, x + S(48), cy + S(24) };
        DrawIcon(dc, ir, steps[i].icon, CLR_ACCENT);
        RECT tr{ x + S(56), cy + S(2), x + S(56) + innerW, cy + S(2) + heights[i] };
        DrawTextIn(dc, tr, steps[i].text, g_font, CLR_TEXT, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        cy += heights[i] + S(12);
        if (i + 1 < heights.size()) FillSolid(dc, MakeRect(x + S(56), cy - S(6), card.right - S(16), cy - S(6) + 1), CLR_STROKE);
    }
    y = card.bottom + S(16);
    if (y + S(36) > rc.bottom) y = rc.bottom - S(44);
    RECT br{ x, y, x + S(190), y + S(34) };
    DrawButton(dc, br, ICO_ADD, TR(S_NEW_TAB_BTN), IsHot(HK::DashNew), BtnStyle::Accent);
    Zone_Add(br, HK::DashNew);
}

static std::wstring DelayText(int ms)
{
    if (ms == 0) return TR(S_NONE);
    wchar_t b[32];
    const wchar_t* sep = (Lang_Effective() == Lang::En) ? L"." : L",";   // Dezimaltrennzeichen der Sprache
    swprintf_s(b, L"%d%s%d s", ms / 1000, sep, (ms % 1000) / 100);
    return b;
}

// Einstellungs-Karte: Icon, Titel, Beschreibung links; Steuerelement rechts. Gibt y fuer die naechste Karte zurueck.
static int g_setCardW = 0;
static RECT SetCard(HDC dc, int& y, const wchar_t* icon, const std::wstring& title, const std::wstring& desc)
{
    RECT card{ S(32), y, S(32) + g_setCardW, y + S(62) };
    DrawCard(dc, card);
    RECT ir{ card.left + S(16), card.top, card.left + S(44), card.bottom };
    DrawIcon(dc, ir, icon, CLR_TEXT, g_fontIconL);
    RECT tr{ card.left + S(56), card.top + S(12), card.right - S(300), card.top + S(32) };
    DrawTextIn(dc, tr, title, g_font, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    RECT dr{ card.left + S(56), card.top + S(32), card.right - S(300), card.top + S(52) };
    DrawTextIn(dc, dr, desc, g_fontSub, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    y = card.bottom + S(6);
    return card;
}
static void SetSpinner(HDC dc, const RECT& card, int id, const std::wstring& value)
{
    int cy = (card.top + card.bottom) / 2;
    RECT p{ card.right - S(16) - S(32), cy - S(16), card.right - S(16), cy + S(16) };
    RECT v{ p.left - S(4) - S(84), cy - S(16), p.left - S(4), cy + S(16) };
    RECT m{ v.left - S(4) - S(32), cy - S(16), v.left - S(4), cy + S(16) };
    DrawButton(dc, m, ICO_MINUS, L"", IsHot(HK::SetMinus, id), BtnStyle::Subtle);
    DrawRoundBox(dc, v, CLR_CARD, CLR_STROKE2, S(4));
    FillSolid(dc, MakeRect(v.left + S(4), v.bottom - 1, v.right - S(4), v.bottom), CLR_STROKE3);
    DrawTextIn(dc, v, value, g_font, CLR_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawButton(dc, p, ICO_ADD, L"", IsHot(HK::SetPlus, id), BtnStyle::Subtle);
    Zone_Add(m, HK::SetMinus, id);
    Zone_Add(p, HK::SetPlus, id);
}
static void SetColorCtl(HDC dc, const RECT& card, int id, COLORREF c)
{
    int cy = (card.top + card.bottom) / 2;
    RECT r{ card.right - S(16) - S(160), cy - S(16), card.right - S(16), cy + S(16) };
    bool hot = IsHot(HK::SetColor, id);
    DrawRoundBox(dc, r, hot ? CLR_CARD_HOT : CLR_CARD, CLR_STROKE2, S(4));
    RECT sw{ r.left + S(6), r.top + S(6), r.left + S(6) + S(20), r.bottom - S(6) };
    DrawRoundBox(dc, sw, c, CLR_STROKE3, S(3));
    wchar_t b[16];
    swprintf_s(b, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    DrawTextIn(dc, MakeRect(sw.right + S(10), r.top, r.right - S(30), r.bottom), b, g_font, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawIcon(dc, MakeRect(r.right - S(30), r.top, r.right, r.bottom), ICO_COLOR, CLR_TEXT2, g_fontIconS);
    Zone_Add(r, HK::SetColor, id);
}
static void SetToggleCtl(HDC dc, const RECT& card, int id, bool on)
{
    int cy = (card.top + card.bottom) / 2;
    RECT r{ card.right - S(16) - S(44), cy - S(10), card.right - S(16), cy + S(10) };
    DrawToggle(dc, r, on, IsHot(HK::SetToggle, id));
    RECT lab{ r.left - S(60), cy - S(12), r.left - S(10), cy + S(12) };
    DrawTextIn(dc, lab, on ? TR(S_ON) : TR(S_OFF), g_font, CLR_TEXT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    Zone_Add(MakeRect(lab.left, card.top, r.right, card.bottom), HK::SetToggle, id);
}

static const wchar_t* AutoCopyName(int v) { return v == 1 ? TR(S_AC_SNIP) : (v == 2 ? TR(S_AC_TAB) : TR(S_AC_OFF)); }
static void SetAutoCopyCtl(HDC dc, const RECT& card, int base, int value)
{
    int cy = (card.top + card.bottom) / 2;
    int right = card.right - S(16);
    for (int i = 2; i >= 0; i--) {
        int w = TextWidth(dc, g_font, AutoCopyName(i)) + S(22);
        RECT r{ right - w, cy - S(16), right, cy + S(16) };
        DrawButton(dc, r, nullptr, AutoCopyName(i), IsHot(HK::SetAutoCopy, base + i), value == i ? BtnStyle::Accent : BtnStyle::Subtle);
        Zone_Add(r, HK::SetAutoCopy, base + i);
        right = r.left - S(4);
    }
}

static const wchar_t* NumScopeName(int v) { return v == 1 ? TR(S_NS_TAB) : TR(S_NS_SNIP); }
static void SetNumScopeCtl(HDC dc, const RECT& card, int base, int value)
{
    int cy = (card.top + card.bottom) / 2;
    int right = card.right - S(16);
    for (int i = 1; i >= 0; i--) {
        int w = TextWidth(dc, g_font, NumScopeName(i)) + S(22);
        RECT r{ right - w, cy - S(16), right, cy + S(16) };
        DrawButton(dc, r, nullptr, NumScopeName(i), IsHot(HK::SetNumScope, base + i), value == i ? BtnStyle::Accent : BtnStyle::Subtle);
        Zone_Add(r, HK::SetNumScope, base + i);
        right = r.left - S(4);
    }
}

static int g_setScroll = 0;      // Scroll-Offset der Einstellungsseite
static int g_setContentH = 0;

static void DrawSettings(HDC dc, const RECT& rc)
{
    FillSolid(dc, rc, CLR_BG);
    int cw = rc.right;
    g_setCardW = std::min(cw - S(64), S(860));
    const int headH = S(60);
    RECT body{ rc.left, rc.top + headH, rc.right, rc.bottom };
    int maxScroll = std::max(0, g_setContentH - RH(body));
    g_setScroll = std::max(0, std::min(g_setScroll, maxScroll));
    HRGN clip = CreateRectRgnIndirect(&body);
    SelectClipRgn(dc, clip);
    DeleteObject(clip);

    int y = body.top + S(4) - g_setScroll;
    const int yStart = y;
    DrawTextIn(dc, MakeRect(S(32), y, cw, y + S(24)), TR(S_SET_DEFAULTS_HDR), g_fontB, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += S(30);
    const Defaults& d = g_app.def;
    RECT c;
    {
        // Sprache: Automatisch / Deutsch / English / Español als Segment-Schalter
        c = SetCard(dc, y, L"\xE8C1", TR(S_SET_LANG), TR(S_SET_LANG_D));
        int cy = (c.top + c.bottom) / 2;
        const wchar_t* names[4] = { TR(S_LANG_AUTO), L"Deutsch", L"English", L"Español" };
        int right = c.right - S(16);
        for (int i = 3; i >= 0; i--) {
            int w = TextWidth(dc, g_font, names[i]) + S(22);
            RECT r{ right - w, cy - S(16), right, cy + S(16) };
            bool sel = (int)Lang_Get() == i;
            DrawButton(dc, r, nullptr, names[i], IsHot(HK::SetLang, i), sel ? BtnStyle::Accent : BtnStyle::Subtle);
            Zone_Add(r, HK::SetLang, i);
            right = r.left - S(4);
        }
    }
    {
        c = SetCard(dc, y, L"\xE790", TR(S_SET_THEME), TR(S_SET_THEME_D));
        int cy = (c.top + c.bottom) / 2;
        const wchar_t* names[3] = { TR(S_LANG_AUTO), TR(S_THEME_LIGHT), TR(S_THEME_DARK) };
        int right = c.right - S(16);
        for (int i = 2; i >= 0; i--) {
            int w = TextWidth(dc, g_font, names[i]) + S(22);
            RECT r{ right - w, cy - S(16), right, cy + S(16) };
            DrawButton(dc, r, nullptr, names[i], IsHot(HK::SetTheme, i), g_app.def.theme == i ? BtnStyle::Accent : BtnStyle::Subtle);
            Zone_Add(r, HK::SetTheme, i);
            right = r.left - S(4);
        }
    }
    c = SetCard(dc, y, ICO_COLOR, TR(S_SET_FILL), TR(S_SET_FILL_D));
    SetColorCtl(dc, c, SET_DEF_FILL, d.fill);
    c = SetCard(dc, y, ICO_PADX, TR(S_SET_PADX), TR(S_SET_PADX_D));
    SetSpinner(dc, c, SET_DEF_PADX, std::to_wstring(d.padX) + L" px");
    c = SetCard(dc, y, ICO_PADY, TR(S_SET_PADY), TR(S_SET_PADY_D));
    SetSpinner(dc, c, SET_DEF_PADY, std::to_wstring(d.padY) + L" px");
    c = SetCard(dc, y, ICO_TIMER, TR(S_SET_DELAY), TR(S_SET_DELAY_D));
    SetSpinner(dc, c, SET_DELAY, DelayText(d.delayMs));
    c = SetCard(dc, y, ICO_COPY, TR(S_SET_AUTOCOPY), TR(S_SET_AUTOCOPY_D));
    SetAutoCopyCtl(dc, c, 0, d.autoCopy);
    c = SetCard(dc, y, ICO_EDIT, TR(S_SET_NUMSCOPE), TR(S_SET_NUMSCOPE_D));
    SetNumScopeCtl(dc, c, 0, d.numberScope);
    c = SetCard(dc, y, ICO_MOUSE, TR(S_SET_CURSOR), TR(S_SET_CURSOR_D));
    SetToggleCtl(dc, c, SET_CURSOR, d.captureCursor);
    {
        c = SetCard(dc, y, ICO_KEYBOARD, TR(S_SET_HOTKEY), g_hotkeyRegistered ? TR(S_SET_HOTKEY_OK) : TR(S_SET_HOTKEY_BAD));
        int cy = (c.top + c.bottom) / 2;
        RECT r{ c.right - S(16) - S(220), cy - S(16), c.right - S(16), cy + S(16) };
        std::wstring txt = g_hotkeyCapture ? TR(S_PRESS_KEYS) : HotkeyName(d.hotMod, d.hotVk);
        bool hot = g_hotkeyCapture || IsHot(HK::SetHotkey);
        DrawRoundBox(dc, r, g_hotkeyCapture ? g_th.captureBg : (hot ? CLR_CARD_HOT : CLR_CARD), hot ? CLR_ACCENT : CLR_STROKE2, S(4));
        FillSolid(dc, MakeRect(r.left + S(4), r.bottom - 1, r.right - S(4), r.bottom), hot ? CLR_ACCENT : CLR_STROKE3);
        DrawTextIn(dc, r, txt, g_font, CLR_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!g_hotkeyRegistered) DrawIcon(dc, MakeRect(r.left - S(34), r.top, r.left - S(6), r.bottom), ICO_WARN, CLR_DANGER);
        Zone_Add(r, HK::SetHotkey);
    }
    c = SetCard(dc, y, ICO_HOME, TR(S_SET_DASH), TR(S_SET_DASH_D));
    SetToggleCtl(dc, c, SET_DASH, d.dashboard);
    {
        c = SetCard(dc, y, ICO_DELETE, TR(S_SET_ASK), d.noAskDelete ? TR(S_SET_ASK_OFF) : TR(S_SET_ASK_ON));
        int cy = (c.top + c.bottom) / 2;
        if (d.noAskDelete) {
            RECT r{ c.right - S(16) - S(190), cy - S(16), c.right - S(16), cy + S(16) };
            DrawButton(dc, r, ICO_REPLACE, TR(S_REENABLE), IsHot(HK::SetResetAsk), BtnStyle::Standard);
            Zone_Add(r, HK::SetResetAsk);
        }
        else DrawIcon(dc, MakeRect(c.right - S(16) - S(32), cy - S(16), c.right - S(16), cy + S(16)), ICO_CHECK, CLR_ACCENT);
    }

    Tab* t = CurTab();
    if (t) {
        y += S(10);
        DrawTextIn(dc, MakeRect(S(32), y, cw, y + S(24)), TR(S_ACTIVE_TAB) + t->title, g_fontB, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y += S(30);
        c = SetCard(dc, y, ICO_COLOR, TR(S_TAB_FILL), TR(S_TAB_ONLY));
        SetColorCtl(dc, c, SET_TAB_FILL, t->fill);
        c = SetCard(dc, y, ICO_PADX, TR(S_TAB_PADX), TR(S_TAB_PADX_D));
        SetSpinner(dc, c, SET_TAB_PADX, std::to_wstring(t->padX) + L" px");
        c = SetCard(dc, y, ICO_PADY, TR(S_TAB_PADY), TR(S_TAB_PADY_D));
        SetSpinner(dc, c, SET_TAB_PADY, std::to_wstring(t->padY) + L" px");
        c = SetCard(dc, y, ICO_TIMER, TR(S_TAB_DELAY), TR(S_TAB_DELAY_D));
        SetSpinner(dc, c, SET_TAB_DELAY, DelayText(t->delayMs));
        c = SetCard(dc, y, ICO_COPY, TR(S_TAB_AUTOCOPY), TR(S_TAB_ONLY));
        SetAutoCopyCtl(dc, c, 10, t->autoCopy);
        c = SetCard(dc, y, ICO_EDIT, TR(S_TAB_NUMSCOPE), TR(S_TAB_ONLY));
        SetNumScopeCtl(dc, c, 10, t->numberScope);
    }
    y += S(4);
    RECT fr{ S(32), y, S(32) + g_setCardW, y + S(24) };
    DrawIcon(dc, MakeRect(fr.left, fr.top, fr.left + S(24), fr.bottom), ICO_FOLDER, CLR_TEXT2, g_fontIconS);
    DrawTextIn(dc, MakeRect(fr.left + S(28), fr.top, fr.right, fr.bottom), g_app.root, g_fontSub, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    y = fr.bottom + S(16);
    g_setContentH = y - yStart;

    // Scrollbalken rechts
    if (g_setContentH > RH(body)) {
        int len = std::max(S(28), (int)((long long)RH(body) * RH(body) / g_setContentH));
        int pos = body.top + S(2) + (int)((long long)g_setScroll * (RH(body) - S(4) - len) / std::max(1, g_setContentH - RH(body)));
        RECT th{ body.right - S(8), pos, body.right - S(5), pos + len };
        DrawRoundBox(dc, th, CLR_STROKE3, CLR_STROKE3, S(1));
    }
    SelectClipRgn(dc, NULL);

    // Kopfzeile oben drueber (deckt gescrollte Karten ab)
    RECT head{ rc.left, rc.top, rc.right, rc.top + headH };
    FillSolid(dc, head, CLR_BG);
    RECT back{ S(28), rc.top + S(14), S(28) + S(36), rc.top + S(14) + S(32) };
    DrawButton(dc, back, ICO_BACK, L"", IsHot(HK::SetBack), BtnStyle::Subtle);
    Zone_Add(back, HK::SetBack);
    DrawTextIn(dc, MakeRect(back.right + S(10), rc.top + S(10), cw / 2, rc.top + S(50)), TR(S_SETTINGS), g_fontH, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_setScroll > 0) FillSolid(dc, MakeRect(0, head.bottom - 1, cw, head.bottom), CLR_STROKE);
    // Zonen, die unter der Kopfzeile liegen, beschneiden (sonst waeren verdeckte Karten klickbar).
    for (auto& z : g_zones) {
        if (z.kind == HK::SetBack || z.kind == HK::Tab || z.kind == HK::TabClose || z.kind == HK::TabAdd || z.kind == HK::Dash) continue;
        if (z.r.top < head.bottom) z.r.top = head.bottom;
        if (z.r.top >= z.r.bottom) z.r = RECT{};
    }
}

static void DrawStatus(HDC dc, int cw, int ch)
{
    RECT bar{ 0, ch - StatusH(), cw, ch };
    FillSolid(dc, bar, CLR_LAYER);
    FillSolid(dc, MakeRect(0, bar.top, cw, bar.top + 1), CLR_STROKE);
    std::wstring left;
    const wchar_t* icon = ICO_INFO;
    if (!g_flash.empty()) { left = g_flash; icon = ICO_ACCEPT; }
    else if (Tab* t = CurTab()) {
        Layout L = Image_ComputeLayout(*t, false);
        wchar_t b[160];
        swprintf_s(b, TR(S_STATUS_FMT), (int)t->snippets.size(), (int)L.items.size(), L.width, L.height);
        left = b; icon = ICO_IMAGE;
        int unsaved = 0;
        for (auto& s : t->snippets) if (s->unsaved) unsaved++;
        if (unsaved) { left += TRF(S_UNSAVED_FMT, unsaved); icon = ICO_WARN; }
    }
    else { left = TR(S_HOTKEY_LBL) + HotkeyName(g_app.def.hotMod, g_app.def.hotVk); icon = ICO_KEYBOARD; }
    DrawIcon(dc, MakeRect(S(10), bar.top, S(34), ch), icon, g_flash.empty() ? CLR_TEXT2 : CLR_ACCENT, g_fontIconS);
    DrawTextIn(dc, MakeRect(S(38), bar.top, cw - S(220), ch), left, g_font, g_flash.empty() ? CLR_TEXT2 : CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    int y = bar.top + S(3), h = S(24);
    RECT plus{ cw - S(10) - S(28), y, cw - S(10), y + h };
    RECT val{ plus.left - S(2) - S(60), y, plus.left - S(2), y + h };
    RECT minus{ val.left - S(2) - S(28), y, val.left - S(2), y + h };
    DrawButton(dc, minus, ICO_ZOOMOUT, L"", IsHot(HK::ZoomOut), BtnStyle::Subtle);
    wchar_t zb[16];
    swprintf_s(zb, L"%d %%", (int)(g_zoom * 100 + 0.5));
    DrawButton(dc, val, nullptr, zb, IsHot(HK::ZoomReset), BtnStyle::Subtle);
    DrawButton(dc, plus, ICO_ZOOMIN, L"", IsHot(HK::ZoomIn), BtnStyle::Subtle);
    Zone_Add(minus, HK::ZoomOut);
    Zone_Add(val, HK::ZoomReset);
    Zone_Add(plus, HK::ZoomIn);
}

static int g_aboutScroll = 0;
static int g_aboutContentH = 0;

static const wchar_t* kMitText =
    L"MIT License\n\n"
    L"Copyright (c) 2026 bjwe\n\n"
    L"Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated "
    L"documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation "
    L"the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to "
    L"permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\n"
    L"The above copyright notice and this permission notice shall be included in all copies or substantial portions of "
    L"the Software.\n\n"
    L"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO "
    L"THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
    L"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, "
    L"TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";

static void DrawAbout(HDC dc, const RECT& rc)
{
    FillSolid(dc, rc, CLR_BG);
    int cw = rc.right;
    int cardW = std::min(cw - S(64), S(760));
    const int headH = S(60);
    RECT body{ rc.left, rc.top + headH, rc.right, rc.bottom };
    int maxScroll = std::max(0, g_aboutContentH - RH(body));
    g_aboutScroll = std::max(0, std::min(g_aboutScroll, maxScroll));
    HRGN clip = CreateRectRgnIndirect(&body);
    SelectClipRgn(dc, clip);
    DeleteObject(clip);

    int x = S(32);
    int y = body.top + S(8) - g_aboutScroll;
    const int yStart = y;

    // Hero: Icon, Name, Version, Einzeiler
    RECT hi{ x, y, x + S(56), y + S(56) };
    DrawIcon(dc, hi, ICO_SNIP, CLR_ACCENT, g_fontIconXL);
    DrawTextIn(dc, MakeRect(hi.right + S(16), y, x + cardW, y + S(28)), L"MultiSnip", g_fontH, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextIn(dc, MakeRect(hi.right + S(16), y + S(28), x + cardW, y + S(50)),
        TRF(S_VERSION_LINE_FMT, kAppVersion), g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += S(72);

    // Karte: Freie Software + GitHub
    {
        RECT card{ x, y, x + cardW, y + S(96) };
        DrawCard(dc, card);
        RECT ir{ card.left + S(16), card.top + S(14), card.left + S(44), card.top + S(42) };
        DrawIcon(dc, ir, ICO_CODE, CLR_TEXT, g_fontIconL);
        DrawTextIn(dc, MakeRect(card.left + S(56), card.top + S(12), card.right - S(16), card.top + S(32)),
            TR(S_FREE_SW), g_fontB, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextIn(dc, MakeRect(card.left + S(56), card.top + S(32), card.right - S(16), card.top + S(52)),
            TR(S_FREE_SW_D),
            g_fontSub, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        RECT gb{ card.left + S(56), card.top + S(56), card.left + S(56) + S(230), card.top + S(86) };
        DrawButton(dc, gb, ICO_LINK, L"github.com/bjwe/multisnip", IsHot(HK::AboutGitHub), BtnStyle::Standard);
        Zone_Add(gb, HK::AboutGitHub);
        y = card.bottom + S(10);
    }

    // Karte: Lizenztext
    {
        int innerW = cardW - S(32);
        RECT calc{ 0, 0, innerW, 0 };
        HGDIOBJ of = SelectObject(dc, g_fontSub);
        DrawTextW(dc, kMitText, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, of);
        RECT card{ x, y, x + cardW, y + RH(calc) + S(32) };
        DrawCard(dc, card);
        DrawTextIn(dc, MakeRect(card.left + S(16), card.top + S(16), card.right - S(16), card.bottom - S(16)),
            kMitText, g_fontSub, CLR_TEXT2, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        y = card.bottom + S(16);
    }
    g_aboutContentH = y - yStart;

    if (g_aboutContentH > RH(body)) {
        int len = std::max(S(28), (int)((long long)RH(body) * RH(body) / g_aboutContentH));
        int pos = body.top + S(2) + (int)((long long)g_aboutScroll * (RH(body) - S(4) - len) / std::max(1, g_aboutContentH - RH(body)));
        RECT th{ body.right - S(8), pos, body.right - S(5), pos + len };
        DrawRoundBox(dc, th, CLR_STROKE3, CLR_STROKE3, S(1));
    }
    SelectClipRgn(dc, NULL);

    RECT head{ rc.left, rc.top, rc.right, rc.top + headH };
    FillSolid(dc, head, CLR_BG);
    RECT back{ S(28), rc.top + S(14), S(28) + S(36), rc.top + S(14) + S(32) };
    DrawButton(dc, back, ICO_BACK, L"", IsHot(HK::SetBack), BtnStyle::Subtle);
    Zone_Add(back, HK::SetBack);
    DrawTextIn(dc, MakeRect(back.right + S(10), rc.top + S(10), cw / 2, rc.top + S(50)), TR(S_ABOUT_TITLE), g_fontH, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_aboutScroll > 0) FillSolid(dc, MakeRect(0, head.bottom - 1, cw, head.bottom), CLR_STROKE);
    for (auto& z : g_zones) {
        if (z.kind != HK::AboutGitHub) continue;
        if (z.r.top < head.bottom) z.r.top = head.bottom;
        if (z.r.top >= z.r.bottom) z.r = RECT{};
    }
}

// ---------------------------------------------------------------- Editor (Annotations-Ebenen)

enum class Tool { Select = 0, Pen, Marker, Line, Arrow, Rect, Ellipse, Text, Number, Crop };

static int g_edSnip = -1;                 // Index des bearbeiteten Snippets im aktiven Tab
static Tool g_tool = Tool::Pen;
static COLORREF g_annColor = RGB(232, 17, 35);
static int g_annWidth = 3;
static double g_edZoom = 1.0;
static int g_edSx = 0, g_edSy = 0;        // Scroll im Canvas
static RECT g_edCanvas{};
static int g_edOx = 0, g_edOy = 0;        // Bildursprung in Client-Pixeln
static int g_curLayer = 0;
static bool g_edDrawing = false, g_edMoving = false;
static Shape g_edShape;                   // Form im Entstehen
static int g_edSel = -1;                  // ausgewaehlte Form in der aktuellen Ebene
static AnnPoint g_edLast{};
static std::vector<std::vector<Layer>> g_undo, g_redo;
static bool g_edTextActive = false;
static int g_edTextIdx = -1;
static int g_editLayer = -1;              // Inline-Umbenennen einer Ebene (nutzt g_editText)

static const COLORREF kPalette[] = {
    RGB(232, 17, 35), RGB(247, 99, 12), RGB(255, 185, 0), RGB(16, 124, 16),
    RGB(0, 120, 212), RGB(136, 23, 152), RGB(27, 27, 27), RGB(255, 255, 255) };
static const int kPaletteCount = 8;
static const int kWidths[] = { 1, 3, 6 };
static const struct { Tool tool; const wchar_t* icon; StrId name; wchar_t key; } kTools[] = {
    { Tool::Select,  L"\xE7C9", S_TOOL_SELECT, L'V' },
    { Tool::Pen,     L"\xE76D", S_TOOL_PEN, L'P' },
    { Tool::Marker,  L"\xE7E6", S_TOOL_MARKER, L'M' },
    { Tool::Line,    L"\xE738", S_TOOL_LINE, L'L' },
    { Tool::Arrow,   L"\xE72A", S_TOOL_ARROW, L'A' },
    { Tool::Rect,    L"\xE739", S_TOOL_RECT, L'R' },
    { Tool::Ellipse, L"\xEA3A", S_TOOL_ELLIPSE, L'E' },
    { Tool::Text,    L"\xE8D2", S_TOOL_TEXT, L'T' },
    { Tool::Number,  nullptr,   S_TOOL_NUMBER, L'N' },
    { Tool::Crop,    L"\xE7A8", S_TOOL_CROP, L'C' },
};
static const int kToolCount = 10;
// Zuschnitt-Werkzeug: Rechteck wird aufgezogen, Ergebnis landet in Snippet::crop (INI), das PNG bleibt.
static bool g_cropDrag = false;
static AnnPoint g_cropA{}, g_cropB{};

static Snippet* EdSnip()
{
    Tab* t = CurTab();
    if (!t || g_edSnip < 0 || g_edSnip >= (int)t->snippets.size()) return nullptr;
    return t->snippets[g_edSnip].get();
}
static Layer* EdLayer()
{
    Snippet* s = EdSnip();
    if (!s || s->layers.empty()) return nullptr;
    g_curLayer = std::max(0, std::min(g_curLayer, (int)s->layers.size() - 1));
    return &s->layers[g_curLayer];
}
static void EdSave()
{
    Tab* t = CurTab();
    Snippet* s = EdSnip();
    if (t && s) Model_SaveAnnot(*t, *s);
}
// Hoechste bereits vergebene Nummer - je nach Tab-Einstellung nur in diesem
// Ausschnitt oder ueber alle Ausschnitte des Tabs.
static int MaxNumber(const Tab& t, const Snippet& s)
{
    int m = 0;
    auto scan = [&m](const Snippet& sn) {
        for (auto& L : sn.layers)
            for (auto& sh : L.shapes)
                if (sh.type == ShapeType::Number) m = std::max(m, sh.number);
    };
    if (t.numberScope == 1) { for (auto& p : t.snippets) scan(*p); }
    else scan(s);
    return m;
}
// Nummer, die die naechste Marke bekommt. Wird nach jeder Aenderung aus dem
// Bestand neu berechnet; der Spinner in der Werkzeugleiste darf sie bis zum
// naechsten Setzen ueberschreiben.
static int g_edNumNext = 1;
static void EdNumRefresh()
{
    Tab* t = CurTab();
    Snippet* s = EdSnip();
    g_edNumNext = (t && s) ? MaxNumber(*t, *s) + 1 : 1;
}
static void PushUndo()
{
    Snippet* s = EdSnip();
    if (!s) return;
    g_undo.push_back(s->layers);
    if (g_undo.size() > 60) g_undo.erase(g_undo.begin());
    g_redo.clear();
}
static void EdUndo()
{
    Snippet* s = EdSnip();
    if (!s || g_undo.empty()) return;
    g_redo.push_back(s->layers);
    s->layers = g_undo.back();
    g_undo.pop_back();
    g_edSel = -1; g_edTextActive = false; g_edTextIdx = -1;
    EdNumRefresh();
    EdSave();
    Redraw();
}
static void EdRedo()
{
    Snippet* s = EdSnip();
    if (!s || g_redo.empty()) return;
    g_undo.push_back(s->layers);
    s->layers = g_redo.back();
    g_redo.pop_back();
    g_edSel = -1; g_edTextActive = false; g_edTextIdx = -1;
    EdNumRefresh();
    EdSave();
    Redraw();
}
static void EdCommitText(bool apply)
{
    if (!g_edTextActive) return;
    Layer* L = EdLayer();
    if (L && g_edTextIdx >= 0 && g_edTextIdx < (int)L->shapes.size()) {
        if (!apply || L->shapes[g_edTextIdx].text.empty()) L->shapes.erase(L->shapes.begin() + g_edTextIdx);
    }
    g_edTextActive = false;
    g_edTextIdx = -1;
    EdSave();
    Redraw();
}
static void CommitLayerRename(bool apply)
{
    if (g_editLayer < 0) return;
    Snippet* s = EdSnip();
    if (apply && s && g_editLayer < (int)s->layers.size()) {
        std::wstring n = g_editText;
        while (!n.empty() && iswspace(n.back())) n.pop_back();
        size_t b = 0; while (b < n.size() && iswspace(n[b])) b++;
        n = n.substr(b);
        if (!n.empty()) { s->layers[g_editLayer].name = n; EdSave(); }
    }
    g_editLayer = -1;
    Redraw();
}
static void StartLayerRename(int idx)
{
    Snippet* s = EdSnip();
    if (!s || idx < 0 || idx >= (int)s->layers.size()) return;
    EdCommitText(true);
    g_editLayer = idx;
    g_editText = s->layers[idx].name;
    g_editSelectAll = true;
    g_curLayer = idx;
    Redraw();
}
static void EdSetZoom(double z)
{
    z = std::max(0.1, std::min(8.0, z));
    if (fabs(z - 1.0) < 0.04) z = 1.0;
    g_edZoom = z;
    Redraw();
}
static void EdFitZoom()
{
    Snippet* s = EdSnip();
    if (!s || !g_hwnd) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    int availW = RW(rc) - S(240) - S(48), availH = RH(rc) - S(48) - StatusH() - S(48);
    double z = std::min(1.0, std::min((double)availW / s->w, (double)availH / s->h));
    g_edZoom = std::max(0.1, z);
    g_edSx = g_edSy = 0;
}
static void OpenEditor(int idx)
{
    Tab* t = CurTab();
    if (!t || idx < 0 || idx >= (int)t->snippets.size()) return;
    if (g_mouseDown || g_dragging) { g_mouseDown = false; g_dragging = false; ReleaseCapture(); }
    Snippet& s = *t->snippets[idx];
    if (!s.bmp) { Flash(TRF(S_FILE_MISSING_FMT, s.file.c_str())); return; }
    g_editTab = -1;
    g_edSnip = idx;
    if (s.layers.empty()) { Layer L; L.name = TRF(S_LAYER_N_FMT, 1); s.layers.push_back(L); }
    g_curLayer = (int)s.layers.size() - 1;
    g_edSel = -1;
    g_edDrawing = g_edMoving = false;
    g_edTextActive = false; g_edTextIdx = -1;
    g_editLayer = -1;
    g_undo.clear(); g_redo.clear();
    EdNumRefresh();
    g_page = Page::Edit;
    EdFitZoom();
    Redraw();
}
static void CloseEditor()
{
    if (g_page != Page::Edit) return;
    EdCommitText(true);
    CommitLayerRename(true);
    if (g_edDrawing || g_edMoving || g_cropDrag) { g_edDrawing = g_edMoving = g_cropDrag = false; ReleaseCapture(); }
    EdSave();
    g_page = Page::Main;
    g_edSnip = -1;
    Redraw();
}
static AnnPoint ToImg(POINT p)
{
    AnnPoint a;
    a.x = (float)((p.x - g_edOx) / g_edZoom);
    a.y = (float)((p.y - g_edOy) / g_edZoom);
    return a;
}
static Shape* EdSelShape()
{
    Layer* L = EdLayer();
    if (!L || g_edSel < 0 || g_edSel >= (int)L->shapes.size()) return nullptr;
    return &L->shapes[g_edSel];
}
// Der Nummern-Spinner in der Werkzeugleiste gilt fuer die ausgewaehlte Marke,
// sonst fuer die naechste zu setzende.
static bool EdNumSelected()
{
    Shape* sh = EdSelShape();
    return sh && sh->type == ShapeType::Number;
}
static bool EdNumActive() { return EdNumSelected() || g_tool == Tool::Number; }

static void EdDeleteSelected()
{
    Layer* L = EdLayer();
    if (!L || g_edSel < 0 || g_edSel >= (int)L->shapes.size()) return;
    PushUndo();
    L->shapes.erase(L->shapes.begin() + g_edSel);
    g_edSel = -1;
    EdNumRefresh();
    EdSave();
    Redraw();
}
static void EdCancelStroke()
{
    if (g_cropDrag) { g_cropDrag = false; Redraw(); }
    if (g_edDrawing || g_edMoving) {
        g_edDrawing = g_edMoving = false;
        if (!g_undo.empty()) { Snippet* s = EdSnip(); if (s) s->layers = g_undo.back(); g_undo.pop_back(); }
        Redraw();
    }
}

// --- Zeichnen

static void DrawSep(HDC dc, int x, int y) { FillSolid(dc, MakeRect(x, y + S(6), x + 1, y + S(26)), CLR_STROKE2); }

static void DrawEdStatus(HDC dc, int cw, int ch)
{
    RECT bar{ 0, ch - StatusH(), cw, ch };
    FillSolid(dc, bar, CLR_LAYER);
    FillSolid(dc, MakeRect(0, bar.top, cw, bar.top + 1), CLR_STROKE);
    std::wstring hint = g_flash.empty()
        ? std::wstring(TR(S_ED_HINT))
        : g_flash;
    if (g_flash.empty() && g_hoverKind == HK::Tool && g_hoverA >= 0 && g_hoverA < kToolCount)
        hint = std::wstring(TR(kTools[g_hoverA].name)) + L"   (" + kTools[g_hoverA].key + L")";
    if (g_flash.empty() && (g_hoverKind == HK::EdNumMinus || g_hoverKind == HK::EdNumPlus))
        hint = TR(S_ED_NUM_HINT);
    DrawIcon(dc, MakeRect(S(10), bar.top, S(34), ch), g_flash.empty() ? ICO_INFO : ICO_ACCEPT, g_flash.empty() ? CLR_TEXT2 : CLR_ACCENT, g_fontIconS);
    DrawTextIn(dc, MakeRect(S(38), bar.top, cw - S(220), ch), hint, g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    int y = bar.top + S(3), h = S(24);
    RECT plus{ cw - S(10) - S(28), y, cw - S(10), y + h };
    RECT val{ plus.left - S(2) - S(60), y, plus.left - S(2), y + h };
    RECT minus{ val.left - S(2) - S(28), y, val.left - S(2), y + h };
    DrawButton(dc, minus, ICO_ZOOMOUT, L"", IsHot(HK::ZoomOut), BtnStyle::Subtle);
    wchar_t zb[16];
    swprintf_s(zb, L"%d %%", (int)(g_edZoom * 100 + 0.5));
    DrawButton(dc, val, nullptr, zb, IsHot(HK::ZoomReset), BtnStyle::Subtle);
    DrawButton(dc, plus, ICO_ZOOMIN, L"", IsHot(HK::ZoomIn), BtnStyle::Subtle);
    Zone_Add(minus, HK::ZoomOut);
    Zone_Add(val, HK::ZoomReset);
    Zone_Add(plus, HK::ZoomIn);
}

static void DrawEditor(HDC dc, int cw, int ch)
{
    Snippet* s = EdSnip();
    if (!s) return;
    const int barH = S(48), panelW = S(240);
    // ---- Werkzeugleiste
    RECT bar{ 0, 0, cw, barH };
    FillSolid(dc, bar, CLR_LAYER);
    int x = S(10), y = S(8);
    {
        RECT r{ x, y, x + S(92), y + S(32) };
        DrawButton(dc, r, ICO_BACK, TR(S_DONE), IsHot(HK::EdDone), BtnStyle::Accent);
        Zone_Add(r, HK::EdDone);
        x = r.right + S(10);
    }
    DrawSep(dc, x, y); x += S(8);
    for (int i = 0; i < kToolCount; i++) {
        RECT r{ x, y, x + S(34), y + S(32) };
        bool sel = g_tool == kTools[i].tool;
        if (sel) DrawRoundBox(dc, r, CLR_SUBTLE_HOT2, CLR_SUBTLE_HOT2, S(4));
        else if (IsHot(HK::Tool, i)) DrawRoundBox(dc, r, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(4));
        COLORREF ic = sel ? CLR_ACCENT : CLR_TEXT;
        if (kTools[i].icon) DrawIcon(dc, r, kTools[i].icon, ic);
        else DrawTextIn(dc, r, L"①", g_fontB, ic, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        Zone_Add(r, HK::Tool, i);
        x += S(36);
    }
    x += S(4); DrawSep(dc, x, y); x += S(10);
    for (int i = 0; i < kPaletteCount; i++) {
        RECT r{ x, y + S(6), x + S(20), y + S(26) };
        COLORREF c = kPalette[i];
        bool sel = g_annColor == c;
        DrawRoundBox(dc, r, c, (c == RGB(255, 255, 255)) ? CLR_STROKE3 : c, S(10));
        if (sel || IsHot(HK::Color, i))
            DrawEllipseAA(dc, MakeRect(r.left - S(4), r.top - S(4), r.right + S(4), r.bottom + S(4)), sel ? CLR_ACCENT : CLR_STROKE3, (float)S(2));
        Zone_Add(MakeRect(r.left - S(4), y, r.right + S(4), y + S(32)), HK::Color, i);
        x += S(28);
    }
    x += S(4); DrawSep(dc, x, y); x += S(8);
    for (int i = 0; i < 3; i++) {
        RECT r{ x, y, x + S(32), y + S(32) };
        bool sel = g_annWidth == kWidths[i];
        if (sel) DrawRoundBox(dc, r, CLR_SUBTLE_HOT2, CLR_SUBTLE_HOT2, S(4));
        else if (IsHot(HK::Width, i)) DrawRoundBox(dc, r, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(4));
        int d = S(4 + i * 4);
        RECT dot{ (r.left + r.right) / 2 - d / 2, (r.top + r.bottom) / 2 - d / 2, (r.left + r.right) / 2 - d / 2 + d, (r.top + r.bottom) / 2 - d / 2 + d };
        DrawRoundBox(dc, dot, sel ? CLR_ACCENT : CLR_TEXT, sel ? CLR_ACCENT : CLR_TEXT, d / 2);
        Zone_Add(r, HK::Width, i);
        x += S(34);
    }
    x += S(4); DrawSep(dc, x, y); x += S(8);
    {
        RECT u{ x, y, x + S(34), y + S(32) };
        DrawButton(dc, u, L"\xE7A7", L"", IsHot(HK::Undo), BtnStyle::Subtle, !g_undo.empty());
        if (!g_undo.empty()) Zone_Add(u, HK::Undo);
        RECT rd{ u.right + S(2), y, u.right + S(2) + S(34), y + S(32) };
        DrawButton(dc, rd, L"\xE7A6", L"", IsHot(HK::Redo), BtnStyle::Subtle, !g_redo.empty());
        if (!g_redo.empty()) Zone_Add(rd, HK::Redo);
        RECT del{ rd.right + S(8), y, rd.right + S(8) + S(34), y + S(32) };
        bool hasSel = EdSelShape() != nullptr;
        DrawButton(dc, del, ICO_DELETE, L"", IsHot(HK::EdDeleteSel), BtnStyle::Subtle, hasSel);
        if (hasSel) Zone_Add(del, HK::EdDeleteSel);
        x = del.right + S(8);
        if (s->HasCrop()) {
            int bw = TextWidth(dc, g_font, TR(S_CROP_RESET)) + S(28) + S(14);
            RECT cr{ x, y, x + bw, y + S(32) };
            DrawButton(dc, cr, ICO_SNIP, TR(S_CROP_RESET), IsHot(HK::EdCropReset), BtnStyle::Subtle);
            Zone_Add(cr, HK::EdCropReset);
            x = cr.right + S(8);
        }
        if (EdNumActive()) {
            bool onSel = EdNumSelected();
            int val = onSel ? EdSelShape()->number : std::max(1, g_edNumNext);
            const wchar_t* lab = onSel ? TR(S_ED_NUM_SEL) : TR(S_ED_NUM_NEXT);
            int lw = TextWidth(dc, g_font, lab) + S(8);
            DrawTextIn(dc, MakeRect(x, y, x + lw, y + S(32)), lab, g_font, CLR_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RECT nm{ x + lw, y, x + lw + S(28), y + S(32) };
            RECT nv{ nm.right + S(2), y, nm.right + S(2) + S(44), y + S(32) };
            RECT np{ nv.right + S(2), y, nv.right + S(2) + S(28), y + S(32) };
            DrawButton(dc, nm, ICO_MINUS, L"", IsHot(HK::EdNumMinus), BtnStyle::Subtle, val > 1);
            DrawRoundBox(dc, nv, CLR_CARD, CLR_STROKE2, S(4));
            FillSolid(dc, MakeRect(nv.left + S(4), nv.bottom - 1, nv.right - S(4), nv.bottom), CLR_STROKE3);
            DrawTextIn(dc, nv, std::to_wstring(val), g_font, CLR_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawButton(dc, np, ICO_ADD, L"", IsHot(HK::EdNumPlus), BtnStyle::Subtle);
            if (val > 1) Zone_Add(nm, HK::EdNumMinus);
            Zone_Add(np, HK::EdNumPlus);
            x = np.right + S(8);
        }
    }
    {
        wchar_t info[160];
        Layer* L = EdLayer();
        std::wstring cropInfo = s->HasCrop() ? TRF(S_CROP_INFO_FMT, s->VW(), s->VH()) : L"";
        swprintf_s(info, L"#%d   %d × %d px%s   ·   %s", g_edSnip + 1, s->w, s->h, cropInfo.c_str(), L ? L->name.c_str() : L"");
        DrawTextIn(dc, MakeRect(x, y, cw - S(12), y + S(32)), info, g_font, CLR_TEXT2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    FillSolid(dc, MakeRect(0, barH - 1, cw, barH), CLR_STROKE);

    // ---- Canvas
    g_edCanvas = MakeRect(0, barH, cw - panelW, ch - StatusH());
    const RECT& cv = g_edCanvas;
    FillSolid(dc, cv, CLR_BG);
    double z = g_edZoom;
    int iw = std::max(1, (int)(s->w * z)), ih = std::max(1, (int)(s->h * z));
    int availW = RW(cv) - S(32), availH = RH(cv) - S(32);
    int maxSx = std::max(0, iw - availW), maxSy = std::max(0, ih - availH);
    g_edSx = std::max(0, std::min(g_edSx, maxSx));
    g_edSy = std::max(0, std::min(g_edSy, maxSy));
    g_edOx = cv.left + (iw <= availW ? (RW(cv) - iw) / 2 : S(16) - g_edSx);
    g_edOy = cv.top + (ih <= availH ? (RH(cv) - ih) / 2 : S(16) - g_edSy);
    HRGN clip = CreateRectRgnIndirect(&cv);
    SelectClipRgn(dc, clip);
    DeleteObject(clip);
    RECT img{ g_edOx, g_edOy, g_edOx + iw, g_edOy + ih };
    RECT frame = img; InflateRect(&frame, 1, 1);
    FrameRectColor(dc, frame, CLR_STROKE3, 1);
    BlitSnippetFull(dc, *s, img.left, img.top, z);
    Annot_Render(dc, *s, z, g_edOx, g_edOy);
    if (g_edDrawing) Annot_RenderShape(dc, g_edShape, s->w, s->h, z, g_edOx, g_edOy);
    {
        // Zuschnitt: Bereich ausserhalb abdunkeln, Rahmen in Akzentfarbe; waehrend des Aufziehens die Vorschau
        RECT c{};
        bool show = false;
        if (g_cropDrag) {
            c.left = (LONG)std::min(g_cropA.x, g_cropB.x); c.right = (LONG)std::max(g_cropA.x, g_cropB.x);
            c.top = (LONG)std::min(g_cropA.y, g_cropB.y); c.bottom = (LONG)std::max(g_cropA.y, g_cropB.y);
            show = true;
        }
        else if (s->HasCrop()) { c = s->crop; show = true; }
        if (show) {
            RECT sc{ g_edOx + (int)(c.left * z), g_edOy + (int)(c.top * z), g_edOx + (int)(c.right * z), g_edOy + (int)(c.bottom * z) };
            FillAlpha(dc, MakeRect(img.left, img.top, img.right, sc.top), CLR_BG, 150);
            FillAlpha(dc, MakeRect(img.left, sc.bottom, img.right, img.bottom), CLR_BG, 150);
            FillAlpha(dc, MakeRect(img.left, sc.top, sc.left, sc.bottom), CLR_BG, 150);
            FillAlpha(dc, MakeRect(sc.right, sc.top, img.right, sc.bottom), CLR_BG, 150);
            RECT fr = sc; InflateRect(&fr, 1, 1);
            FrameRectColor(dc, fr, CLR_ACCENT, 2);
        }
    }
    if (Shape* sel = EdSelShape()) {
        RECT b = Annot_Bounds(*sel);
        RECT sr{ g_edOx + (int)(b.left * z) - S(3), g_edOy + (int)(b.top * z) - S(3),
                 g_edOx + (int)(b.right * z) + S(3), g_edOy + (int)(b.bottom * z) + S(3) };
        HPEN pen = CreatePen(PS_DOT, 1, CLR_ACCENT);
        HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        int obk = SetBkMode(dc, TRANSPARENT);
        Rectangle(dc, sr.left, sr.top, sr.right, sr.bottom);
        SetBkMode(dc, obk);
        SelectObject(dc, op); SelectObject(dc, ob);
        DeleteObject(pen);
    }
    if (g_edTextActive) {
        Layer* L = EdLayer();
        if (L && g_edTextIdx >= 0 && g_edTextIdx < (int)L->shapes.size()) {
            Shape& sh = L->shapes[g_edTextIdx];
            RECT b = Annot_Bounds(sh);
            int fh = (int)(Annot_FontPx(sh.width) * z);
            int cx = g_edOx + (int)((sh.text.empty() ? sh.pts[0].x : (float)b.right - 2) * z);
            int cy = g_edOy + (int)(sh.pts[0].y * z);
            FillSolid(dc, MakeRect(cx, cy, cx + std::max(1, S(1)), cy + fh + S(4)), CLR_ACCENT);
        }
    }
    SelectClipRgn(dc, NULL);
    Zone_Add(cv, HK::Canvas);

    // ---- Ebenen-Panel
    RECT panel{ cw - panelW, barH, cw, ch - StatusH() };
    FillSolid(dc, panel, CLR_LAYER);
    FillSolid(dc, MakeRect(panel.left, panel.top, panel.left + 1, panel.bottom), CLR_STROKE);
    DrawIcon(dc, MakeRect(panel.left + S(10), panel.top + S(10), panel.left + S(34), panel.top + S(38)), L"\xE81E", CLR_TEXT, g_fontIconS);
    DrawTextIn(dc, MakeRect(panel.left + S(36), panel.top + S(10), panel.right - S(50), panel.top + S(38)), TR(S_LAYERS), g_fontB, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT addB{ panel.right - S(10) - S(30), panel.top + S(10), panel.right - S(10), panel.top + S(38) };
    DrawButton(dc, addB, ICO_ADD, L"", IsHot(HK::LayerAdd), BtnStyle::Subtle);
    Zone_Add(addB, HK::LayerAdd);
    int ry = panel.top + S(46);
    for (int i = (int)s->layers.size() - 1; i >= 0; i--) {
        Layer& L = s->layers[i];
        RECT row{ panel.left + S(8), ry, panel.right - S(8), ry + S(34) };
        bool sel = (i == g_curLayer);
        if (sel) DrawRoundBox(dc, row, CLR_SUBTLE_HOT, CLR_SUBTLE_HOT, S(4));
        else if (IsHot(HK::LayerSel, i)) DrawRoundBox(dc, row, CLR_CARD_HOT, CLR_STROKE, S(4));
        if (sel) DrawRoundBox(dc, MakeRect(row.left, row.top + S(8), row.left + S(3), row.bottom - S(8)), CLR_ACCENT, CLR_ACCENT, S(1));
        int cy = (row.top + row.bottom) / 2;
        RECT chk{ row.left + S(12), cy - S(9), row.left + S(12) + S(18), cy + S(9) };
        DrawCheck(dc, chk, L.visible, IsHot(HK::LayerVis, i));
        Zone_Add(MakeRect(chk.left - S(4), row.top, chk.right + S(4), row.bottom), HK::LayerVis, i);
        RECT nr{ chk.right + S(10), row.top, row.right - S(36), row.bottom };
        if (g_editLayer == i) {
            RECT er{ nr.left - S(4), row.top + S(5), nr.right, row.bottom - S(5) };
            DrawRoundBox(dc, er, CLR_CARD, CLR_ACCENT, S(3));
            RECT tr{ er.left + S(4), er.top, er.right - S(2), er.bottom };
            int textW = std::min(TextWidth(dc, g_font, g_editText), RW(tr));
            if (g_editSelectAll && !g_editText.empty())
                FillSolid(dc, MakeRect(tr.left, tr.top + S(3), tr.left + textW, tr.bottom - S(3)), g_th.selBg);
            DrawTextIn(dc, tr, g_editText, g_font, CLR_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            FillSolid(dc, MakeRect(tr.left + textW, tr.top + S(4), tr.left + textW + 1, tr.bottom - S(4)), CLR_TEXT);
        }
        else {
            DrawTextIn(dc, nr, L.name, sel ? g_fontB : g_font, L.visible ? CLR_TEXT : CLR_TEXT3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        DrawTextIn(dc, MakeRect(row.right - S(36), row.top, row.right - S(10), row.bottom), std::to_wstring(L.shapes.size()), g_fontSub, CLR_TEXT2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        Zone_Add(row, HK::LayerSel, i);
        ry += S(36);
    }
    ry += S(6);
    {
        int n = (int)s->layers.size();
        RECT up{ panel.left + S(8), ry, panel.left + S(8) + S(34), ry + S(32) };
        RECT dn{ up.right + S(2), ry, up.right + S(2) + S(34), ry + S(32) };
        RECT del{ dn.right + S(10), ry, dn.right + S(10) + S(34), ry + S(32) };
        bool canUp = g_curLayer < n - 1, canDown = g_curLayer > 0, canDel = n > 1;
        DrawButton(dc, up, L"\xE74A", L"", IsHot(HK::LayerUp), BtnStyle::Subtle, canUp);
        DrawButton(dc, dn, L"\xE74B", L"", IsHot(HK::LayerDown), BtnStyle::Subtle, canDown);
        DrawButton(dc, del, ICO_DELETE, L"", IsHot(HK::LayerDel), BtnStyle::Subtle, canDel);
        if (canUp) Zone_Add(up, HK::LayerUp);
        if (canDown) Zone_Add(dn, HK::LayerDown);
        if (canDel) Zone_Add(del, HK::LayerDel);
        ry += S(40);
    }
    DrawTextIn(dc, MakeRect(panel.left + S(12), ry, panel.right - S(12), ry + S(60)),
        TR(S_LAYERS_HINT),
        g_fontSub, CLR_TEXT2, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    DrawEdStatus(dc, cw, ch);
}

// --- Eingaben

static void EdCanvasDown(POINT p)
{
    Snippet* s = EdSnip();
    Layer* L = EdLayer();
    if (!s || !L) return;
    EdCommitText(true);
    AnnPoint ip = ToImg(p);
    float tol = (float)(4.0 / g_edZoom);
    switch (g_tool) {
    case Tool::Select: {
        int idx = Annot_HitTest(*L, ip, tol);
        g_edSel = idx;
        if (idx >= 0) { PushUndo(); g_edMoving = true; g_edLast = ip; SetCapture(g_hwnd); }
        Redraw();
        return;
    }
    case Tool::Text: {
        PushUndo();
        Shape sh; sh.type = ShapeType::Text; sh.color = g_annColor; sh.width = g_annWidth; sh.pts = { ip };
        L->shapes.push_back(sh);
        g_edTextIdx = (int)L->shapes.size() - 1;
        g_edTextActive = true;
        g_edSel = -1;
        Redraw();
        return;
    }
    case Tool::Crop:
        g_cropDrag = true;
        g_cropA = g_cropB = ip;
        g_edSel = -1;
        SetCapture(g_hwnd);
        Redraw();
        return;
    case Tool::Number: {
        PushUndo();
        Shape sh; sh.type = ShapeType::Number; sh.color = g_annColor; sh.width = g_annWidth; sh.pts = { ip };
        sh.number = std::max(1, g_edNumNext);
        L->shapes.push_back(sh);
        g_edSel = (int)L->shapes.size() - 1;
        EdNumRefresh();
        EdSave();
        Redraw();
        return;
    }
    default: break;
    }
    PushUndo();
    g_edShape = Shape{};
    g_edShape.color = g_annColor;
    g_edShape.width = g_annWidth;
    switch (g_tool) {
    case Tool::Pen:     g_edShape.type = ShapeType::Pen; g_edShape.pts = { ip }; break;
    case Tool::Marker:  g_edShape.type = ShapeType::Marker; g_edShape.pts = { ip }; break;
    case Tool::Line:    g_edShape.type = ShapeType::Line; g_edShape.pts = { ip, ip }; break;
    case Tool::Arrow:   g_edShape.type = ShapeType::Arrow; g_edShape.pts = { ip, ip }; break;
    case Tool::Rect:    g_edShape.type = ShapeType::Rect; g_edShape.pts = { ip, ip }; break;
    case Tool::Ellipse: g_edShape.type = ShapeType::Ellipse; g_edShape.pts = { ip, ip }; break;
    default: break;
    }
    g_edDrawing = true;
    g_edSel = -1;
    SetCapture(g_hwnd);
    Redraw();
}

static void EdOnMouseMove(POINT p)
{
    AnnPoint ip = ToImg(p);
    if (g_cropDrag) {
        Snippet* s = EdSnip();
        if (s) {
            g_cropB.x = std::max(0.0f, std::min((float)s->w, ip.x));
            g_cropB.y = std::max(0.0f, std::min((float)s->h, ip.y));
            Redraw();
        }
        return;
    }
    if (g_edDrawing) {
        if (g_edShape.type == ShapeType::Pen || g_edShape.type == ShapeType::Marker) {
            AnnPoint& last = g_edShape.pts.back();
            float dx = ip.x - last.x, dy = ip.y - last.y;
            if (dx * dx + dy * dy >= (float)(2.0 / g_edZoom) * (float)(2.0 / g_edZoom)) g_edShape.pts.push_back(ip);
        }
        else if (g_edShape.pts.size() >= 2) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift) {   // Umschalt: Quadrat/Kreis bzw. 45-Grad-Linien
                float dx = ip.x - g_edShape.pts[0].x, dy = ip.y - g_edShape.pts[0].y;
                if (g_edShape.type == ShapeType::Rect || g_edShape.type == ShapeType::Ellipse) {
                    float m = std::max(fabsf(dx), fabsf(dy));
                    ip.x = g_edShape.pts[0].x + (dx < 0 ? -m : m);
                    ip.y = g_edShape.pts[0].y + (dy < 0 ? -m : m);
                }
                else {
                    if (fabsf(dx) > fabsf(dy) * 2) ip.y = g_edShape.pts[0].y;
                    else if (fabsf(dy) > fabsf(dx) * 2) ip.x = g_edShape.pts[0].x;
                    else { float m = std::max(fabsf(dx), fabsf(dy)); ip.x = g_edShape.pts[0].x + (dx < 0 ? -m : m); ip.y = g_edShape.pts[0].y + (dy < 0 ? -m : m); }
                }
            }
            g_edShape.pts[1] = ip;
        }
        Redraw();
    }
    else if (g_edMoving) {
        if (Shape* sh = EdSelShape()) Annot_Move(*sh, ip.x - g_edLast.x, ip.y - g_edLast.y);
        g_edLast = ip;
        Redraw();
    }
}

static void EdOnLButtonUp(POINT)
{
    if (g_cropDrag) {
        g_cropDrag = false;
        ReleaseCapture();
        Snippet* s = EdSnip();
        Tab* t = CurTab();
        if (s && t) {
            RECT c{ (LONG)floorf(std::min(g_cropA.x, g_cropB.x) + 0.5f), (LONG)floorf(std::min(g_cropA.y, g_cropB.y) + 0.5f),
                    (LONG)floorf(std::max(g_cropA.x, g_cropB.x) + 0.5f), (LONG)floorf(std::max(g_cropA.y, g_cropB.y) + 0.5f) };
            c.left = std::max(0L, c.left); c.top = std::max(0L, c.top);
            c.right = std::min((LONG)s->w, c.right); c.bottom = std::min((LONG)s->h, c.bottom);
            if (c.right - c.left >= 4 && c.bottom - c.top >= 4) {
                bool full = c.left == 0 && c.top == 0 && c.right == s->w && c.bottom == s->h;
                s->crop = full ? RECT{} : c;
                s->FreeCache();
                Model_SaveTab(*t);
                Flash(full ? std::wstring(TR(S_CROP_CLEARED)) : TRF(S_CROP_SET_FMT, s->VW(), s->VH()));
            }
        }
        Redraw();
        return;
    }
    if (g_edDrawing) {
        g_edDrawing = false;
        ReleaseCapture();
        Layer* L = EdLayer();
        bool keep = L != nullptr;
        if (keep && g_edShape.pts.size() >= 2 && g_edShape.type != ShapeType::Pen && g_edShape.type != ShapeType::Marker) {
            float dx = g_edShape.pts[1].x - g_edShape.pts[0].x, dy = g_edShape.pts[1].y - g_edShape.pts[0].y;
            if (dx * dx + dy * dy < 4.0f) keep = false;
        }
        if (keep) { L->shapes.push_back(g_edShape); g_edSel = -1; EdSave(); }
        else if (!g_undo.empty()) g_undo.pop_back();
        Redraw();
    }
    else if (g_edMoving) {
        g_edMoving = false;
        ReleaseCapture();
        EdSave();
        Redraw();
    }
}

static void EdApplyStyleToSelection()
{
    if (Shape* sh = EdSelShape()) {
        PushUndo();
        sh = EdSelShape();
        sh->color = g_annColor;
        sh->width = g_annWidth;
        EdSave();
    }
}

// Aendert die Nummer der Auswahl, sonst die der naechsten Marke.
static void EdNumAdjust(int d)
{
    if (EdNumSelected()) {
        int nv = std::max(1, EdSelShape()->number + d);
        if (nv != EdSelShape()->number) {
            PushUndo();
            EdSelShape()->number = nv;
            EdSave();
        }
    }
    else g_edNumNext = std::max(1, g_edNumNext + d);
    Redraw();
}

static bool EdOnLButtonDown(POINT p, const Zone* z)
{
    if (!z) { if (g_edTextActive) EdCommitText(true); return true; }
    Snippet* s = EdSnip();
    if (!s) return true;
    switch (z->kind) {
    case HK::EdDone: CloseEditor(); break;
    case HK::Tool: EdCommitText(true); g_tool = kTools[z->a].tool; if (g_tool != Tool::Select) g_edSel = -1; Redraw(); break;
    case HK::Color: g_annColor = kPalette[z->a]; EdApplyStyleToSelection(); Redraw(); break;
    case HK::Width: g_annWidth = kWidths[z->a]; EdApplyStyleToSelection(); Redraw(); break;
    case HK::Undo: EdUndo(); break;
    case HK::Redo: EdRedo(); break;
    case HK::EdDeleteSel: EdDeleteSelected(); break;
    case HK::Canvas: EdCanvasDown(p); break;
    case HK::LayerSel: EdCommitText(true); g_curLayer = z->a; g_edSel = -1; Redraw(); break;
    case HK::LayerVis:
        if (z->a >= 0 && z->a < (int)s->layers.size()) { s->layers[z->a].visible = !s->layers[z->a].visible; EdSave(); Redraw(); }
        break;
    case HK::LayerAdd: {
        EdCommitText(true);
        PushUndo();
        Layer L; L.name = TRF(S_LAYER_N_FMT, (int)s->layers.size() + 1);
        s->layers.push_back(L);
        g_curLayer = (int)s->layers.size() - 1;
        g_edSel = -1;
        EdSave(); Redraw();
        break;
    }
    case HK::LayerDel:
        if (s->layers.size() > 1 && g_curLayer >= 0 && g_curLayer < (int)s->layers.size()) {
            EdCommitText(true);
            PushUndo();
            s->layers.erase(s->layers.begin() + g_curLayer);
            g_curLayer = std::min(g_curLayer, (int)s->layers.size() - 1);
            g_edSel = -1;
            EdSave(); Redraw();
        }
        break;
    case HK::LayerUp:
        if (g_curLayer + 1 < (int)s->layers.size()) { PushUndo(); std::swap(s->layers[g_curLayer], s->layers[g_curLayer + 1]); g_curLayer++; EdSave(); Redraw(); }
        break;
    case HK::LayerDown:
        if (g_curLayer > 0) { PushUndo(); std::swap(s->layers[g_curLayer], s->layers[g_curLayer - 1]); g_curLayer--; EdSave(); Redraw(); }
        break;
    case HK::ZoomOut: EdSetZoom(g_edZoom / 1.25); break;
    case HK::ZoomIn: EdSetZoom(g_edZoom * 1.25); break;
    case HK::ZoomReset: EdFitZoom(); Redraw(); break;
    case HK::EdCropReset:
        if (Tab* t = CurTab()) { s->crop = RECT{}; s->FreeCache(); Model_SaveTab(*t); Flash(TR(S_CROP_CLEARED)); Redraw(); }
        break;
    case HK::EdNumMinus: EdNumAdjust(-1); break;
    case HK::EdNumPlus:  EdNumAdjust(+1); break;
    default: break;
    }
    return true;
}

static bool EdOnKeyDown(WPARAM vk, bool ctrl)
{
    if (g_editLayer >= 0 || g_edTextActive) return false;
    switch (vk) {
    case VK_ESCAPE:
        if (g_edDrawing || g_edMoving || g_cropDrag) { ReleaseCapture(); EdCancelStroke(); return true; }
        if (g_edSel >= 0) { g_edSel = -1; Redraw(); return true; }
        CloseEditor();
        return true;
    case VK_DELETE: case VK_BACK: EdDeleteSelected(); return true;
    case 'Z': if (ctrl) { EdUndo(); return true; } break;
    case 'Y': if (ctrl) { EdRedo(); return true; } break;
    case '0': case VK_NUMPAD0: if (ctrl) { EdFitZoom(); Redraw(); return true; } break;
    case VK_ADD: case VK_OEM_PLUS:
        if (ctrl) { EdSetZoom(g_edZoom * 1.25); return true; }
        if (EdNumActive()) { EdNumAdjust(+1); return true; }
        break;
    case VK_SUBTRACT: case VK_OEM_MINUS:
        if (ctrl) { EdSetZoom(g_edZoom / 1.25); return true; }
        if (EdNumActive()) { EdNumAdjust(-1); return true; }
        break;
    }
    if (!ctrl) {
        for (int i = 0; i < kToolCount; i++) {
            if ((WPARAM)kTools[i].key == vk) { g_tool = kTools[i].tool; if (g_tool != Tool::Select) g_edSel = -1; Redraw(); return true; }
        }
        if (vk >= '1' && vk <= '8') { g_annColor = kPalette[vk - '1']; EdApplyStyleToSelection(); Redraw(); return true; }
    }
    return false;
}

static void EdOnChar(WPARAM ch)
{
    if (g_editLayer >= 0) {
        if (ch == VK_RETURN) CommitLayerRename(true);
        else if (ch == VK_ESCAPE) CommitLayerRename(false);
        else if (ch == VK_BACK) {
            if (g_editSelectAll) g_editText.clear();
            else TextBackspace(g_editText);
            g_editSelectAll = false;
            Redraw();
        }
        else if (ch == 0x16) {
            if (g_editSelectAll) { g_editText.clear(); g_editSelectAll = false; }
            for (wchar_t c : ClipboardLine()) TextAppend(g_editText, c, 40);
            Redraw();
        }
        else if (ch >= 32) {
            if (g_editSelectAll) { g_editText.clear(); g_editSelectAll = false; }
            TextAppend(g_editText, (wchar_t)ch, 40);
            Redraw();
        }
        return;
    }
    if (!g_edTextActive) return;
    Layer* L = EdLayer();
    if (!L || g_edTextIdx < 0 || g_edTextIdx >= (int)L->shapes.size()) { g_edTextActive = false; return; }
    Shape& sh = L->shapes[g_edTextIdx];
    if (ch == VK_RETURN) EdCommitText(true);
    else if (ch == VK_ESCAPE) EdCommitText(false);
    else if (ch == VK_BACK) { TextBackspace(sh.text); Redraw(); }
    else if (ch == 0x16) { for (wchar_t c : ClipboardLine()) TextAppend(sh.text, c, 200); Redraw(); }
    else if (ch >= 32) { TextAppend(sh.text, (wchar_t)ch, 200); Redraw(); }
}

static int FindFocusIdx()
{
    if (g_focusKind == HK::None) return -1;
    for (size_t i = 0; i < g_zones.size(); i++)
        if (g_zones[i].kind == g_focusKind && g_zones[i].a == g_focusA && RW(g_zones[i].r) > 0 && RH(g_zones[i].r) > 0) return (int)i;
    return -1;
}

static void RenderPages(HDC dc, int cw, int ch);
static void Render(HDC dc, int cw, int ch)
{
    g_zones.clear();
    RenderPages(dc, cw, ch);
    if (g_focusVisible) {
        int i = FindFocusIdx();
        if (i >= 0) {
            RECT r = g_zones[i].r;
            InflateRect(&r, S(2), S(2));
            DrawRoundFrameAA(dc, r, CLR_ACCENT, (float)S(2), S(5));
        }
    }
}

static void RenderPages(HDC dc, int cw, int ch)
{
    RECT all{ 0, 0, cw, ch };
    FillSolid(dc, all, CLR_BG);
    if (g_page == Page::Edit) {
        if (EdSnip()) { DrawEditor(dc, cw, ch); return; }
        g_page = Page::Main;
    }
    DrawTabBar(dc, cw);
    if (g_page == Page::Settings) {
        DrawSettings(dc, MakeRect(0, TabBarH(), cw, ch));
        return;
    }
    if (g_page == Page::About) {
        DrawAbout(dc, MakeRect(0, TabBarH(), cw, ch));
        return;
    }
    DrawToolbar(dc, cw);
    g_rcContent = MakeRect(0, TabBarH() + ToolbarH(), cw, ch - StatusH());
    if (Tab* t = CurTab()) DrawStack(dc, t, g_rcContent);
    else DrawDashboard(dc, g_rcContent);
    DrawStatus(dc, cw, ch);
}

// ---------------------------------------------------------------- Hit-Test

static const Zone* HitTest(POINT p)
{
    for (auto& z : g_zones) if (PtIn(z.r, p)) return &z;
    return nullptr;
}

// ---------------------------------------------------------------- Aktionen

static void SaveWindowState()
{
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(g_hwnd, &wp)) {
        g_app.st.x = wp.rcNormalPosition.left;
        g_app.st.y = wp.rcNormalPosition.top;
        g_app.st.w = RW(wp.rcNormalPosition);
        g_app.st.h = RH(wp.rcNormalPosition);
        g_app.st.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        g_app.st.dpi = g_dpi;
    }
    Tab* t = CurTab();
    g_app.st.lastTab = t ? t->guid : L"";
}

static void SelectTab(int idx)
{
    if (g_editTab >= 0) g_editTab = -1;
    g_hotkeyCapture = false;
    g_active = (idx >= 0 && idx < (int)g_app.tabs.size()) ? idx : -1;
    if (g_active == -1 && !g_app.def.dashboard && !g_app.tabs.empty()) g_active = 0;
    Redraw();
}

static void NewTab()
{
    Tab* t = Model_NewTab();
    if (!t) return;
    g_page = Page::Main;
    SelectTab((int)g_app.tabs.size() - 1);
}

static void EnsureTabForCapture()
{
    if (CurTab()) return;
    if (g_app.tabs.empty()) { Tab* t = Model_NewTab(); if (!t) return; }
    g_active = 0;
}

// Stille Kopierhelfer fuer das automatische Kopieren nach der Aufnahme (Meldung macht der Aufrufer).
static bool CopySnippetSilent(Tab& t, int idx)
{
    if (idx < 0 || idx >= (int)t.snippets.size() || !t.snippets[idx]->bmp) return false;
    Snippet& s = *t.snippets[idx];
    HBITMAP c = Image_RenderSnippet(s, true);
    if (!c) return false;
    bool ok = Image_CopyToClipboard(g_hwnd, c, s.VW(), s.VH());
    DeleteObject(c);
    return ok;
}
static bool CopyTabSilent(Tab& t, int* w, int* h)
{
    HBITMAP c = Image_BuildComposite(t, w, h);
    if (!c) return false;
    bool ok = Image_CopyToClipboard(g_hwnd, c, *w, *h);
    DeleteObject(c);
    return ok;
}
static std::wstring AutoCopyAfterCapture(Tab& t, int idx)
{
    if (t.autoCopy == 1) return CopySnippetSilent(t, idx) ? std::wstring(TR(S_AC_DONE_SNIP)) : std::wstring();
    if (t.autoCopy == 2) { int w = 0, h = 0; return CopyTabSilent(t, &w, &h) ? TRF(S_AC_DONE_TAB, w, h) : std::wstring(); }
    return std::wstring();
}

static void OnCaptureDone(HBITMAP bmp, int w, int h, CaptureMode mode, int replaceIndex, void*)
{
    if (!bmp) {
        if (g_wasVisible) { ShowWindow(g_hwnd, SW_SHOW); }
        return;
    }
    Tab* t = CurTab();
    if (!t) { DeleteObject(bmp); ShowWindow(g_hwnd, SW_SHOW); return; }
    if (mode == CaptureMode::Replace) {
        if (replaceIndex >= 0 && replaceIndex < (int)t->snippets.size()) {
            Snippet& s = *t->snippets[replaceIndex];
            s.SetBitmap(bmp, w, h);                       // alte Bitmap wird freigegeben
            s.crop = RECT{};                               // Zuschnitt passt zum alten Bild, nicht zum neuen
            s.unsaved = !Model_SaveSnippetPng(*t, s);     // PNG gleichnamig ueberschreiben; bei Fehler spaeter erneut
            Flash(TRF(S_REPLACED_FMT, replaceIndex + 1) + AutoCopyAfterCapture(*t, replaceIndex));
        }
        else DeleteObject(bmp);
    }
    else {
        auto s = std::make_unique<Snippet>();
        s->file = Model_NewSnippetFile(*t);
        s->SetBitmap(bmp, w, h);
        s->unsaved = !Model_SaveSnippetPng(*t, *s);      // bleibt im Speicher, naechstes Speichern versucht es erneut
        t->snippets.push_back(std::move(s));
        Model_SaveTab(*t);
        t->scroll = INT_MAX / 2;                          // wird beim Zeichnen nach unten geklemmt
        Flash(TRF(S_APPENDED_FMT, w, h) + AutoCopyAfterCapture(*t, (int)t->snippets.size() - 1));
    }
    ShowWindow(g_hwnd, SW_SHOW);
    if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
    SetForegroundWindow(g_hwnd);
    Redraw();
}

static void BeginCapture(CaptureMode mode, int replaceIndex)
{
    if (Capture_IsActive()) {
        // Hotkey waehrend der Verzoegerung: laufende Aufnahme abbrechen statt still ignorieren.
        if (Capture_IsWaiting()) { Capture_Cancel(); Flash(TR(S_CAPTURE_CANCELLED)); }
        return;
    }
    if (g_modal > 0) { MessageBeep(MB_OK); Flash(TR(S_MODAL_BUSY)); return; }
    if (g_editTab >= 0) CommitRename(true);
    g_hotkeyCapture = false;
    CloseEditor();
    g_page = Page::Main;
    EnsureTabForCapture();
    Tab* t = CurTab();
    if (!t) return;
    if (mode == CaptureMode::Replace && (replaceIndex < 0 || replaceIndex >= (int)t->snippets.size())) return;
    g_wasVisible = IsWindowVisible(g_hwnd) != FALSE;
    ShowWindow(g_hwnd, SW_HIDE);
    Capture_Begin(g_hInst, t->delayMs, g_app.def.captureCursor, mode, replaceIndex, OnCaptureDone, nullptr);
}

static bool Confirm(const std::wstring& instr, const std::wstring& content, bool* dontAskAgain)
{
    ModalScope modal;
    TASKDIALOGCONFIG tdc{};
    tdc.cbSize = sizeof(tdc);
    tdc.hwndParent = g_hwnd;
    tdc.hInstance = g_hInst;
    tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    tdc.pszWindowTitle = L"MultiSnip";
    tdc.pszMainIcon = TD_WARNING_ICON;
    tdc.pszMainInstruction = instr.c_str();
    tdc.pszContent = content.c_str();
    if (dontAskAgain) tdc.pszVerificationText = TR(S_DONT_ASK);
    tdc.nDefaultButton = IDNO;
    int btn = 0;
    BOOL ver = FALSE;
    if (FAILED(TaskDialogIndirect(&tdc, &btn, NULL, &ver))) {
        btn = MessageBoxW(g_hwnd, (instr + L"\n\n" + content).c_str(), L"MultiSnip", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        ver = FALSE;
    }
    if (dontAskAgain) *dontAskAgain = (ver != FALSE);
    return btn == IDYES;
}

static void CloseTab(int idx)
{
    if (idx < 0 || idx >= (int)g_app.tabs.size()) return;
    Tab& t = *g_app.tabs[idx];
    std::wstring instr = TRF(S_DEL_TAB_Q_FMT, t.title.c_str());
    std::wstring content = TRF(S_DEL_TAB_D_FMT, (int)t.snippets.size());
    if (!t.snippets.empty() && !Confirm(instr, content, nullptr)) return;
    Model_DeleteTab((size_t)idx);
    if (g_app.tabs.empty()) {
        if (g_app.def.dashboard) g_active = -1;
        else { Model_NewTab(); g_active = 0; }
    }
    else if (idx < g_active) g_active--;                           // Tab links vom aktiven: Auswahl bleibt
    else if (g_active >= (int)g_app.tabs.size()) g_active = (int)g_app.tabs.size() - 1;
    g_loadedTab = -2;                                              // Indizes verschoben: Ladezustand neu abgleichen
    g_editTab = -1;
    Redraw();
}

static void DeleteSnippet(int idx)
{
    Tab* t = CurTab();
    if (!t || idx < 0 || idx >= (int)t->snippets.size()) return;
    if (!g_app.def.noAskDelete) {
        bool dont = false;
        if (!Confirm(TRF(S_DEL_SNIP_Q_FMT, idx + 1), TR(S_DEL_SNIP_D), &dont)) return;
        if (dont) { g_app.def.noAskDelete = true; Model_SaveGlobal(); }
    }
    Model_DeleteSnippet(*t, (size_t)idx);
    Model_SaveTab(*t);
    Redraw();
}

// ---------------------------------------------------------------- Texterkennung (OCR)

static bool g_ocrBusy = false;
static std::wstring g_ocrText;
static HFONT g_ocrFont = nullptr;
enum { IDC_OCR_EDIT = 100, IDC_OCR_COPY = 101, IDC_OCR_HINT = 102 };

static LRESULT CALLBACK OcrEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN && wp == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, OcrEditProc, 1);
    return DefSubclassProc(h, msg, wp, lp);
}

static INT_PTR CALLBACK OcrDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
{
    switch (msg) {
    case WM_INITDIALOG: {
        std::wstring lang = Ocr_LanguageName();
        SetWindowTextW(dlg, TRF(S_OCR_TITLE_FMT, lang.c_str()).c_str());
        HWND ed = GetDlgItem(dlg, IDC_OCR_EDIT);
        UINT dpi = GetDpiForWindow(dlg);
        g_ocrFont = CreateFontW(-MulDiv(10, (int)dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        SendMessageW(ed, WM_SETFONT, (WPARAM)g_ocrFont, TRUE);
        SetWindowTextW(ed, g_ocrText.c_str());
        SetWindowSubclass(ed, OcrEditProc, 1, 0);
        SetDlgItemTextW(dlg, IDC_OCR_COPY, TR(S_OCR_COPY_ALL));
        SetDlgItemTextW(dlg, IDCANCEL, TR(S_CLOSE));
        SetDlgItemTextW(dlg, IDC_OCR_HINT, TR(S_OCR_HINT));
        BOOL dark = g_dark ? TRUE : FALSE;
        DwmSetWindowAttribute(dlg, 20, &dark, sizeof(dark));
        SetFocus(ed);
        SendMessageW(ed, EM_SETSEL, 0, 0);
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_OCR_COPY || LOWORD(wp) == IDOK) {
            HWND ed = GetDlgItem(dlg, IDC_OCR_EDIT);
            int n = GetWindowTextLengthW(ed);
            std::wstring txt((size_t)n + 1, L'\0');
            GetWindowTextW(ed, &txt[0], n + 1);
            txt.resize((size_t)n);
            if (OpenClipboard(dlg)) {
                EmptyClipboard();
                HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (txt.size() + 1) * sizeof(wchar_t));
                if (hg) {
                    memcpy(GlobalLock(hg), txt.c_str(), (txt.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(hg);
                    if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);
                }
                CloseClipboard();
            }
            SetDlgItemTextW(dlg, IDC_OCR_COPY, TR(S_OCR_COPIED_BTN));
            SetTimer(dlg, 1, 1500, NULL);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, 0); return TRUE; }
        break;
    case WM_TIMER:
        KillTimer(dlg, 1);
        SetDlgItemTextW(dlg, IDC_OCR_COPY, TR(S_OCR_COPY_ALL));
        return TRUE;
    case WM_CLOSE: EndDialog(dlg, 0); return TRUE;
    case WM_DESTROY:
        if (g_ocrFont) { DeleteObject(g_ocrFont); g_ocrFont = nullptr; }
        break;
    }
    return FALSE;
}

// Dialogvorlage im Speicher (kein .rc-Dialog noetig). Alle Masse in Dialogeinheiten.
static void DlgAlign(std::vector<WORD>& v) { while (v.size() % 2) v.push_back(0); }
static void DlgStr(std::vector<WORD>& v, const wchar_t* s) { for (; *s; s++) v.push_back((WORD)*s); v.push_back(0); }
static void DlgItem(std::vector<WORD>& v, DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* text)
{
    DlgAlign(v);
    DWORD ex = 0;
    v.push_back(LOWORD(style)); v.push_back(HIWORD(style));
    v.push_back(LOWORD(ex)); v.push_back(HIWORD(ex));
    v.push_back((WORD)x); v.push_back((WORD)y); v.push_back((WORD)cx); v.push_back((WORD)cy);
    v.push_back(id);
    v.push_back(0xFFFF); v.push_back(cls);          // vordefinierte Klasse: 0x80 Button, 0x81 Edit, 0x82 Static
    DlgStr(v, text);
    v.push_back(0);                                 // keine Creation-Daten
}

static void ShowOcrDialog(const std::wstring& text)
{
    g_ocrText = text;
    std::vector<WORD> v;
    const short W = 340, H = 220;
    DWORD style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    v.push_back(LOWORD(style)); v.push_back(HIWORD(style));
    v.push_back(0); v.push_back(0);                 // Ex-Style
    v.push_back(4);                                 // Anzahl Elemente
    v.push_back(0); v.push_back(0); v.push_back(W); v.push_back(H);
    v.push_back(0); v.push_back(0);                 // Menue, Klasse
    DlgStr(v, L"MultiSnip");
    v.push_back(9); DlgStr(v, L"Segoe UI");         // DS_SETFONT
    DlgItem(v, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        7, 7, W - 14, H - 44, IDC_OCR_EDIT, 0x81, L"");
    DlgItem(v, WS_CHILD | WS_VISIBLE | SS_LEFT, 7, H - 31, W - 130, 10, IDC_OCR_HINT, 0x82, L"");
    DlgItem(v, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, W - 7 - 60 - 4 - 60, H - 21, 60, 14, IDC_OCR_COPY, 0x80, L"");
    DlgItem(v, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, W - 7 - 60, H - 21, 60, 14, IDCANCEL, 0x80, L"");
    ModalScope modal;
    DialogBoxIndirectParamW(g_hInst, (LPCDLGTEMPLATEW)v.data(), g_hwnd, OcrDlgProc, 0);
}

static void StartOcr(int idx)
{
    Tab* t = CurTab();
    if (!t || idx < 0 || idx >= (int)t->snippets.size()) return;
    Snippet& s = *t->snippets[idx];
    if (!s.bmp) { Flash(TRF(S_FILE_MISSING_FMT, s.file.c_str())); return; }
    if (g_ocrBusy) { Flash(TR(S_OCR_RUNNING)); return; }
    if (!Ocr_Available()) {
        ModalScope modal;
        MessageBoxW(g_hwnd, TR(S_OCR_NO_LANG), TR(S_OCR_NO_LANG_TITLE), MB_OK | MB_ICONINFORMATION);
        return;
    }
    HBITMAP view = Image_RenderSnippet(s, false);        // nur der Zuschnitt, ohne Ebenen
    bool started = view && Ocr_Start(view, s.VW(), s.VH(), g_hwnd, WM_OCR_RESULT);
    if (view) DeleteObject(view);
    if (!started) { Flash(TR(S_OCR_FAILED)); return; }
    g_ocrBusy = true;
    Flash(TR(S_OCR_RUNNING));
}

static void OnOcrResult(WPARAM status, LPARAM lp)
{
    std::unique_ptr<std::wstring> text((std::wstring*)lp);
    g_ocrBusy = false;
    switch ((OcrStatus)status) {
    case OcrStatus::Ok:
        g_flash.clear(); Redraw();
        if (text) ShowOcrDialog(*text);
        break;
    case OcrStatus::NoText: Flash(TR(S_OCR_NONE)); break;
    case OcrStatus::NoLanguage: {
        g_flash.clear();
        ModalScope modal;
        MessageBoxW(g_hwnd, TR(S_OCR_NO_LANG), TR(S_OCR_NO_LANG_TITLE), MB_OK | MB_ICONINFORMATION);
        break;
    }
    default: Flash(TR(S_OCR_FAILED)); break;
    }
}

static void DoCopySnippet(int idx)
{
    Tab* t = CurTab();
    if (!t || idx < 0 || idx >= (int)t->snippets.size()) return;
    Snippet& s = *t->snippets[idx];
    if (!s.bmp) { Flash(TRF(S_FILE_MISSING_FMT, s.file.c_str())); return; }
    HBITMAP c = Image_RenderSnippet(s);
    if (!c) { Flash(TR(S_COMPOSITE_TOO_BIG)); return; }
    bool ok = Image_CopyToClipboard(g_hwnd, c, s.VW(), s.VH());
    DeleteObject(c);
    Flash(ok ? TRF(S_SNIP_COPIED_FMT, idx + 1, s.VW(), s.VH()) : std::wstring(TR(S_CLIP_FAIL)));
}

static void DoCopy()
{
    Tab* t = CurTab();
    if (!t) return;
    int w = 0, h = 0;
    HBITMAP c = Image_BuildComposite(*t, &w, &h);
    if (!c) { Flash(TR(Image_ComputeLayout(*t, false).items.empty() ? S_NO_VISIBLE_COPY : S_COMPOSITE_TOO_BIG)); return; }
    bool ok = Image_CopyToClipboard(g_hwnd, c, w, h);
    DeleteObject(c);
    Flash(ok ? TRF(S_COPIED_FMT, w, h) : std::wstring(TR(S_CLIP_FAIL)));
}

static void DoSave()
{
    Tab* t = CurTab();
    if (!t) return;
    int w = 0, h = 0;
    HBITMAP c = Image_BuildComposite(*t, &w, &h);
    if (!c) { Flash(TR(Image_ComputeLayout(*t, false).items.empty() ? S_NO_VISIBLE_SAVE : S_COMPOSITE_TOO_BIG)); return; }
    wchar_t file[MAX_PATH]{};
    std::wstring name = t->title;
    for (auto& ch : name) if (wcschr(L"\\/:*?\"<>|", ch)) ch = L'_';
    wcscpy_s(file, (name + L".png").c_str());
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    // Filter braucht doppeltes Nullzeichen: "<Beschreibung>\0*.png\0\0"
    std::wstring filter = TR(S_PNG_FILTER);
    filter.push_back(L'\0');
    filter += L"*.png";
    filter.push_back(L'\0');
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.lpstrTitle = TR(S_SAVE_TITLE);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    BOOL chosen;
    { ModalScope modal; chosen = GetSaveFileNameW(&ofn); }
    if (chosen) {
        if (Image_SavePng(c, file)) Flash(TR(S_SAVED) + std::wstring(file));
        else ShowSaveError(file, GetLastError());
    }
    DeleteObject(c);
}

static void StartRename(int idx)
{
    if (idx < 0 || idx >= (int)g_app.tabs.size()) return;
    g_editTab = idx;
    g_editText = g_app.tabs[idx]->title;
    g_editSelectAll = true;
    SetFocus(g_hwnd);
    Redraw();
}
static void CommitRename(bool apply)
{
    if (g_editTab < 0) return;
    if (apply && g_editTab < (int)g_app.tabs.size()) {
        std::wstring s = g_editText;
        while (!s.empty() && iswspace(s.back())) s.pop_back();
        size_t b = 0; while (b < s.size() && iswspace(s[b])) b++;
        s = s.substr(b);
        if (!s.empty() && s != g_app.tabs[g_editTab]->title) {
            g_app.tabs[g_editTab]->title = s;
            Model_SaveTab(*g_app.tabs[g_editTab]);
        }
    }
    g_editTab = -1;
    Redraw();
}

static void SetZoom(double z)
{
    if (g_page == Page::Edit) { EdSetZoom(z); return; }
    z = std::max(0.1, std::min(4.0, z));
    if (fabs(z - 1.0) < 0.04) z = 1.0;
    if (Tab* t = CurTab()) t->scroll = (int)(t->scroll * z / g_zoom);
    g_zoom = z;
    Redraw();
}

static void ScrollBy(int dy)
{
    Tab* t = CurTab();
    if (!t) return;
    t->scroll += dy;
    ClampScroll(t);
    Redraw();
}
static void ScrollXBy(int dx)
{
    Tab* t = CurTab();
    if (!t) return;
    t->scrollX = std::max(0, std::min(t->scrollX + dx, g_maxScrollX));
    Redraw();
}

// Menue mit allen Tabs (wenn die Tab-Leiste ueberlaeuft)
static void ShowTabMenu(const RECT& anchor)
{
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < (int)g_app.tabs.size() && i < 1000; i++) {
        std::wstring title = g_app.tabs[i]->title;
        for (auto& c : title) if (c == L'&') c = L'+';          // '&' waere ein Menue-Mnemonic
        AppendMenuW(m, MF_STRING | (i == g_active ? MF_CHECKED : 0), IDM_TAB_BASE + i, title.c_str());
    }
    POINT p{ anchor.left, anchor.bottom + S(2) };
    ClientToScreen(g_hwnd, &p);
    TrackPopupMenu(m, TPM_LEFTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, g_hwnd, NULL);
    DestroyMenu(m);
}

// ---------------------------------------------------------------- Hotkey / Tray

static bool RegisterAppHotkey(bool verbose)
{
    if (g_hotkeyRegistered) { UnregisterHotKey(g_hwnd, ID_HOTKEY); g_hotkeyRegistered = false; }
    g_hotkeyRegistered = RegisterHotKey(g_hwnd, ID_HOTKEY, g_app.def.hotMod | MOD_NOREPEAT, g_app.def.hotVk) != FALSE;
    if (!g_hotkeyRegistered && verbose) {
        std::wstring m = TRF(S_HOTKEY_TAKEN_FMT, HotkeyName(g_app.def.hotMod, g_app.def.hotVk).c_str());
        MessageBoxW(g_hwnd, m.c_str(), TR(S_HOTKEY_TITLE), MB_OK | MB_ICONWARNING);
        g_page = Page::Settings;
        Redraw();
    }
    return g_hotkeyRegistered;
}

static void UpdateTrayTip()
{
    std::wstring tip = L"MultiSnip  -  " + HotkeyName(g_app.def.hotMod, g_app.def.hotVk);
    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);
}
static void TrayAdd()
{
    g_nid = NOTIFYICONDATAW{};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_trayIcon ? g_trayIcon : g_icon;
    UpdateTrayTip();
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void TrayUpdate()
{
    UpdateTrayTip();
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void TrayRemove() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

static void ShowMain()
{
    ShowWindow(g_hwnd, SW_SHOW);
    if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
    SetForegroundWindow(g_hwnd);
}

// Verzoegerung, die ein Capture gerade verwenden wuerde (aktiver Tab, sonst Standard).
static int& ActiveDelayRef()
{
    Tab* t = CurTab();
    return t ? t->delayMs : g_app.def.delayMs;
}
static void SetActiveDelay(int ms)
{
    Tab* t = CurTab();
    if (t) { t->delayMs = ms; Model_SaveTab(*t); }
    else { g_app.def.delayMs = ms; Model_SaveGlobal(); }
    Flash(TRF(S_DELAY_SET_FMT, DelayText(ms).c_str()) + (t ? TRF(S_DELAY_TAB_FMT, t->title.c_str()) : std::wstring(TR(S_DELAY_DEFAULT))));
    Redraw();
}
static HMENU BuildDelayMenu()
{
    HMENU d = CreatePopupMenu();
    int cur = ActiveDelayRef();
    for (int i = 0; i < kDelayCount; i++) {
        int ms = kDelayChoices[i];
        std::wstring txt = ms == 0 ? std::wstring(TR(S_NO_DELAY)) : DelayText(ms);
        AppendMenuW(d, MF_STRING | (cur == ms ? MF_CHECKED : 0), IDM_DELAY_BASE + i, txt.c_str());
    }
    return d;
}
static int ActiveAutoCopy()
{
    Tab* t = CurTab();
    return t ? t->autoCopy : g_app.def.autoCopy;
}
static void SetActiveAutoCopy(int v)
{
    Tab* t = CurTab();
    if (t) { t->autoCopy = v; Model_SaveTab(*t); }
    else { g_app.def.autoCopy = v; Model_SaveGlobal(); }
    Flash(TRF(S_AC_SET_FMT, AutoCopyName(v)) + (t ? TRF(S_DELAY_TAB_FMT, t->title.c_str()) : std::wstring(TR(S_DELAY_DEFAULT))));
    Redraw();
}
static HMENU BuildAutoCopyMenu()
{
    HMENU m = CreatePopupMenu();
    int cur = ActiveAutoCopy();
    for (int i = 0; i < 3; i++) AppendMenuW(m, MF_STRING | (cur == i ? MF_CHECKED : 0), IDM_AUTOCOPY_BASE + i, AutoCopyName(i));
    return m;
}
// Menue unter dem Split-Button
static void ShowDelayMenu(const RECT& anchor)
{
    HMENU d = BuildDelayMenu();
    AppendMenuW(d, MF_SEPARATOR, 0, NULL);
    AppendMenuW(d, MF_POPUP, (UINT_PTR)BuildAutoCopyMenu(), TR(S_AC_MENU));
    POINT p{ anchor.left, anchor.bottom + S(2) };
    ClientToScreen(g_hwnd, &p);
    TrackPopupMenu(d, TPM_LEFTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, g_hwnd, NULL);
    DestroyMenu(d);
}

static void ShowTrayMenu()
{
    HMENU m = CreatePopupMenu();
    HMENU d = BuildDelayMenu();
    AppendMenuW(m, MF_STRING, IDM_SNIP, (std::wstring(TR(S_NEW_SNIP)) + L"\t" + HotkeyName(g_app.def.hotMod, g_app.def.hotVk)).c_str());
    AppendMenuW(m, MF_STRING, IDM_SHOW, IsWindowVisible(g_hwnd) ? TR(S_TRAY_HIDE) : TR(S_TRAY_SHOW));
    Tab* ct = CurTab();
    std::wstring delayLabel = TRF(S_TRAY_DELAY_FMT, ct ? ct->title.c_str() : TR(S_DEFAULT));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)d, delayLabel.c_str());
    AppendMenuW(m, MF_POPUP, (UINT_PTR)BuildAutoCopyMenu(), TR(S_AC_MENU));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, TR(S_EXIT));
    SetMenuDefaultItem(m, IDM_SHOW, FALSE);
    POINT p; GetCursorPos(&p);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, g_hwnd, NULL);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static void ExitApp()
{
    SaveWindowState();
    Model_SaveGlobal();
    for (auto& t : g_app.tabs) Model_SaveTab(*t);
    TrayRemove();
    if (g_hotkeyRegistered) UnregisterHotKey(g_hwnd, ID_HOTKEY);
    DestroyWindow(g_hwnd);
}

// ---------------------------------------------------------------- Einstellungen

static void PickColor(COLORREF* target)
{
    static COLORREF custom[16] = { RGB(255,255,255), RGB(0,0,0), RGB(240,240,240), RGB(30,30,30), RGB(255,250,205), RGB(220,235,255) };
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g_hwnd;
    cc.lpCustColors = custom;
    cc.rgbResult = *target;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    ModalScope modal;
    if (ChooseColorW(&cc)) *target = cc.rgbResult;
}

static void SettingAdjust(int id, int dir, bool bigStep)
{
    Tab* t = CurTab();
    int step = bigStep ? 10 : 1;
    auto adj = [&](int& v, int lo, int hi, int st) { v = std::max(lo, std::min(hi, v + dir * st)); };
    switch (id) {
    case SET_DEF_PADX: adj(g_app.def.padX, 0, 500, step); Model_SaveGlobal(); break;
    case SET_DEF_PADY: adj(g_app.def.padY, 0, 500, step); Model_SaveGlobal(); break;
    case SET_DELAY:    adj(g_app.def.delayMs, 0, 30000, 500); Model_SaveGlobal(); break;
    case SET_TAB_PADX: if (t) { adj(t->padX, 0, 500, step); Model_SaveTab(*t); } break;
    case SET_TAB_PADY: if (t) { adj(t->padY, 0, 500, step); Model_SaveTab(*t); } break;
    case SET_TAB_DELAY: if (t) { adj(t->delayMs, 0, 30000, 500); Model_SaveTab(*t); } break;
    }
    Redraw();
}

static void SettingToggle(int id)
{
    switch (id) {
    case SET_CURSOR: g_app.def.captureCursor = !g_app.def.captureCursor; break;
    case SET_DASH:
        g_app.def.dashboard = !g_app.def.dashboard;
        if (!g_app.def.dashboard && g_active == -1) {
            if (g_app.tabs.empty()) { Model_NewTab(); }
            g_active = 0;
        }
        break;
    }
    Model_SaveGlobal();
    Redraw();
}

static void SettingColor(int id)
{
    Tab* t = CurTab();
    if (id == SET_DEF_FILL) { PickColor(&g_app.def.fill); Model_SaveGlobal(); }
    else if (id == SET_TAB_FILL && t) { PickColor(&t->fill); Model_SaveTab(*t); }
    Redraw();
}

static void HotkeyKeyDown(UINT vk)
{
    switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_MENU: case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN:
        return;
    case VK_ESCAPE:
        g_hotkeyCapture = false; Redraw(); return;
    }
    UINT mod = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
    if (GetKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
    if (GetKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mod |= MOD_WIN;
    if (!Model_HotkeyValid(mod, vk)) { Flash(TR(S_NEED_MOD)); return; }
    UINT oldMod = g_app.def.hotMod, oldVk = g_app.def.hotVk;
    g_app.def.hotMod = mod;
    g_app.def.hotVk = vk;
    g_hotkeyCapture = false;
    if (!RegisterAppHotkey(false)) {
        std::wstring m = TRF(S_COMBO_TAKEN_FMT, HotkeyName(mod, vk).c_str());
        MessageBoxW(g_hwnd, m.c_str(), TR(S_HOTKEY_TITLE), MB_OK | MB_ICONWARNING);
        g_app.def.hotMod = oldMod;
        g_app.def.hotVk = oldVk;
        RegisterAppHotkey(false);
    }
    else {
        Model_SaveGlobal();
        TrayUpdate();
        Flash(TR(S_HOTKEY_LBL) + HotkeyName(mod, vk));
    }
    Redraw();
}

// ---------------------------------------------------------------- Drag

static int ComputeDrop(int y)
{
    for (int i = 0; i < (int)g_rows.size(); i++) {
        int mid = (g_rows[i].top + g_rows[i].bottom) / 2;
        if (y < mid) return i;
    }
    return (int)g_rows.size();
}

static void FinishDrag(bool apply)
{
    if (g_autoDir) { KillTimer(g_hwnd, IDT_AUTOSCROLL); g_autoDir = 0; }
    Tab* t = CurTab();
    if (apply && t && g_dragging && g_pressSnip >= 0 && g_dropIdx >= 0) {
        int from = g_pressSnip, to = g_dropIdx;
        if (to != from && to != from + 1 && from < (int)t->snippets.size()) {
            auto p = std::move(t->snippets[from]);
            t->snippets.erase(t->snippets.begin() + from);
            if (to > from) to--;
            t->snippets.insert(t->snippets.begin() + to, std::move(p));
            Model_SaveTab(*t);
        }
    }
    g_dragging = false;
    g_mouseDown = false;
    g_pressSnip = -1;
    g_dropIdx = -1;
    Redraw();
}

// ---------------------------------------------------------------- Maus

static void Activate(const Zone& zc, POINT p, bool kb);
static void OnLButtonDown(POINT p)
{
    SetFocus(g_hwnd);
    const Zone* z = HitTest(p);
    // Mausklick setzt den Tastaturfokus auf die getroffene Zone, blendet den Ring aber aus.
    g_focusVisible = false;
    if (z) { g_focusKind = z->kind; g_focusA = z->a; }
    if (g_editTab >= 0 && !(z && z->kind == HK::Tab && z->a == g_editTab)) CommitRename(true);
    if (g_hotkeyCapture && !(z && z->kind == HK::SetHotkey)) { g_hotkeyCapture = false; Redraw(); }
    if (g_page == Page::Edit) {
        if (g_editLayer >= 0 && !(z && z->kind == HK::LayerSel && z->a == g_editLayer)) CommitLayerRename(true);
        const Zone zc2 = z ? *z : Zone{};
        EdOnLButtonDown(p, z ? &zc2 : nullptr);
        return;
    }
    if (!z) return;
    // Kopie, weil modale Dialoge waehrend der Aktion neu zeichnen und g_zones neu aufbauen.
    const Zone zc = *z;
    Activate(zc, p, false);
}

// Aktion einer Zone; kb = per Tastatur (Enter/Leertaste) ausgeloest.
static void Activate(const Zone& zc, POINT p, bool kb)
{
    const Zone* z = &zc;
    switch (z->kind) {
    case HK::Tab:      if (g_editTab != z->a) SelectTab(z->a); break;
    case HK::TabClose: CloseTab(z->a); break;
    case HK::TabAdd:   NewTab(); break;
    case HK::Dash:     g_page = Page::Main; SelectTab(-1); break;
    case HK::Snip:     BeginCapture(CaptureMode::Append, -1); break;
    case HK::SnipDelay: ShowDelayMenu(z->r); break;
    case HK::Copy:     DoCopy(); break;
    case HK::Save:     DoSave(); break;
    case HK::Rename:   StartRename(g_active); break;
    case HK::Settings: g_page = Page::Settings; Redraw(); break;
    case HK::ZoomOut:  SetZoom(g_zoom / 1.25); break;
    case HK::ZoomIn:   SetZoom(g_zoom * 1.25); break;
    case HK::ZoomReset: SetZoom(1.0); break;
    case HK::SChk:
        if (Tab* t = CurTab()) {
            if (z->a < (int)t->snippets.size()) { t->snippets[z->a]->visible = !t->snippets[z->a]->visible; Model_SaveTab(*t); Redraw(); }
        }
        break;
    case HK::SDel:     DeleteSnippet(z->a); break;
    case HK::SRep:     BeginCapture(CaptureMode::Replace, z->a); break;
    case HK::SEdit:    OpenEditor(z->a); break;
    case HK::SCopy:    DoCopySnippet(z->a); break;
    case HK::SOcr:     StartOcr(z->a); break;
    case HK::SBody:
        if (kb) { OpenEditor(z->a); break; }
        g_mouseDown = true; g_pressSnip = z->a; g_pressPt = p; g_dragging = false; g_dropIdx = -1;
        SetCapture(g_hwnd);
        break;
    case HK::DashNew:  NewTab(); break;
    case HK::SetBack:  g_page = Page::Main; g_hotkeyCapture = false; Redraw(); break;
    case HK::About:    g_page = Page::About; g_hotkeyCapture = false; Redraw(); break;
    case HK::AboutGitHub: ShellExecuteW(g_hwnd, L"open", kGitHubUrl, NULL, NULL, SW_SHOWNORMAL); break;
    case HK::SetToggle: SettingToggle(z->a); break;
    case HK::SetMinus: SettingAdjust(z->a, -1, (GetKeyState(VK_SHIFT) & 0x8000) != 0); break;
    case HK::SetPlus:  SettingAdjust(z->a, +1, (GetKeyState(VK_SHIFT) & 0x8000) != 0); break;
    case HK::SetColor: SettingColor(z->a); break;
    case HK::SetHotkey: g_hotkeyCapture = !g_hotkeyCapture; Redraw(); break;
    case HK::SetResetAsk: g_app.def.noAskDelete = false; Model_SaveGlobal(); Redraw(); break;
    case HK::SetAutoCopy:
        if (z->a >= 10) { if (Tab* t = CurTab()) { t->autoCopy = z->a - 10; Model_SaveTab(*t); } }
        else { g_app.def.autoCopy = z->a; Model_SaveGlobal(); }
        Redraw();
        break;
    case HK::SetNumScope:
        if (z->a >= 10) { if (Tab* t = CurTab()) { t->numberScope = z->a - 10; Model_SaveTab(*t); } }
        else { g_app.def.numberScope = z->a; Model_SaveGlobal(); }
        Redraw();
        break;
    case HK::SetTheme:
        g_app.def.theme = z->a;
        Model_SaveGlobal();
        ApplyTheme();
        break;
    case HK::SetLang:
        g_app.def.language = z->a;
        Lang_Set((Lang)z->a);
        Model_SaveGlobal();
        TrayUpdate();
        Redraw();
        break;
    case HK::ScrollThumb:
        g_thumbDrag = true; g_thumbGrab = p.y - g_thumb.top; SetCapture(g_hwnd); Redraw();
        break;
    case HK::ScrollTrack:
        ScrollBy(p.y < g_thumb.top ? -RH(g_rcContent) : RH(g_rcContent));
        break;
    case HK::ScrollHThumb:
        g_hthumbDrag = true; g_hthumbGrab = p.x - g_hthumb.left; SetCapture(g_hwnd); Redraw();
        break;
    case HK::ScrollHTrack:
        ScrollXBy(p.x < g_hthumb.left ? -RW(g_rcContent) / 2 : RW(g_rcContent) / 2);
        break;
    case HK::TabMore: ShowTabMenu(z->r); break;
    default: break;
    }
}

static void OnMouseMove(POINT p)
{
    g_lastMouse = p;
    if (g_page == Page::Edit && (g_edDrawing || g_edMoving || g_cropDrag)) { EdOnMouseMove(p); return; }
    if (g_thumbDrag) {
        Tab* t = CurTab();
        if (t && g_contentH > RH(g_rcContent)) {
            int len = RH(g_thumb);
            int range = RH(g_rcContent) - (g_maxScrollX > 0 ? SbW() : 0) - len;
            int maxScroll = g_contentH - RH(g_rcContent);
            int pos = p.y - g_thumbGrab - g_rcContent.top;
            t->scroll = range > 0 ? (int)((long long)pos * maxScroll / range) : 0;
            ClampScroll(t);
            Redraw();
        }
        return;
    }
    if (g_hthumbDrag) {
        Tab* t = CurTab();
        if (t && g_maxScrollX > 0) {
            int len = RW(g_hthumb);
            int range = RW(g_rcContent) - (g_contentH > RH(g_rcContent) ? SbW() : 0) - len;
            int pos = p.x - g_hthumbGrab - g_rcContent.left;
            t->scrollX = range > 0 ? (int)((long long)pos * g_maxScrollX / range) : 0;
            t->scrollX = std::max(0, std::min(t->scrollX, g_maxScrollX));
            Redraw();
        }
        return;
    }
    if (g_mouseDown) {
        if (!g_dragging) {
            if (abs(p.x - g_pressPt.x) > S(4) || abs(p.y - g_pressPt.y) > S(4)) g_dragging = true;
        }
        if (g_dragging) {
            g_dropIdx = ComputeDrop(p.y);
            int dir = 0;
            if (p.y < g_rcContent.top + S(30)) dir = -1;
            else if (p.y > g_rcContent.bottom - S(30)) dir = 1;
            if (dir != g_autoDir) {
                g_autoDir = dir;
                if (dir) SetTimer(g_hwnd, IDT_AUTOSCROLL, 30, NULL);
                else KillTimer(g_hwnd, IDT_AUTOSCROLL);
            }
            Redraw();
        }
        return;
    }
    const Zone* z = HitTest(p);
    HK k = z ? z->kind : HK::None;
    int a = z ? z->a : -1;
    if (k == HK::SBody || k == HK::ScrollTrack || k == HK::Canvas) { k = HK::None; a = -1; }
    if (k != g_hoverKind || a != g_hoverA) {
        g_hoverKind = k; g_hoverA = a;
        Redraw();
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, g_hwnd, 0 };
        TrackMouseEvent(&tme);
    }
}

static void OnLButtonUp(POINT p)
{
    if (g_page == Page::Edit) { EdOnLButtonUp(p); return; }
    if (g_thumbDrag || g_hthumbDrag) { g_thumbDrag = g_hthumbDrag = false; ReleaseCapture(); Redraw(); return; }
    if (g_mouseDown) {
        // Erst abschliessen, dann Capture freigeben: WM_CAPTURECHANGED wuerde den Drag sonst abbrechen.
        FinishDrag(true);
        ReleaseCapture();
    }
}

// ---------------------------------------------------------------- Tastatur

// ---- Tastaturnavigation: Tab/Umschalt+Tab durch die Zonen, Enter/Leertaste aktiviert,
// Pfeiltasten springen zwischen den Karten des Stapels, Strg+Pfeil sortiert um, Entf loescht.

static bool Focusable(const Zone& z)
{
    switch (z.kind) {
    case HK::None: case HK::ScrollThumb: case HK::ScrollTrack: case HK::ScrollHThumb: case HK::ScrollHTrack: case HK::Canvas:
        return false;
    default: return RW(z.r) > 0 && RH(z.r) > 0;
    }
}
static bool IsCardZone(HK k) { return k == HK::SChk || k == HK::SDel || k == HK::SRep || k == HK::SEdit || k == HK::SCopy || k == HK::SOcr || k == HK::SBody; }

// Zeile i des Stapels in den sichtbaren Bereich scrollen (nutzt die Zeilen der letzten Zeichnung).
static void EnsureRowVisible(int i)
{
    Tab* t = CurTab();
    if (!t || i < 0 || i >= (int)g_rows.size()) return;
    const Row& r = g_rows[i];
    int top = g_rcContent.top + S(8), bottom = g_rcContent.bottom - S(8) - (g_maxScrollX > 0 ? SbW() : 0);
    if (r.top < top) t->scroll += r.top - top;
    else if (r.bottom > bottom) t->scroll += std::min(r.bottom - bottom, r.top - top);
    ClampScroll(t);
}
// Einstellungs-/About-Seite so scrollen, dass die fokussierte Zone sichtbar ist.
static void EnsureZoneVisible(const RECT& r)
{
    int head = TabBarH() + S(60);
    RECT rc; GetClientRect(g_hwnd, &rc);
    int* scroll = g_page == Page::Settings ? &g_setScroll : (g_page == Page::About ? &g_aboutScroll : nullptr);
    if (!scroll) return;
    if (r.top < head) *scroll -= head - r.top + S(8);
    else if (r.bottom > rc.bottom) *scroll += r.bottom - rc.bottom + S(8);
}
static void SetFocusZone(HK k, int a)
{
    g_focusKind = k; g_focusA = a; g_focusVisible = true;
    Redraw();
}
static void MoveFocus(int dir)
{
    std::vector<int> list;
    for (size_t i = 0; i < g_zones.size(); i++) if (Focusable(g_zones[i])) list.push_back((int)i);
    if (list.empty()) return;
    int cur = FindFocusIdx();
    int pos = -1;
    for (size_t i = 0; i < list.size(); i++) if (list[i] == cur) { pos = (int)i; break; }
    if (pos < 0) pos = dir > 0 ? -1 : 0;
    int n = (int)list.size();
    int next = ((pos + dir) % n + n) % n;
    const Zone& z = g_zones[list[next]];
    // Am Ende der sichtbaren Karten weiter zur naechsten (noch unsichtbaren) Karte scrollen.
    if (g_page == Page::Main && cur >= 0 && IsCardZone(g_zones[cur].kind)) {
        Tab* t = CurTab();
        int i = g_zones[cur].a;
        if (t && dir > 0 && !IsCardZone(z.kind) && i + 1 < (int)t->snippets.size()) {
            EnsureRowVisible(i + 1); SetFocusZone(HK::SChk, i + 1); return;
        }
        if (t && dir < 0 && (!IsCardZone(z.kind) || z.a > i) && i > 0) {
            EnsureRowVisible(i - 1); SetFocusZone(HK::SBody, i - 1); return;
        }
    }
    EnsureZoneVisible(z.r);
    SetFocusZone(z.kind, z.a);
}
static void ActivateFocus(bool space)
{
    int i = FindFocusIdx();
    if (i < 0) return;
    const Zone zc = g_zones[i];
    POINT c{ (zc.r.left + zc.r.right) / 2, (zc.r.top + zc.r.bottom) / 2 };
    if (g_page == Page::Edit) { EdOnLButtonDown(c, &zc); return; }
    if (space && zc.kind == HK::SBody) {           // Leertaste auf der Karte: Sichtbarkeit umschalten
        Zone chk = zc; chk.kind = HK::SChk;
        Activate(chk, c, true);
        return;
    }
    Activate(zc, c, true);
}
// true = Taste verbraucht
static bool NavKeyDown(WPARAM vk, bool ctrl, bool shift)
{
    if (vk == VK_TAB && !ctrl) { MoveFocus(shift ? -1 : 1); return true; }
    if (!g_focusVisible) return false;
    int fi = FindFocusIdx();
    if (fi < 0) { g_focusVisible = false; return false; }
    HK k = g_focusKind; int a = g_focusA;
    switch (vk) {
    case VK_RETURN: ActivateFocus(false); return true;
    case VK_SPACE:  ActivateFocus(true); return true;
    case VK_ESCAPE: g_focusVisible = false; Redraw(); return true;
    case VK_LEFT: case VK_RIGHT:
        if (k == HK::SetMinus || k == HK::SetPlus) { SettingAdjust(a, vk == VK_LEFT ? -1 : 1, shift); return true; }
        if (k == HK::SetToggle) { SettingToggle(a); return true; }
        if (k == HK::ZoomOut || k == HK::ZoomIn || k == HK::ZoomReset) { SetZoom(vk == VK_LEFT ? g_zoom / 1.25 : g_zoom * 1.25); return true; }
        if (k == HK::Tab || k == HK::Dash || k == HK::TabClose) { MoveFocus(vk == VK_LEFT ? -1 : 1); return true; }
        return false;
    case VK_UP: case VK_DOWN:
        if (g_page == Page::Main && IsCardZone(k)) {
            Tab* t = CurTab();
            if (!t) return false;
            int n = (int)t->snippets.size();
            int to = a + (vk == VK_UP ? -1 : 1);
            if (to < 0 || to >= n) return true;
            if (ctrl) {                                // umsortieren
                std::swap(t->snippets[a], t->snippets[to]);
                Model_SaveTab(*t);
            }
            EnsureRowVisible(to);
            SetFocusZone(k, to);
            return true;
        }
        if (g_page == Page::Settings || g_page == Page::About || g_page == Page::Edit) { MoveFocus(vk == VK_UP ? -1 : 1); return true; }
        return false;
    case 'C':
        if (ctrl && g_page == Page::Main && IsCardZone(k)) { DoCopySnippet(a); return true; }   // Strg+C auf einer Karte: nur diesen Ausschnitt
        return false;
    case VK_DELETE:
        if (g_page == Page::Main && IsCardZone(k)) { DeleteSnippet(a); return true; }
        if (k == HK::Tab) { CloseTab(a); return true; }
        return false;
    case VK_F2:
        if (k == HK::Tab) { StartRename(a); return true; }
        return false;
    }
    return false;
}

static bool OnKeyDown(WPARAM vk)
{
    if (g_hotkeyCapture) { HotkeyKeyDown((UINT)vk); return true; }
    if (g_editTab >= 0) return false;      // WM_CHAR uebernimmt
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool editing = g_page == Page::Edit && (g_edTextActive || g_editLayer >= 0 || g_edDrawing || g_edMoving || g_cropDrag);
    if (!editing && NavKeyDown(vk, ctrl, shift)) return true;
    if (g_page == Page::Edit) return EdOnKeyDown(vk, ctrl);
    switch (vk) {
    case VK_ESCAPE:
        if (g_dragging) { ReleaseCapture(); FinishDrag(false); return true; }
        if (g_page != Page::Main) { g_page = Page::Main; Redraw(); return true; }
        return false;
    case 'N': if (ctrl) { BeginCapture(CaptureMode::Append, -1); return true; } break;
    case 'C': if (ctrl) { DoCopy(); return true; } break;
    case 'S': if (ctrl) { DoSave(); return true; } break;
    case 'T': if (ctrl) { NewTab(); return true; } break;
    case VK_TAB:
        if (ctrl && !g_app.tabs.empty()) {        // Strg+Tab / Strg+Umschalt+Tab: Tabs durchschalten
            int n = (int)g_app.tabs.size();
            bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int cur = g_active < 0 ? (back ? 0 : n - 1) : g_active;
            g_page = Page::Main;
            SelectTab(((cur + (back ? -1 : 1)) % n + n) % n);
            return true;
        }
        break;
    case '0': case VK_NUMPAD0: if (ctrl) { SetZoom(1.0); return true; } break;
    case VK_ADD: case VK_OEM_PLUS: if (ctrl) { SetZoom(g_zoom * 1.25); return true; } break;
    case VK_SUBTRACT: case VK_OEM_MINUS: if (ctrl) { SetZoom(g_zoom / 1.25); return true; } break;
    case VK_PRIOR: ScrollBy(-RH(g_rcContent)); return true;
    case VK_NEXT:  ScrollBy(RH(g_rcContent)); return true;
    case VK_UP:    ScrollBy(-S(60)); return true;
    case VK_DOWN:  ScrollBy(S(60)); return true;
    case VK_HOME:  if (Tab* t = CurTab()) { t->scroll = 0; Redraw(); } return true;
    case VK_END:   if (Tab* t = CurTab()) { t->scroll = INT_MAX / 2; Redraw(); } return true;
    case VK_F2:    StartRename(g_active); return true;
    }
    return false;
}

static void OnChar(WPARAM ch)
{
    if (g_page == Page::Edit) { EdOnChar(ch); return; }
    if (g_editTab < 0) return;
    if (ch == VK_RETURN) CommitRename(true);
    else if (ch == VK_ESCAPE) CommitRename(false);
    else if (ch == VK_BACK) {
        if (g_editSelectAll) g_editText.clear();
        else TextBackspace(g_editText);
        g_editSelectAll = false;
        Redraw();
    }
    else if (ch == 0x16) {                       // Strg+V
        if (g_editSelectAll) { g_editText.clear(); g_editSelectAll = false; }
        for (wchar_t c : ClipboardLine()) TextAppend(g_editText, c, 60);
        Redraw();
    }
    else if (ch >= 32) {
        if (g_editSelectAll) { g_editText.clear(); g_editSelectAll = false; }
        TextAppend(g_editText, (wchar_t)ch, 60);
        Redraw();
    }
}

// ---------------------------------------------------------------- WndProc

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_taskbarCreated && g_taskbarCreated) { TrayAdd(); return 0; }
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int cw = RW(rc), ch = RH(rc);
        if (cw > 0 && ch > 0) {
            if (!g_back || g_backW != cw || g_backH != ch) {
                if (g_back) DeleteObject(g_back);
                g_back = CreateCompatibleBitmap(hdc, cw, ch);
                g_backW = cw; g_backH = ch;
            }
            HDC m = CreateCompatibleDC(hdc);
            HGDIOBJ o = SelectObject(m, g_back);
            Render(m, cw, ch);
            BitBlt(hdc, 0, 0, cw, ch, m, 0, 0, SRCCOPY);
            SelectObject(m, o);
            DeleteDC(m);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SIZE: Redraw(); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = S(640);
        mmi->ptMinTrackSize.y = S(360);
        return 0;
    }
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wp);
        CreateFonts();
        RECT* r = (RECT*)lp;
        SetWindowPos(h, NULL, r->left, r->top, RW(*r), RH(*r), SWP_NOZORDER | SWP_NOACTIVATE);
        Redraw();
        return 0;
    }
    case WM_LBUTTONDOWN: OnLButtonDown({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }); return 0;
    case WM_LBUTTONDBLCLK: {
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        const Zone* z = HitTest(p);
        if (z && z->kind == HK::Tab) StartRename(z->a);
        else if (z && z->kind == HK::LayerSel) StartLayerRename(z->a);
        else if (z && z->kind == HK::SBody && g_page == Page::Main) OpenEditor(z->a);
        else OnLButtonDown(p);
        return 0;
    }
    case WM_MOUSEMOVE: OnMouseMove({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }); return 0;
    case WM_LBUTTONUP: OnLButtonUp({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }); return 0;
    case WM_MOUSELEAVE:
        if (g_hoverKind != HK::None) { g_hoverKind = HK::None; g_hoverA = -1; Redraw(); }
        return 0;
    case WM_CAPTURECHANGED:
        if ((HWND)lp != h) { if (g_mouseDown) FinishDrag(false); g_thumbDrag = g_hthumbDrag = false; EdCancelStroke(); }
        return 0;
    case WM_SYSKEYDOWN:
        // Alt-Kombinationen kommen als WM_SYSKEYDOWN; nur fuer die Hotkey-Erfassung relevant.
        if (g_hotkeyCapture && wp != VK_F10) { HotkeyKeyDown((UINT)wp); return 0; }
        break;
    case WM_KEYUP: case WM_SYSKEYUP:
        // Fuer Druck sendet Windows kein KEYDOWN, nur KEYUP.
        if (g_hotkeyCapture && wp == VK_SNAPSHOT) { HotkeyKeyDown(VK_SNAPSHOT); return 0; }
        break;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (g_page == Page::Settings) { g_setScroll -= delta * S(100) / WHEEL_DELTA; Redraw(); return 0; }
        if (g_page == Page::About) { g_aboutScroll -= delta * S(100) / WHEEL_DELTA; Redraw(); return 0; }
        if (g_page == Page::Edit) {
            if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) EdSetZoom(g_edZoom * (delta > 0 ? 1.1 : 1 / 1.1));
            else if (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) g_edSx -= delta * S(80) / WHEEL_DELTA;
            else g_edSy -= delta * S(80) / WHEEL_DELTA;
            Redraw();
            return 0;
        }
        if (!CurTab()) return 0;
        if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) SetZoom(g_zoom * (delta > 0 ? 1.1 : 1 / 1.1));
        else if (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) ScrollXBy(-delta * S(120) / WHEEL_DELTA);
        else ScrollBy(-delta * S(120) / WHEEL_DELTA);
        if (g_dragging) { g_dropIdx = ComputeDrop(g_lastMouse.y); Redraw(); }
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            if (g_dragging) { SetCursor(LoadCursorW(NULL, IDC_SIZENS)); return TRUE; }
            if (g_page == Page::Edit) {
                POINT cp; GetCursorPos(&cp); ScreenToClient(h, &cp);
                if (PtIn(g_edCanvas, cp) && g_hoverKind == HK::None) {
                    SetCursor(LoadCursorW(NULL, g_tool == Tool::Select ? IDC_ARROW : (g_tool == Tool::Text ? IDC_IBEAM : IDC_CROSS)));
                    return TRUE;
                }
            }
            if (g_hoverKind != HK::None && g_hoverKind != HK::ScrollThumb) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        }
        break;
    case WM_KEYDOWN: if (OnKeyDown(wp)) return 0; break;
    case WM_CHAR: OnChar(wp); return 0;
    case WM_HOTKEY: if (wp == ID_HOTKEY) BeginCapture(CaptureMode::Append, -1); return 0;
    case WM_TIMER:
        if (wp == IDT_AUTOSCROLL && g_dragging && g_autoDir) {
            ScrollBy(g_autoDir * S(14));
            g_dropIdx = ComputeDrop(g_lastMouse.y);
        }
        else if (wp == IDT_FLASH) { KillTimer(h, IDT_FLASH); g_flash.clear(); Redraw(); }
        return 0;
    case WM_OCR_RESULT: OnOcrResult(wp, lp); return 0;
    case WM_TRAYICON:
        switch (LOWORD(lp)) {
        case WM_RBUTTONUP: case WM_CONTEXTMENU: ShowTrayMenu(); break;
        case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: ShowMain(); break;
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_SNIP: BeginCapture(CaptureMode::Append, -1); break;
        case IDM_SHOW: if (IsWindowVisible(h)) ShowWindow(h, SW_HIDE); else ShowMain(); break;
        case IDM_EXIT: ExitApp(); break;
        default:
            if (LOWORD(wp) >= IDM_DELAY_BASE && LOWORD(wp) < IDM_DELAY_BASE + kDelayCount)
                SetActiveDelay(kDelayChoices[LOWORD(wp) - IDM_DELAY_BASE]);
            else if (LOWORD(wp) >= IDM_AUTOCOPY_BASE && LOWORD(wp) < IDM_AUTOCOPY_BASE + 3)
                SetActiveAutoCopy(LOWORD(wp) - IDM_AUTOCOPY_BASE);
            else if (LOWORD(wp) >= IDM_TAB_BASE && LOWORD(wp) < IDM_TAB_BASE + (int)g_app.tabs.size()) {
                g_page = Page::Main;
                SelectTab(LOWORD(wp) - IDM_TAB_BASE);
            }
            break;
        }
        return 0;
    case WM_CLOSE:
        // Nur verstecken; die App lebt im Tray weiter.
        if (g_editTab >= 0) CommitRename(true);
        SaveWindowState();
        Model_SaveGlobal();
        for (auto& t : g_app.tabs) Model_SaveTab(*t);
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_ENDSESSION:
        if (wp) {
            g_suppressSaveErrors = true;       // beim Herunterfahren keine blockierende Meldungsbox
            SaveWindowState(); Model_SaveGlobal(); for (auto& t : g_app.tabs) Model_SaveTab(*t);
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (lp && wcscmp((const wchar_t*)lp, L"ImmersiveColorSet") == 0 && g_app.def.theme == 0) ApplyTheme();
        break;
    case WM_DESTROY:
        if (g_back) { DeleteObject(g_back); g_back = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------------------------------------------------------- WinMain

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    g_hInst = hInst;
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"MultiSnip.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"MultiSnipMain", NULL);
        if (other) { ShowWindow(other, SW_SHOW); SetForegroundWindow(other); }
        return 0;
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    if (!Image_Init()) {
        MessageBoxW(NULL, TR(S_ERR_GDIPLUS), L"MultiSnip", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!Model_Init()) {
        MessageBoxW(NULL, TR(S_ERR_APPDATA), L"MultiSnip", MB_OK | MB_ICONERROR);
        return 1;
    }
    Model_LoadAll();

    g_icon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    HICON iconSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    g_trayIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = g_icon;
    wc.hIconSm = iconSmall;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"MultiSnipMain";
    RegisterClassExW(&wc);

    // Vorgabegroesse in System-DPI; die gespeicherte Lage wird unten per WINDOWPLACEMENT
    // (Arbeitsbereichs-Koordinaten, wie beim Speichern) wiederhergestellt.
    UINT sysDpi = GetDpiForSystem();
    g_hwnd = CreateWindowExW(0, L"MultiSnipMain", L"MultiSnip", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(960, (int)sysDpi, 96), MulDiv(720, (int)sysDpi, 96), NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;
    g_dpi = (int)GetDpiForWindow(g_hwnd);
    if (g_dpi <= 0) g_dpi = 96;
    CreateFonts();
    ApplyTheme();
    bool placed = false;
    if (g_app.st.w > 200 && g_app.st.h > 150) {
        RECT r{ g_app.st.x, g_app.st.y, g_app.st.x + g_app.st.w, g_app.st.y + g_app.st.h };
        HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONULL);
        if (mon) {
            // Groesse an die DPI des Zielmonitors anpassen, falls sie sich seit dem Speichern geaendert hat
            UINT dx = 96, dy = 96;
            if (g_app.st.dpi > 0 && SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy)) && (int)dx != g_app.st.dpi) {
                r.right = r.left + MulDiv(RW(r), (int)dx, g_app.st.dpi);
                r.bottom = r.top + MulDiv(RH(r), (int)dx, g_app.st.dpi);
            }
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            wp.rcNormalPosition = r;
            wp.showCmd = g_app.st.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
            placed = SetWindowPlacement(g_hwnd, &wp) != FALSE;
        }
    }
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Aktiven Tab bestimmen
    g_active = -1;
    for (size_t i = 0; i < g_app.tabs.size(); i++) if (g_app.tabs[i]->guid == g_app.st.lastTab) g_active = (int)i;
    if (g_active == -1 && !g_app.def.dashboard) {
        if (g_app.tabs.empty()) Model_NewTab();
        g_active = g_app.tabs.empty() ? -1 : 0;
    }

    if (!placed) ShowWindow(g_hwnd, g_app.st.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(g_hwnd);
    TrayAdd();
    RegisterAppHotkey(true);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    TrayRemove();
    g_app.tabs.clear();              // Bitmaps freigeben
    Image_Shutdown();
    CoUninitialize();
    if (mutex) CloseHandle(mutex);
    return 0;
}
