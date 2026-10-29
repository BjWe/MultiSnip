#include "Annot.h"
#include "Model.h"
#include <algorithm>
#include <cmath>
#include <objidl.h>
using std::min;
using std::max;
#include <gdiplus.h>

using namespace Gdiplus;

int Annot_FontPx(int width) { return width <= 1 ? 14 : (width <= 3 ? 20 : 30); }

static Color GpColor(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

static float MarkerWidth(int w) { return (float)max(12, w * 5); }
static float BadgeRadius(int w) { return (float)(10 + w * 2); }

static void DrawShape(Graphics& g, const Shape& sh)
{
    if (sh.pts.empty()) return;
    switch (sh.type) {
    case ShapeType::Pen:
    case ShapeType::Marker: {
        bool marker = sh.type == ShapeType::Marker;
        Pen pen(GpColor(sh.color, marker ? 110 : 255), marker ? MarkerWidth(sh.width) : (float)sh.width);
        pen.SetLineCap(marker ? LineCapFlat : LineCapRound, marker ? LineCapFlat : LineCapRound, DashCapRound);
        pen.SetLineJoin(LineJoinRound);
        if (sh.pts.size() == 1) {
            float r = (marker ? MarkerWidth(sh.width) : (float)sh.width) / 2;
            SolidBrush b(GpColor(sh.color, marker ? 110 : 255));
            g.FillEllipse(&b, sh.pts[0].x - r, sh.pts[0].y - r, 2 * r, 2 * r);
        }
        else {
            std::vector<PointF> p;
            p.reserve(sh.pts.size());
            for (auto& q : sh.pts) p.push_back(PointF(q.x, q.y));
            g.DrawLines(&pen, p.data(), (INT)p.size());
        }
        break;
    }
    case ShapeType::Line:
    case ShapeType::Arrow: {
        if (sh.pts.size() < 2) return;
        Pen pen(GpColor(sh.color), (float)sh.width);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        if (sh.type == ShapeType::Arrow) {
            AdjustableArrowCap cap(3.0f + sh.width * 0.6f, 2.5f + sh.width * 0.5f, true);
            pen.SetCustomEndCap(&cap);
        }
        g.DrawLine(&pen, sh.pts[0].x, sh.pts[0].y, sh.pts[1].x, sh.pts[1].y);
        break;
    }
    case ShapeType::Rect:
    case ShapeType::Ellipse: {
        if (sh.pts.size() < 2) return;
        Pen pen(GpColor(sh.color), (float)sh.width);
        pen.SetLineJoin(LineJoinRound);
        float x = min(sh.pts[0].x, sh.pts[1].x), y = min(sh.pts[0].y, sh.pts[1].y);
        float w = fabsf(sh.pts[1].x - sh.pts[0].x), h = fabsf(sh.pts[1].y - sh.pts[0].y);
        if (sh.type == ShapeType::Rect) g.DrawRectangle(&pen, x, y, w, h);
        else g.DrawEllipse(&pen, x, y, w, h);
        break;
    }
    case ShapeType::Text: {
        if (sh.text.empty()) return;
        Font font(L"Segoe UI", (REAL)Annot_FontPx(sh.width), FontStyleBold, UnitPixel);
        SolidBrush b(GpColor(sh.color));
        // leichte helle Kontur fuer Lesbarkeit auf beliebigem Hintergrund
        SolidBrush halo(Color(170, 255, 255, 255));
        PointF at(sh.pts[0].x, sh.pts[0].y);
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++)
            if (dx || dy) g.DrawString(sh.text.c_str(), -1, &font, PointF(at.X + dx, at.Y + dy), &halo);
        g.DrawString(sh.text.c_str(), -1, &font, at, &b);
        break;
    }
    case ShapeType::Number: {
        float r = BadgeRadius(sh.width);
        SolidBrush b(GpColor(sh.color));
        Pen ring(Color(255, 255, 255, 255), 2.0f);
        g.FillEllipse(&b, sh.pts[0].x - r, sh.pts[0].y - r, 2 * r, 2 * r);
        g.DrawEllipse(&ring, sh.pts[0].x - r, sh.pts[0].y - r, 2 * r, 2 * r);
        Font font(L"Segoe UI", r * 1.1f, FontStyleBold, UnitPixel);
        SolidBrush white(Color(255, 255, 255, 255));
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        RectF box(sh.pts[0].x - r, sh.pts[0].y - r, 2 * r, 2 * r);
        g.DrawString(std::to_wstring(sh.number).c_str(), -1, &font, box, &fmt, &white);
        break;
    }
    }
}

static void SetupGraphics(Graphics& g, int imgW, int imgH, double zoom, int ox, int oy)
{
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetClip(Rect(ox, oy, (INT)ceil(imgW * zoom), (INT)ceil(imgH * zoom)));
    g.TranslateTransform((REAL)ox, (REAL)oy);
    g.ScaleTransform((REAL)zoom, (REAL)zoom);
}

void Annot_RenderView(HDC dc, const Snippet& s, double zoom, int ox, int oy, const RECT& view)
{
    bool any = false;
    for (auto& L : s.layers) if (L.visible && !L.shapes.empty()) { any = true; break; }
    if (!any) return;
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    int vw = view.right - view.left, vh = view.bottom - view.top;
    g.SetClip(Rect(ox, oy, (INT)ceil(vw * zoom), (INT)ceil(vh * zoom)));
    g.TranslateTransform((REAL)(ox - view.left * zoom), (REAL)(oy - view.top * zoom));
    g.ScaleTransform((REAL)zoom, (REAL)zoom);
    for (auto& L : s.layers) {
        if (!L.visible) continue;
        for (auto& sh : L.shapes) DrawShape(g, sh);
    }
}

