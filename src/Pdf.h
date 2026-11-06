#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Model.h"

// ---------------------------------------------------------------------------
// PDF-Export. Schreibt ein PDF 1.4 von Hand; es wird keine Bibliothek gebraucht.
// Die Bilder gehen verlustfrei hinein: GDI+ liefert ein PNG, dessen IDAT-Daten
// sich unveraendert als /FlateDecode mit /Predictor 15 einbetten lassen - PDF
// kennt genau die Zeilenfilter, die PNG benutzt. Siehe Pdf.cpp.
// ---------------------------------------------------------------------------

enum class PdfPages {
    PerSnippet = 0,     // ein Ausschnitt pro Seite
    Single = 1,         // alle Ausschnitte auf einer Seite (das Gesamtbild)
};

enum class PdfPaper {
    A4 = 0,             // A4, Bild eingepasst, Querformat automatisch
    ImageSize = 1,      // Seite genau in Bildgroesse (1 px = 1/96 Zoll)
};

struct PdfOptions {
    PdfPages pages = PdfPages::PerSnippet;
    PdfPaper paper = PdfPaper::A4;
    // PDF/A-1b: XMP-Kennung, Dokumentmetadaten und ein OutputIntent mit
    // eingebettetem sRGB-Profil. Schlaegt mit ERROR_NOT_FOUND fehl, wenn das
    // Systemfarbprofil nicht lesbar ist - lieber kein PDF als ein falsch
    // ausgezeichnetes.
    bool pdfA = false;
    std::wstring title;                 // Dokumenttitel; leer = Dateiname des Tabs
};

// Kern: fertige Bitmaps (32-bpp-DIBs) werden zu Seiten in der uebergebenen
// Reihenfolge. Nimmt den Bitmaps nichts ab und gibt sie nicht frei.
// Atomar: schreibt nach <path>.tmp und benennt um.
bool Pdf_WriteBitmaps(const std::vector<HBITMAP>& pages, const PdfOptions& o, const std::wstring& path);

// Baut die Seiten aus dem Tab (je nach o.pages) und schreibt sie; pageCount
// nimmt die Zahl der geschriebenen Seiten auf.
// false bei leerem Tab oder Schreibfehler; GetLastError() ist dann gesetzt.
bool Pdf_ExportTab(const Tab& t, const PdfOptions& o, const std::wstring& path, int* pageCount = nullptr);
