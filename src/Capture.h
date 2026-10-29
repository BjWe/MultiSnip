#pragma once
#include <windows.h>

enum class CaptureMode { Append, Replace };

// bmp == nullptr bedeutet Abbruch. Der Empfaenger uebernimmt den Besitz der Bitmap.
typedef void (*CaptureDoneFn)(HBITMAP bmp, int w, int h, CaptureMode mode, int replaceIndex, void* ctx);

// Ein einziger Capture-Pfad mit Modus. Der Aufrufer blendet sein Fenster vorher
// selbst aus; nach Abschluss/Abbruch wird immer genau einmal `done` gerufen.
void Capture_Begin(HINSTANCE hInst, int delayMs, bool includeCursor,
    CaptureMode mode, int replaceIndex, CaptureDoneFn done, void* ctx);
bool Capture_IsActive();
bool Capture_IsWaiting();      // Verzoegerung laeuft noch (kein Overlay); kann mit Capture_Cancel abgebrochen werden
void Capture_Cancel();         // ruft `done` mit nullptr
