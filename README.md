# MultiSnip

Schlankes Windows-Snipping-Tool in C++ mit nackter Win32-API. Ausschnitte werden
pro Tab untereinander zu einem Verlauf gestapelt, bleiben einzeln verwaltbar und
lassen sich als ein Gesamtbild kopieren oder als PNG speichern.

## Installieren

Fertige Pakete liegen unter [Releases](https://github.com/BjWe/MultiSnip/releases):

| Datei | Wofür |
|---|---|
| `MultiSnip-<version>-x64.msi` | Installer für 64-Bit-Windows (empfohlen) |
| `MultiSnip-<version>-x86.msi` | Installer für 32-Bit-Windows |
| `MultiSnip-<version>-x64.exe` | Einzelne EXE, portabel, ohne Installation |

Das MSI installiert nach `%ProgramFiles%\MultiSnip`, legt eine Verknüpfung im Startmenü an,
beendet eine noch laufende Instanz und ersetzt eine ältere Installation. Deinstalliert wird über
„Apps & Features“; `%LocalAppData%\MultiSnip` mit Einstellungen und Ausschnitten bleibt dabei
erhalten. Still installieren geht mit `msiexec /i MultiSnip-<version>-x64.msi /qn`.

Vorausgesetzt wird Windows 10 ab Version 1809 (Build 17763); die Texterkennung braucht
zusätzlich ein Windows-Sprachpaket mit OCR. Die Pakete sind nicht signiert, darum zeigt Windows
beim ersten Start gegebenenfalls einen SmartScreen-Hinweis.

## Bauen

Visual Studio 2022 oder neuer mit Windows 10 SDK (ab 10.0.17763, wegen der C++/WinRT-Header für
die OCR). Keine weiteren Abhängigkeiten.

```bash
msbuild MultiSnip.sln -p:Configuration=Release -p:Platform=x64
```

Für 32-Bit `-p:Platform=x86`. Die EXE liegt danach unter `bin\<Platform>\Release\MultiSnip.exe`
(Release ist statisch gegen die CRT gelinkt, es wird keine VC-Redistributable gebraucht).
Das Projekt gibt kein Toolset vor, es nimmt das der benutzten Visual-Studio-Version;
mit `-p:PlatformToolset=v143` lässt sich eines erzwingen.

### MSI bauen

```bash
powershell -ExecutionPolicy Bypass -File installer/build-msi.ps1 -Platform x64,x86
```

Das Skript holt sich beim ersten Lauf das [WiX-3.14-Toolset](https://github.com/wixtoolset/wix3)
nach `build\.tools\wix3` (braucht kein .NET SDK und keine Installation), übersetzt die Solution
und legt die Pakete als `dist\MultiSnip-<version>-<platform>.msi` ab. `-SkipBuild` überspringt
das Übersetzen, wenn die EXE schon vorliegt. Die Version kommt dabei wie überall sonst aus
`src/resource.h`.

Das Paket selbst ist in [installer/MultiSnip.wxs](installer/MultiSnip.wxs) beschrieben
(Per-Machine-Installation, Lizenz- und Zielordner-Dialog, Startmenü-Verknüpfung, Major-Upgrade).

### Release

[.github/workflows/build.yml](.github/workflows/build.yml) baut EXE und MSI für x64 und x86 bei
jedem Push. Ein Tag `v<version>` löst [release.yml](.github/workflows/release.yml) aus: der
Workflow prüft, dass der Tag zu `APP_VERSION_STR` in `src/resource.h` passt, baut beide
Plattformen und hängt MSIs und EXEs an ein GitHub-Release.

```bash
git tag v1.1.0 && git push origin v1.1.0
```

## Bedienung

| Aktion | Wie |
|---|---|
| Neuer Ausschnitt | Hotkey (Standard `Strg+Druck`), Knopf „Neuer Snip“, Tray-Menü, `Strg+N` |
| Verzögerung des Tabs | Pfeil am Split-Button „Neuer Snip“ (0 bis 10 s) oder Tray-Menü; gilt pro Tab. Während der Wartezeit zeigt eine Karte den Countdown; Hotkey erneut oder Klick auf die Karte bricht ab |
| Füllfarbe / Rand / Abstand des Tabs | Farbfeld und Spinner rechts in der CommandBar (Umschalt: 10er-Schritte) |
| Nach dem Snip kopieren | Pro Tab: Nichts / nur der neue Ausschnitt / Gesamtbild des Tabs. Umschalten im Split-Button-Menü, im Tray-Menü oder in den Einstellungen (dort auch die Vorgabe für neue Tabs). Die Statuszeile bestätigt das Kopieren |
| Auswahl abbrechen | `Esc`, Rechtsklick, zu kleine Auswahl, Fokusverlust |
| Ausschnitt ausblenden | Checkbox in der Steuerleiste |
| Ausschnitt ersetzen | Knopf „Ersetzen“ (Capture im Ersetzen-Modus, gleiche Datei) |
| Ausschnitt löschen | „x“ in der Steuerleiste (Task-Dialog mit „Nicht mehr nachfragen“) |
| Umsortieren | Ausschnitt mit der Maus ziehen, Einfügemarker zeigt das Ziel |
| Ausschnitt beschriften | Stift-Symbol auf der Karte oder Doppelklick öffnet den Editor (siehe unten) |
| Gesamtbild kopieren | „Kopieren“ oder `Strg+C` (CF_DIB + PNG in der Zwischenablage) |
| Einzelnen Ausschnitt kopieren | Kopieren-Symbol in der Kartenleiste (links vom Stift), mit Ebenen, Originalpixel |
| Text erkennen (OCR) | „A“-Symbol in der Kartenleiste: Windows-Texterkennung im Hintergrund, danach ein Dialog mit dem Text zum Nachbearbeiten und „Alles kopieren“ (nichts wird gespeichert). Braucht ein Windows-Sprachpaket mit Texterkennung |
| Ausschnitt zuschneiden | Werkzeug „Zuschneiden“ (`C`) im Editor: Rechteck aufziehen. Das PNG bleibt unverändert, der Zuschnitt steht als `Crop=l,t,r,b` in der `tab.ini`; Vorschau, Gesamtbild, Einzelkopie und OCR zeigen nur den Zuschnitt. „Zuschnitt aufheben“ in der Editor-Leiste |
| Gesamtbild speichern | „Speichern als PNG…“ oder `Strg+S` |
| Tab anlegen / umbenennen | „+“ bzw. Doppelklick auf den Tab, `F2`, Knopf „Umbenennen“ |
| Tab wechseln | Klick, `Strg+Tab` / `Strg+Umschalt+Tab`; passen nicht alle Tabs in die Leiste, listet „…“ rechts alle auf |
| Zoom der Vorschau | `Strg+Mausrad`, `Strg++`/`Strg+-`, `Strg+0`, Knöpfe in der Statuszeile |
| Scrollen | Mausrad senkrecht, `Umschalt+Mausrad` waagerecht (breite Ausschnitte), Scrollbalken rechts und unten |
| Fenster schließen | versteckt nur im Tray; Beenden über das Tray-Menü |

## Tastaturnavigation

| Taste | Wirkung |
|---|---|
| `Tab` / `Umschalt+Tab` | Fokus durch alle Bedienelemente (Tabs, CommandBar, Karten, Statuszeile, Einstellungen, Editor); ein Akzentring zeigt den Fokus, ein Mausklick blendet ihn aus |
| `Enter` | fokussiertes Element auslösen; auf einer Karte: Editor öffnen |
| `Leertaste` | wie Enter; auf einer Karte: Sichtbarkeit umschalten |
| `↑` / `↓` | mit Fokus auf einer Karte: zur vorigen/nächsten Karte (scrollt mit); in Einstellungen/Editor: Fokus bewegen; sonst scrollen |
| `Strg+↑` / `Strg+↓` | fokussierte Karte nach oben/unten verschieben |
| `←` / `→` | Spinner ändern, Schalter umschalten, Zoom, Tab-Leiste durchlaufen |
| `Entf` | fokussierte Karte löschen bzw. fokussierten Tab schließen |
| `Strg+C` | mit Fokus auf einer Karte: nur diesen Ausschnitt kopieren, sonst das Gesamtbild |
| `F2` | fokussierten Tab umbenennen |
| `Esc` | Fokusring ausblenden, danach wie bisher (Seite verlassen, Drag abbrechen) |

## Editor (Ebenen und Werkzeuge)

Jeder Ausschnitt kann beliebig viele Ebenen mit Vektor-Formen tragen. Das PNG bleibt unverändert,
die Formen liegen in `<png>.ann.ini` und werden in Vorschau, Zwischenablage und Export mitgerendert.

| Werkzeug | Taste | Hinweis |
|---|---|---|
| Auswählen / Verschieben | `V` | Form anklicken, ziehen; `Entf` löscht; Farbe/Stärke-Klick ändert die Auswahl |
| Stift | `P` | Freihand |
| Marker | `M` | breit, halbtransparent |
| Linie / Pfeil | `L` / `A` | `Umschalt` rastet auf 0°, 45°, 90° |
| Rechteck / Ellipse | `R` / `E` | `Umschalt` erzwingt Quadrat / Kreis |
| Text | `T` | klicken, tippen (`Strg+V` fügt eine Zeile ein), `Enter` übernimmt, `Esc` verwirft |
| Nummer | `N` | fortlaufender Zähler pro Ausschnitt |
| Zuschneiden | `C` | Rechteck aufziehen; der Editor zeigt immer das ganze Bild, der Bereich außerhalb ist abgedunkelt. Ebenen liegen auf dem ungecroppten Bild und werden am Zuschnittrand abgeschnitten |

Der Name des Werkzeugs unter dem Mauszeiger erscheint in der Statuszeile. Rechteck und Ellipse
werden an ihrer Kontur getroffen, Formen darin bleiben auswählbar.
Acht Standardfarben (`1`–`8`), drei Strichstärken, `Strg+Z` / `Strg+Y`, `Strg+Mausrad` zoomt,
`Umschalt+Mausrad` scrollt waagerecht. Ebenen: anlegen, ausblenden, verschieben, löschen,
Doppelklick benennt um. Gezeichnet wird immer in der markierten Ebene.

## Datenablage

`%LocalAppData%\MultiSnip\` (lokal, damit Roaming-Profile und Sync-Clients die Screenshots nicht
mitschleppen; ein Ordner aus älteren Versionen unter `%AppData%` wird beim ersten Start verschoben).

- `MultiSnip.ini` – `[Defaults]` (Füllfarbe, Paddings, Verzögerung, Mauszeiger, Hotkey,
  Dashboard, Lösch-Nachfrage, Sprache) und `[State]` (letzter Tab, Fensterposition).
- Pro Tab ein Ordner mit GUID-Namen: `tab.ini` (Titel, Reihenfolge, Farbe, Paddings, Verzögerung,
  Scroll, Snippet-Liste mit Datei/Sichtbarkeit/Position) plus die PNG-Dateien. Ein neuer Tab
  erbt Farbe, Paddings und Verzögerung aus `[Defaults]` und lebt danach eigenständig.

Alle INI-Dateien sind UTF-16 und werden atomar (tmp + rename) in einem Zug geschrieben, PNGs ebenso.
Beim Start werden nur Metadaten und PNG-Größen gelesen; die Bilder selbst lädt die App nur für den
aktiven Tab in den Speicher. Ein gerade nicht lesbares PNG (gesperrt, fehlend, defekt) bleibt als
Platzhalter in der Liste und wird beim nächsten Tab-Wechsel erneut versucht; es geht nicht verloren.
Schlägt das Schreiben eines PNG fehl (Platte voll, Ordner gesperrt), bleibt der Ausschnitt im Speicher,
die Statuszeile zählt ihn als „noch nicht gespeichert“, und jedes spätere Speichern des Tabs versucht es erneut.
Die Fensterlage wird samt DPI gespeichert und beim Start an den Zielmonitor angepasst.
Die App startet auch ohne jede Datei; fehlende Werte kommen aus fest eincodierten Vorgaben.

## Optik

Die GUI ist komplett selbst gezeichnet, orientiert sich aber am Fluent Design von Windows 11:
Mica-ähnlicher Hintergrund, Karten mit feinem Strich, TabView-Tabs, CommandBar, Fluent-Toggle,
Checkbox und Akzentfarbe. Helles und dunkles Design („Automatisch“ folgt der Windows-Einstellung,
in den Einstellungen umschaltbar; die Titelleiste wird per DWM mitgefärbt). Symbole kommen als
Unicode-Glyphen aus „Segoe Fluent Icons“ (Windows 11), Fallback „Segoe MDL2 Assets“ (Windows 10);
fehlt ein Glyph in beiden, wird ein Textzeichen gezeichnet. Die Quellen werden mit `/utf-8` übersetzt.
Die Versionsnummer steht nur in `src/resource.h` (Ressource und About-Seite).

## Sprachen

Deutsch, Englisch und Spanisch. Standard ist „Automatisch“ (folgt der Windows-Anzeigesprache,
sonst Englisch); die Auswahl steht oben in den Einstellungen und wird als `Language` in der
globalen INI gespeichert. Alle Texte liegen in einer Tabelle in [src/Lang.cpp](src/Lang.cpp);
eine weitere Sprache ist eine zusätzliche Spalte plus ein Eintrag im Auswahlschalter.

## Lizenz

MultiSnip ist freie Software unter der MIT-Lizenz (siehe `LICENSE`).
Quellcode: https://github.com/BjWe/MultiSnip

## Quellstruktur

| Datei | Inhalt |
|---|---|
| `src/Model.*` | Datenmodell (Tabs, Snippets), INI-Persistenz, Ordner-Scan |
| `src/Image.*` | GDI+-Init, PNG laden/speichern, Layout + Gesamtbild, Clipboard |
| `src/Capture.*` | Einfrieren pro Monitor, Overlay-Fenster, Auswahl, Abbruch-Routine |
| `src/Ocr.*` | Texterkennung über Windows.Media.Ocr (C++/WinRT, Arbeitsthread, Ergebnis per Fensternachricht) |
| `src/MainWindow.cpp` | Hauptfenster (selbst gezeichnet, Double Buffering), Tabs, Stapel, Drag, Zoom, Dashboard, Einstellungen, Tray, Hotkey, `wWinMain` |
| `src/Util.*` | kleine GDI-Zeichenhelfer |
| `src/app.manifest` | Per-Monitor-DPI (V2) und Common-Controls 6 (Task-Dialog) |
| `src/app.ico`, `src/resource.h` | Programmsymbol (16–256 px) und zentrale Versionsnummer |
| `installer/` | WiX-Paketbeschreibung, Lizenz für den Dialog und `build-msi.ps1` |
| `.github/workflows/` | Build bei jedem Push, Release beim Tag `v<version>` |
