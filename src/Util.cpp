#include "Util.h"
#include <algorithm>
#include <objidl.h>
using std::min;
using std::max;
#include <gdiplus.h>

void FillSolid(HDC dc, const RECT& r, COLORREF c)
{
    COLORREF old = SetBkColor(dc, c);
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &r, L"", 0, NULL);
    SetBkColor(dc, old);
}

void FillAlpha(HDC dc, const RECT& r, COLORREF c, BYTE alpha)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 1;
    bi.bmiHeader.biHeight = 1;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp) return;
    *(DWORD*)bits = 0xFF000000u | ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | GetBValue(c);
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ o = SelectObject(m, bmp);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, 0 };
    AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, m, 0, 0, 1, 1, bf);
    SelectObject(m, o);
    DeleteDC(m);
    DeleteObject(bmp);
}

void FrameRectColor(HDC dc, const RECT& r, COLORREF c, int width)
{
    HBRUSH b = CreateSolidBrush(c);
    RECT rr = r;
    for (int i = 0; i < width; i++) { FrameRect(dc, &rr, b); InflateRect(&rr, -1, -1); }
    DeleteObject(b);
}

static Gdiplus::Color GpColor(COLORREF c) { return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c)); }

// Pfad fuer ein abgerundetes Rechteck; wird der Radius groesser als die halbe Kante, entsteht eine Ellipse.
static void RoundPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius)
{
    float d = radius * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    if (d <= 0) { path.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

void DrawRoundBox(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    // 1px-Rand liegt innerhalb des Rechtecks: Pfad um einen halben Pixel einruecken
    float x = r.left + 0.5f, y = r.top + 0.5f, w = (float)(r.right - r.left) - 1, h = (float)(r.bottom - r.top) - 1;
    Gdiplus::GraphicsPath path;
    RoundPath(path, x, y, w, h, (float)radius);
    Gdiplus::SolidBrush b(GpColor(fill));
    g.FillPath(&b, &path);
    if (border != fill) {
        Gdiplus::Pen p(GpColor(border), 1.0f);
        g.DrawPath(&p, &path);
    }
    else {
        // gleiche Farbe: Rand mitfuellen, damit die Kante nicht blass bleibt
        Gdiplus::Pen p(GpColor(fill), 1.0f);
        g.DrawPath(&p, &path);
    }
}

void DrawRoundBoxGdi(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

void DrawRoundFrameAA(HDC dc, const RECT& r, COLORREF border, float width, int radius)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float x = r.left + width / 2, y = r.top + width / 2;
    float w = (float)(r.right - r.left) - width, h = (float)(r.bottom - r.top) - width;
    Gdiplus::GraphicsPath path;
    RoundPath(path, x, y, w, h, (float)radius);
    Gdiplus::Pen p(GpColor(border), width);
    g.DrawPath(&p, &path);
}

void DrawEllipseAA(HDC dc, const RECT& r, COLORREF border, float width)
{
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen p(GpColor(border), width);
    g.DrawEllipse(&p, r.left + width / 2, r.top + width / 2, (float)(r.right - r.left) - width, (float)(r.bottom - r.top) - width);
}

void DrawTextIn(HDC dc, const RECT& r, const std::wstring& s, HFONT font, COLORREF color, UINT fmt)
{
    HGDIOBJ of = SelectObject(dc, font);
    int obk = SetBkMode(dc, TRANSPARENT);
    COLORREF oc = SetTextColor(dc, color);
    RECT rc = r;
    DrawTextW(dc, s.c_str(), -1, &rc, fmt);
    SetTextColor(dc, oc);
    SetBkMode(dc, obk);
    SelectObject(dc, of);
}

static int CALLBACK FontExistsProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp) { *(bool*)lp = true; return 0; }
bool FontExists(const wchar_t* face)
{
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, face);
    bool found = false;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, FontExistsProc, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}

bool GlyphExists(HDC dc, HFONT font, wchar_t ch)
{
    HGDIOBJ of = SelectObject(dc, font);
    WORD idx = 0xFFFF;
    DWORD n = GetGlyphIndicesW(dc, &ch, 1, &idx, GGI_MARK_NONEXISTING_GLYPHS);
    SelectObject(dc, of);
    return n != GDI_ERROR && idx != 0xFFFF;
}

int TextWidth(HDC dc, HFONT font, const std::wstring& s)
{
    HGDIOBJ of = SelectObject(dc, font);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, of);
    return sz.cx;
}
