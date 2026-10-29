#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Model.h"

bool Image_Init();
void Image_Shutdown();

HBITMAP Image_CreateDib(int w, int h, void** bits = nullptr);     // 32bpp top-down
HBITMAP Image_LoadPng(const std::wstring& path, int* w, int* h);  // nullptr bei Fehler
bool    Image_PngSize(const std::wstring& path, int* w, int* h);  // nur IHDR lesen, ohne Dekodieren
bool    Image_SavePng(HBITMAP bmp, const std::wstring& path);     // atomar (tmp + rename)

// Gemeinsame Layout-Berechnung fuer Vorschau, Clipboard und Datei-Export.
struct LayoutItem { Snippet* s; int x; int y; };
struct Layout { std::vector<LayoutItem> items; int width = 0; int height = 0; };
Layout  Image_ComputeLayout(const Tab& t, bool includeHidden);

HBITMAP Image_BuildComposite(const Tab& t, int* w, int* h);      // nullptr wenn nichts sichtbar
HBITMAP Image_RenderSnippet(const Snippet& s, bool withAnnotations = true);   // ein Ausschnitt (Zuschnitt) in Originalpixeln
void    Image_FixAlpha(HBITMAP dib);                              // Alphakanal eines 32-bpp-DIB auf 255
bool    Image_CopyToClipboard(HWND owner, HBITMAP bmp, int w, int h);
