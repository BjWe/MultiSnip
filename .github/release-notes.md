## Installation

| Datei | Wofür |
|---|---|
| `MultiSnip-{{VERSION}}-x64.msi` | Installer für 64-Bit-Windows (empfohlen) |
| `MultiSnip-{{VERSION}}-x86.msi` | Installer für 32-Bit-Windows |
| `MultiSnip-{{VERSION}}-x64.exe` | Einzelne EXE, portabel, ohne Installation |
| `MultiSnip-{{VERSION}}-x86.exe` | dieselbe EXE für 32-Bit-Windows |

Das MSI installiert nach `%ProgramFiles%\MultiSnip`, legt eine Verknüpfung im
Startmenü an und ersetzt eine ältere Installation. Die EXEs sind statisch gegen
die CRT gelinkt und brauchen keine Visual-C++-Redistributable.

Einstellungen und Ausschnitte liegen unter `%LocalAppData%\MultiSnip` und bleiben
beim Deinstallieren erhalten.

MultiSnip braucht Windows 10 ab Version 1809 (Build 17763); die Texterkennung
setzt zusätzlich ein Windows-Sprachpaket mit OCR voraus.

Die Pakete sind nicht signiert, darum zeigt Windows beim ersten Start
gegebenenfalls einen SmartScreen-Hinweis („Weitere Informationen“ →
„Trotzdem ausführen“).
