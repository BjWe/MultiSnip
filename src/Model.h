#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include "Annot.h"

// ---------------------------------------------------------------------------
// Datenmodell. Strikte Trennung von Daten und Darstellung: Das Gesamtbild wird
// nie gespeichert, sondern immer frisch aus den sichtbaren Snippets berechnet.
// ---------------------------------------------------------------------------

struct Snippet {
    std::wstring file;          // PNG-Dateiname im Tab-Ordner
    bool visible = true;        // Sichtbarkeits-Flag (Checkbox)
    HBITMAP bmp = nullptr;      // nur Laufzeit; nullptr = (noch) nicht geladen oder Datei defekt
    int w = 0, h = 0;           // aus dem PNG-Header, auch ohne geladene Bitmap bekannt (0 = unbekannt)
    bool loadFailed = false;    // letzter Ladeversuch schlug fehl (Datei fehlt/defekt/gesperrt)
    bool unsaved = false;       // PNG konnte nicht geschrieben werden; Bitmap bleibt im Speicher, Speichern wird wiederholt
    std::vector<Layer> layers;  // Annotations-Ebenen (eigene Datei <file>.ann.ini)
    // Zuschnitt in Bildpixeln (leer = keiner). Das PNG bleibt unveraendert; Vorschau, Export,
    // Zwischenablage und OCR zeigen nur diesen Ausschnitt. Ebenen liegen auf dem Vollbild.
    RECT crop{};
    bool HasCrop() const { return crop.right > crop.left && crop.bottom > crop.top; }
    RECT View() const { return HasCrop() ? crop : RECT{ 0, 0, w, h }; }
    int VW() const { return HasCrop() ? crop.right - crop.left : w; }
    int VH() const { return HasCrop() ? crop.bottom - crop.top : h; }
    // Darstellungs-Cache: verkleinerte Kopie fuer die Vorschau (nur Laufzeit)
    HBITMAP cache = nullptr;
    double cacheZoom = 0;
    int cacheW = 0, cacheH = 0;
    RECT cacheCrop{};

    Snippet() = default;
    Snippet(const Snippet&) = delete;
    Snippet& operator=(const Snippet&) = delete;
    ~Snippet();                                   // RAII: Bitmaps freigeben
    void SetBitmap(HBITMAP b, int width, int height); // uebernimmt Besitz, gibt alte frei
    void FreeCache();
    void Unload();                                // Bitmap + Cache freigeben, Groesse bleibt bekannt
};

struct Tab {
    std::wstring guid;          // zugleich Ordnername
    std::wstring title;
    int order = 0;              // globale Reihenfolge
    COLORREF fill = RGB(255, 255, 255);
    int padX = 8;
    int padY = 8;
    int delayMs = 0;            // Verzoegerung vor dem Einfrieren (pro Tab)
    int autoCopy = 0;           // nach der Aufnahme kopieren: 0 = nichts, 1 = neuer Ausschnitt, 2 = Gesamtbild des Tabs
    int numberScope = 0;        // Nummern-Werkzeug zaehlt: 0 = pro Ausschnitt, 1 = durch den ganzen Tab
    int scroll = 0;             // Scroll-Offset (Pixel)
    int scrollX = 0;            // horizontaler Scroll-Offset (nur Laufzeit)
    bool loaded = false;        // Bitmaps der Snippets sind im Speicher (nur der aktive Tab)
    // Die Listenreihenfolge ist die EINZIGE Wahrheit fuer die Stapelreihenfolge.
    std::vector<std::unique_ptr<Snippet>> snippets;

    std::wstring Dir() const;
    std::wstring IniPath() const;
    int VisibleCount() const;
};

struct Defaults {
    COLORREF fill = RGB(255, 255, 255);
    int padX = 8;
    int padY = 8;
    int delayMs = 0;
    int autoCopy = 0;           // Vorgabe fuer neue Tabs (siehe Tab::autoCopy)
    int numberScope = 0;        // Vorgabe fuer neue Tabs (siehe Tab::numberScope)
    bool captureCursor = false;
    UINT hotMod = MOD_CONTROL;  // Strg+Druck: kollidiert nicht mit "Speichern unter" (Strg+Umschalt+S)
    UINT hotVk = VK_SNAPSHOT;
    bool dashboard = true;
    bool noAskDelete = false;
    int language = 0;           // 0 = automatisch, 1 = Deutsch, 2 = Englisch, 3 = Spanisch
    int theme = 0;              // 0 = Windows-Einstellung, 1 = hell, 2 = dunkel
};

struct WinState {
    std::wstring lastTab;
    int x = 0, y = 0, w = 0, h = 0;   // w==0 → kein gespeicherter Zustand (Arbeitsbereichs-Koordinaten)
    int dpi = 0;                      // DPI beim Speichern, um die Groesse beim Monitorwechsel zu skalieren
    bool maximized = false;
};

struct AppData {
    std::wstring root;          // %LocalAppData%\MultiSnip (aeltere Versionen: %AppData%, wird migriert)
    std::vector<std::unique_ptr<Tab>> tabs;
    Defaults def;
    WinState st;
};

extern AppData g_app;
extern bool g_suppressSaveErrors;       // beim Herunterfahren: keine Meldungsboxen

bool Model_Init();                       // Wurzelordner bestimmen/anlegen
void Model_LoadAll();                    // globale INI + alle Tab-Ordner (nur Metadaten, keine Bitmaps)
void Model_LoadTabBitmaps(Tab& t);       // Bitmaps des Tabs nachladen (fehlgeschlagene werden erneut versucht)
void Model_UnloadTabBitmaps(Tab& t);     // Bitmaps freigeben; Snippets bleiben mit Groesse erhalten
bool Model_HotkeyValid(UINT mod, UINT vk); // Modifikator noetig, ausser F-Tasten/Druck
bool Model_SaveGlobal();                 // atomar; Fehler → Meldungsbox
bool Model_SaveTab(const Tab& t);        // atomar; Fehler → Meldungsbox
Tab* Model_NewTab();                     // erbt [Defaults]; nullptr bei Fehler
void Model_DeleteTab(size_t idx);        // Ordner samt Inhalt loeschen
bool Model_SaveSnippetPng(const Tab& t, const Snippet& s);
bool Model_SaveAnnot(const Tab& t, const Snippet& s);    // Ebenen atomar speichern (loescht Datei, wenn leer)
void Model_LoadAnnot(const Tab& t, Snippet& s);
void Model_DeleteSnippet(Tab& t, size_t idx);
std::wstring Model_NewSnippetFile(const Tab& t);
std::wstring NewGuid();
void ShowSaveError(const std::wstring& what, DWORD err);
