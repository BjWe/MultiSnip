#pragma once
#include <windows.h>
#include <string>

// Kleine GDI-Helfer, von allen Modulen gemeinsam genutzt.
void FillSolid(HDC dc, const RECT& r, COLORREF c);
void FillAlpha(HDC dc, const RECT& r, COLORREF c, BYTE alpha);
void FrameRectColor(HDC dc, const RECT& r, COLORREF c, int width = 1);
void DrawRoundBox(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius);   // GDI+, geglaettet
void DrawEllipseAA(HDC dc, const RECT& r, COLORREF border, float width);                  // nur Kontur, geglaettet
void DrawRoundFrameAA(HDC dc, const RECT& r, COLORREF border, float width, int radius);   // nur Kontur (Fokusring)
void DrawRoundBoxGdi(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius); // reines GDI (fuer 1bpp-Masken)
void DrawTextIn(HDC dc, const RECT& r, const std::wstring& s, HFONT font, COLORREF color, UINT fmt);
int  TextWidth(HDC dc, HFONT font, const std::wstring& s);
bool FontExists(const wchar_t* face);                       // Schriftfamilie installiert?
bool GlyphExists(HDC dc, HFONT font, wchar_t ch);           // Zeichen in der Schrift vorhanden?

inline RECT MakeRect(int l, int t, int r, int b) { RECT rc{ l, t, r, b }; return rc; }
inline int RW(const RECT& r) { return r.right - r.left; }
inline int RH(const RECT& r) { return r.bottom - r.top; }
inline bool PtIn(const RECT& r, POINT p) { return PtInRect(&r, p) != FALSE; }