void Annot_Render(HDC dc, const Snippet& s, double zoom, int ox, int oy)
{
    RECT full{ 0, 0, s.w, s.h };
    Annot_RenderView(dc, s, zoom, ox, oy, full);
}

void Annot_RenderShape(HDC dc, const Shape& sh, int imgW, int imgH, double zoom, int ox, int oy)
{
    Graphics g(dc);
    SetupGraphics(g, imgW, imgH, zoom, ox, oy);
    DrawShape(g, sh);
}

RECT Annot_Bounds(const Shape& sh)
{
    RECT r{ 0, 0, 0, 0 };
    if (sh.pts.empty()) return r;
    float x0 = sh.pts[0].x, y0 = sh.pts[0].y, x1 = x0, y1 = y0;
    for (auto& p : sh.pts) { x0 = min(x0, p.x); y0 = min(y0, p.y); x1 = max(x1, p.x); y1 = max(y1, p.y); }
    float pad = (float)sh.width;
    switch (sh.type) {
    case ShapeType::Marker: pad = MarkerWidth(sh.width) / 2; break;
    case ShapeType::Number: pad = BadgeRadius(sh.width); break;
    case ShapeType::Text: {
        float fs = (float)Annot_FontPx(sh.width);
        x1 = x0 + fs * 0.6f * (float)max<size_t>(1, sh.text.size());
        y1 = y0 + fs * 1.4f;
        pad = 2;
        break;
    }
    default: break;
    }
    r.left = (LONG)floorf(x0 - pad); r.top = (LONG)floorf(y0 - pad);
    r.right = (LONG)ceilf(x1 + pad); r.bottom = (LONG)ceilf(y1 + pad);
    return r;
}

static float SegDist(AnnPoint a, AnnPoint b, AnnPoint p)
{
    float vx = b.x - a.x, vy = b.y - a.y;
    float len2 = vx * vx + vy * vy;
    float t = len2 > 0 ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / len2 : 0;
    t = max(0.0f, min(1.0f, t));
    float cx = a.x + t * vx - p.x, cy = a.y + t * vy - p.y;
    return sqrtf(cx * cx + cy * cy);
}

static bool HitShape(const Shape& sh, AnnPoint p, float tol)
{
    if (sh.pts.empty()) return false;
    switch (sh.type) {
    case ShapeType::Pen:
    case ShapeType::Marker: {
        float half = (sh.type == ShapeType::Marker ? MarkerWidth(sh.width) : (float)sh.width) / 2 + tol;
        if (sh.pts.size() == 1) return SegDist(sh.pts[0], sh.pts[0], p) <= half;
        for (size_t i = 1; i < sh.pts.size(); i++) if (SegDist(sh.pts[i - 1], sh.pts[i], p) <= half) return true;
        return false;
    }
    case ShapeType::Line:
    case ShapeType::Arrow:
        return sh.pts.size() >= 2 && SegDist(sh.pts[0], sh.pts[1], p) <= sh.width / 2.0f + tol;
    case ShapeType::Rect: {
        // Nur die Kontur zaehlt, damit Formen innerhalb eines grossen Rahmens erreichbar bleiben.
        if (sh.pts.size() < 2) return false;
        float x0 = min(sh.pts[0].x, sh.pts[1].x), x1 = max(sh.pts[0].x, sh.pts[1].x);
        float y0 = min(sh.pts[0].y, sh.pts[1].y), y1 = max(sh.pts[0].y, sh.pts[1].y);
        float half = sh.width / 2.0f + tol;
        AnnPoint a{ x0, y0 }, b{ x1, y0 }, c{ x1, y1 }, d{ x0, y1 };
        return SegDist(a, b, p) <= half || SegDist(b, c, p) <= half || SegDist(c, d, p) <= half || SegDist(d, a, p) <= half;
    }
    case ShapeType::Ellipse: {
        if (sh.pts.size() < 2) return false;
        float cx = (sh.pts[0].x + sh.pts[1].x) / 2, cy = (sh.pts[0].y + sh.pts[1].y) / 2;
        float rx = fabsf(sh.pts[1].x - sh.pts[0].x) / 2, ry = fabsf(sh.pts[1].y - sh.pts[0].y) / 2;
        float half = sh.width / 2.0f + tol;
        if (rx < 1 || ry < 1) return SegDist(sh.pts[0], sh.pts[1], p) <= half;
        // Abstand zur Kontur naeherungsweise ueber den normierten Radius
        float nx = (p.x - cx) / rx, ny = (p.y - cy) / ry;
        float r = sqrtf(nx * nx + ny * ny);
        return fabsf(r - 1.0f) * min(rx, ry) <= half;
    }
    case ShapeType::Text: {
        RECT r = Annot_Bounds(sh);
        return p.x >= r.left - tol && p.x <= r.right + tol && p.y >= r.top - tol && p.y <= r.bottom + tol;
    }
    case ShapeType::Number: {
        float dx = p.x - sh.pts[0].x, dy = p.y - sh.pts[0].y;
        return sqrtf(dx * dx + dy * dy) <= BadgeRadius(sh.width) + tol;
    }
    }
    return false;
}

int Annot_HitTest(const Layer& layer, AnnPoint p, float tol)
{
    for (int i = (int)layer.shapes.size() - 1; i >= 0; i--)
        if (HitShape(layer.shapes[i], p, tol)) return i;
    return -1;
}

void Annot_Move(Shape& sh, float dx, float dy)
{
    for (auto& p : sh.pts) { p.x += dx; p.y += dy; }
}
