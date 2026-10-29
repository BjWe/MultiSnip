#pragma once
#include <windows.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Annotationen: Ebenen mit Vektor-Formen ueber einem Snippet. Koordinaten sind
// immer Bildpixel des Snippets (unabhaengig vom Zoom der Darstellung).
// ---------------------------------------------------------------------------

enum class ShapeType { Pen = 0, Marker, Line, Arrow, Rect, Ellipse, Text, Number };

struct AnnPoint { float x = 0, y = 0; };

struct Shape {
    ShapeType type = ShapeType::Pen;
    COLORREF color = RGB(232, 17, 35);
    int width = 3;                  // Strichstaerke in Bildpixeln (1 = duenn, 3 = mittel, 6 = dick)
    std::vector<AnnPoint> pts;      // Pen/Marker: Polylinie; Line/Arrow/Rect/Ellipse: 2 Punkte; Text/Number: Anker
    std::wstring text;              // Text-Werkzeug
    int number = 0;                 // Nummern-Badge
};

struct Layer {
    std::wstring name;
    bool visible = true;
    std::vector<Shape> shapes;
};

struct Snippet;

// Zeichnet alle sichtbaren Ebenen in den DC. ox/oy = Bildursprung in DC-Pixeln, zoom = Darstellungsfaktor.
void Annot_Render(HDC dc, const Snippet& s, double zoom, int ox, int oy);
// Wie Annot_Render, aber nur der Bildausschnitt `view` (Bildpixel) liegt bei ox/oy; alles ausserhalb wird abgeschnitten.
void Annot_RenderView(HDC dc, const Snippet& s, double zoom, int ox, int oy, const RECT& view);
// Zeichnet eine einzelne Form (z. B. die gerade entstehende) mit Clip auf das Bild.
void Annot_RenderShape(HDC dc, const Shape& sh, int imgW, int imgH, double zoom, int ox, int oy);
// Trefferpruefung in einer Ebene (Index der obersten getroffenen Form oder -1). tol in Bildpixeln.
int  Annot_HitTest(const Layer& layer, AnnPoint p, float tol);
RECT Annot_Bounds(const Shape& sh);         // Bounding-Box in Bildpixeln
void Annot_Move(Shape& sh, float dx, float dy);
int  Annot_FontPx(int width);               // Textgroesse fuer die Strichstaerke
