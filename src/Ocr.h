#pragma once
#include <windows.h>
#include <string>

// Texterkennung mit der in Windows enthaltenen OCR (Windows.Media.Ocr).
// Laeuft in einem Arbeitsthread; das Ergebnis kommt als Fensternachricht:
//   msg, wParam = Statuscode (OcrStatus), lParam = std::wstring* (Besitz geht an den Empfaenger, kann nullptr sein)
enum class OcrStatus { Ok = 0, NoText, NoLanguage, Failed };

bool Ocr_Available();                               // mindestens eine OCR-Sprache installiert
std::wstring Ocr_LanguageName();                    // Anzeigename der verwendeten Sprache ("" = keine)
// bmp: 32-bpp-DIB (top-down), w/h Pixel. Die Pixel werden sofort kopiert, bmp darf danach freigegeben werden.
bool Ocr_Start(HBITMAP bmp, int w, int h, HWND notify, UINT msg);
