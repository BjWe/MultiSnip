#include "Lang.h"
#include <cstdarg>
#include <cstdio>

// Spalten: Deutsch, Englisch, Spanisch
static const wchar_t* kStr[S__COUNT][3] = {
    /* S_MOD_CTRL  */ { L"Strg+", L"Ctrl+", L"Ctrl+" },
    /* S_MOD_ALT   */ { L"Alt+", L"Alt+", L"Alt+" },
    /* S_MOD_SHIFT */ { L"Umschalt+", L"Shift+", L"Mayús+" },
    /* S_MOD_WIN   */ { L"Win+", L"Win+", L"Win+" },
    /* S_NEW_SNIP  */ { L"Neuer Snip", L"New snip", L"Nuevo recorte" },
    /* S_COPY      */ { L"Kopieren", L"Copy", L"Copiar" },
    /* S_SAVE      */ { L"Speichern", L"Save", L"Guardar" },
    /* S_RENAME    */ { L"Umbenennen", L"Rename", L"Renombrar" },
    /* S_MARGIN    */ { L"Rand", L"Margin", L"Margen" },
    /* S_GAP       */ { L"Abstand", L"Spacing", L"Espacio" },
    /* S_EMPTY_TAB_FMT */ {
        L"Noch keine Ausschnitte in diesem Tab.\n%s drücken oder „Neuer Snip“ klicken, um einen Bildschirmbereich zu erfassen.",
        L"No snippets in this tab yet.\nPress %s or click “New snip” to capture a screen area.",
        L"Aún no hay recortes en esta pestaña.\nPulsa %s o haz clic en «Nuevo recorte» para capturar un área de la pantalla." },
    /* S_HIDDEN    */ { L"Ausgeblendet", L"Hidden", L"Oculto" },
    /* S_WELCOME   */ { L"Willkommen bei MultiSnip", L"Welcome to MultiSnip", L"Bienvenido a MultiSnip" },
    /* S_TAGLINE   */ {
        L"Bildschirmausschnitte stapeln, verwalten und als ein Bild weitergeben.",
        L"Stack screen snippets, manage them and share them as one image.",
        L"Apila recortes de pantalla, gestiónalos y compártelos como una sola imagen." },
    /* S_STEP1 */ {
        L"Mit „+“ in der Tab-Leiste oder dem Knopf unten einen Tab anlegen.",
        L"Create a tab with “+” in the tab bar or the button below.",
        L"Crea una pestaña con «+» en la barra de pestañas o con el botón de abajo." },
    /* S_STEP2_FMT */ {
        L"%s drücken (oder „Neuer Snip“ / Tray-Symbol) und einen Bereich aufziehen. Esc bricht ab.",
        L"Press %s (or “New snip” / tray icon) and drag out an area. Esc cancels.",
        L"Pulsa %s (o «Nuevo recorte» / icono de la bandeja) y arrastra un área. Esc cancela." },
    /* S_STEP3 */ {
        L"Jeder Ausschnitt landet unten im Stapel. Die Checkbox blendet ihn aus, ↻ erfasst ihn neu, ✕ löscht ihn.",
        L"Each snippet lands at the bottom of the stack. The checkbox hides it, ↻ recaptures it, ✕ deletes it.",
        L"Cada recorte se añade al final de la pila. La casilla lo oculta, ↻ lo vuelve a capturar, ✕ lo elimina." },
    /* S_STEP4 */ {
        L"Ausschnitte mit der Maus an eine andere Stelle ziehen, um die Reihenfolge zu ändern.",
        L"Drag snippets with the mouse to change their order.",
        L"Arrastra los recortes con el ratón para cambiar su orden." },
    /* S_STEP5 */ {
        L"Der Stift auf der Karte (oder Doppelklick) öffnet den Editor: Ebenen, Stift, Marker, Linie, Pfeil, Rechteck, Ellipse, Text und Nummern.",
        L"The pen on the card (or double-click) opens the editor: layers, pen, highlighter, line, arrow, rectangle, ellipse, text and numbers.",
        L"El lápiz de la tarjeta (o doble clic) abre el editor: capas, lápiz, marcador, línea, flecha, rectángulo, elipse, texto y números." },
    /* S_STEP6 */ {
        L"„Kopieren“ (Strg+C) legt alle sichtbaren Ausschnitte als ein Bild in die Zwischenablage; „Speichern“ schreibt ein PNG.",
        L"“Copy” (Ctrl+C) puts all visible snippets as one image on the clipboard; “Save” writes a PNG.",
        L"«Copiar» (Ctrl+C) pone todos los recortes visibles como una imagen en el portapapeles; «Guardar» escribe un PNG." },
    /* S_STEP7 */ {
        L"Strg+Mausrad zoomt die Vorschau. Kopieren und Speichern nutzen immer die Originalpixel.",
        L"Ctrl+wheel zooms the preview. Copy and Save always use the original pixels.",
        L"Ctrl+rueda hace zoom en la vista previa. Copiar y Guardar usan siempre los píxeles originales." },
    /* S_STEP8 */ {
        L"Füllfarbe, Abstände, Verzögerung, Mauszeiger und Hotkey unter „Einstellungen“. Schließen versteckt nur im Tray.",
        L"Fill colour, spacing, delay, mouse pointer and hotkey are in “Settings”. Closing only hides to the tray.",
        L"Color de relleno, espacios, retardo, puntero y atajo en «Ajustes». Cerrar solo oculta en la bandeja." },
    /* S_NEW_TAB_BTN */ { L"Neuen Tab anlegen", L"Create new tab", L"Crear pestaña nueva" },
    /* S_NONE      */ { L"keine", L"none", L"ninguno" },
    /* S_ON        */ { L"Ein", L"On", L"Sí" },
    /* S_OFF       */ { L"Aus", L"Off", L"No" },
    /* S_SET_DEFAULTS_HDR */ { L"Standardwerte für neue Tabs und Erfassung", L"Defaults for new tabs and capture", L"Valores predeterminados para pestañas nuevas y captura" },
    /* S_SET_FILL  */ { L"Standard-Füllfarbe", L"Default fill colour", L"Color de relleno predeterminado" },
    /* S_SET_FILL_D */ { L"Hintergrund zwischen den Ausschnitten für neue Tabs", L"Background between snippets for new tabs", L"Fondo entre los recortes en pestañas nuevas" },
    /* S_SET_PADX  */ { L"Padding horizontal", L"Horizontal padding", L"Relleno horizontal" },
    /* S_SET_PADX_D */ { L"Rand links und rechts im Gesamtbild (Umschalt: 10er-Schritte)", L"Margin left and right in the composite (Shift: steps of 10)", L"Margen izquierdo y derecho en la imagen final (Mayús: pasos de 10)" },
    /* S_SET_PADY  */ { L"Padding vertikal", L"Vertical padding", L"Relleno vertical" },
    /* S_SET_PADY_D */ { L"Abstand zwischen den Ausschnitten (Umschalt: 10er-Schritte)", L"Spacing between snippets (Shift: steps of 10)", L"Espacio entre recortes (Mayús: pasos de 10)" },
    /* S_SET_DELAY */ { L"Verzögerung", L"Delay", L"Retardo" },
    /* S_SET_DELAY_D */ { L"Wartezeit, bevor der Bildschirm eingefroren wird", L"Wait time before the screen is frozen", L"Tiempo de espera antes de congelar la pantalla" },
    /* S_SET_CURSOR */ { L"Mauszeiger mit einfangen", L"Capture mouse pointer", L"Capturar el puntero del ratón" },
    /* S_SET_CURSOR_D */ { L"Zeichnet den Cursor in den eingefrorenen Bildschirm", L"Draws the cursor into the frozen screen", L"Dibuja el cursor en la pantalla congelada" },
    /* S_SET_HOTKEY */ { L"Globaler Hotkey", L"Global hotkey", L"Atajo global" },
    /* S_SET_HOTKEY_OK */ { L"Systemweit; mindestens ein Modifikator plus Taste", L"System-wide; at least one modifier plus a key", L"En todo el sistema; al menos un modificador más una tecla" },
    /* S_SET_HOTKEY_BAD */ { L"Nicht registriert: die Kombination ist belegt", L"Not registered: the combination is already in use", L"No registrado: la combinación ya está en uso" },
    /* S_PRESS_KEYS */ { L"Tastenkombination drücken…", L"Press key combination…", L"Pulsa la combinación de teclas…" },
    /* S_SET_DASH  */ { L"Dashboard-Tab anzeigen", L"Show dashboard tab", L"Mostrar pestaña de inicio" },
    /* S_SET_DASH_D */ { L"Startseite mit Hilfe ganz links in der Tab-Leiste", L"Start page with help at the far left of the tab bar", L"Página de inicio con ayuda a la izquierda de la barra de pestañas" },
    /* S_SET_ASK   */ { L"Sicherheitsabfrage beim Löschen", L"Confirm before deleting", L"Confirmar antes de eliminar" },
    /* S_SET_ASK_OFF */ { L"Deaktiviert: Ausschnitte werden ohne Nachfrage gelöscht", L"Disabled: snippets are deleted without asking", L"Desactivado: los recortes se eliminan sin preguntar" },
    /* S_SET_ASK_ON */ { L"Aktiv: vor dem Löschen eines Ausschnitts wird gefragt", L"Active: asks before a snippet is deleted", L"Activo: pregunta antes de eliminar un recorte" },
    /* S_REENABLE  */ { L"Wieder aktivieren", L"Re-enable", L"Reactivar" },
    /* S_SET_LANG  */ { L"Sprache", L"Language", L"Idioma" },
    /* S_SET_LANG_D */ { L"„Automatisch“ folgt der Windows-Anzeigesprache", L"“Automatic” follows the Windows display language", L"«Automático» sigue el idioma de Windows" },
    /* S_LANG_AUTO */ { L"Automatisch", L"Automatic", L"Automático" },
    /* S_ACTIVE_TAB */ { L"Aktiver Tab: ", L"Active tab: ", L"Pestaña activa: " },
    /* S_TAB_FILL  */ { L"Füllfarbe dieses Tabs", L"Fill colour of this tab", L"Color de relleno de esta pestaña" },
    /* S_TAB_ONLY  */ { L"Gilt nur für diesen Tab", L"Applies to this tab only", L"Solo para esta pestaña" },
    /* S_TAB_PADX  */ { L"Padding horizontal dieses Tabs", L"Horizontal padding of this tab", L"Relleno horizontal de esta pestaña" },
    /* S_TAB_PADX_D */ { L"Rand links und rechts", L"Margin left and right", L"Margen izquierdo y derecho" },
    /* S_TAB_PADY  */ { L"Padding vertikal dieses Tabs", L"Vertical padding of this tab", L"Relleno vertical de esta pestaña" },
    /* S_TAB_PADY_D */ { L"Abstand zwischen den Ausschnitten", L"Spacing between snippets", L"Espacio entre recortes" },
    /* S_TAB_DELAY */ { L"Verzögerung dieses Tabs", L"Delay of this tab", L"Retardo de esta pestaña" },
    /* S_TAB_DELAY_D */ { L"Auch über den Pfeil am Knopf „Neuer Snip“ wählbar", L"Also available via the arrow on the “New snip” button", L"También disponible en la flecha del botón «Nuevo recorte»" },
    /* S_SETTINGS  */ { L"Einstellungen", L"Settings", L"Ajustes" },
    /* S_HOTKEY_LBL */ { L"Hotkey: ", L"Hotkey: ", L"Atajo: " },
    /* S_STATUS_FMT */ { L"%d Ausschnitte, %d sichtbar   ·   Gesamtbild %d × %d px", L"%d snippets, %d visible   ·   composite %d × %d px", L"%d recortes, %d visibles   ·   imagen final %d × %d px" },
    /* S_VERSION_LINE_FMT */ { L"Version %s  ·  Stapelndes Snipping-Tool für Windows", L"Version %s  ·  Stacking snipping tool for Windows", L"Versión %s  ·  Herramienta de recortes apilados para Windows" },
    /* S_FREE_SW   */ { L"Freie Software unter der MIT-Lizenz", L"Free software under the MIT licence", L"Software libre bajo licencia MIT" },
    /* S_FREE_SW_D */ {
        L"Du darfst MultiSnip frei nutzen, kopieren, verändern und weitergeben. Der Quellcode liegt auf GitHub.",
        L"You may freely use, copy, modify and redistribute MultiSnip. The source code is on GitHub.",
        L"Puedes usar, copiar, modificar y redistribuir MultiSnip libremente. El código fuente está en GitHub." },
    /* S_ABOUT_TITLE */ { L"Über MultiSnip", L"About MultiSnip", L"Acerca de MultiSnip" },
    /* S_LAYER_N_FMT */ { L"Ebene %d", L"Layer %d", L"Capa %d" },
    /* S_ED_HINT   */ {
        L"Mit der Maus zeichnen   ·   Auswahl-Werkzeug verschiebt Formen, Entf löscht   ·   Strg+Z / Strg+Y   ·   Strg+Mausrad zoomt",
        L"Draw with the mouse   ·   Select tool moves shapes, Del deletes   ·   Ctrl+Z / Ctrl+Y   ·   Ctrl+wheel zooms",
        L"Dibuja con el ratón   ·   La herramienta de selección mueve formas, Supr elimina   ·   Ctrl+Z / Ctrl+Y   ·   Ctrl+rueda hace zoom" },
    /* S_DONE      */ { L"Fertig", L"Done", L"Listo" },
    /* S_LAYERS    */ { L"Ebenen", L"Layers", L"Capas" },
    /* S_LAYERS_HINT */ {
        L"Oberste Ebene liegt vorn. Doppelklick benennt um. Gezeichnet wird immer in der markierten Ebene.",
        L"The top layer is in front. Double-click renames. Drawing always goes to the selected layer.",
        L"La capa superior está delante. Doble clic renombra. Siempre se dibuja en la capa seleccionada." },
    /* S_REPLACED_FMT */ { L"Ausschnitt #%d ersetzt", L"Snippet #%d replaced", L"Recorte #%d reemplazado" },
    /* S_APPENDED_FMT */ { L"Ausschnitt angehängt (%d × %d px)", L"Snippet appended (%d × %d px)", L"Recorte añadido (%d × %d px)" },
    /* S_DONT_ASK  */ { L"Nicht mehr nachfragen", L"Don't ask again", L"No volver a preguntar" },
    /* S_DEL_TAB_Q_FMT */ { L"Tab „%s“ löschen?", L"Delete tab “%s”?", L"¿Eliminar la pestaña «%s»?" },
    /* S_DEL_TAB_D_FMT */ { L"Alle %d Ausschnitte dieses Tabs werden endgültig gelöscht.", L"All %d snippets of this tab will be deleted permanently.", L"Se eliminarán definitivamente los %d recortes de esta pestaña." },
    /* S_DEL_SNIP_Q_FMT */ { L"Ausschnitt #%d löschen?", L"Delete snippet #%d?", L"¿Eliminar el recorte #%d?" },
    /* S_DEL_SNIP_D */ { L"Der Ausschnitt wird aus dem Stapel entfernt und die PNG-Datei gelöscht.", L"The snippet is removed from the stack and its PNG file deleted.", L"El recorte se quita de la pila y se elimina su archivo PNG." },
    /* S_NO_VISIBLE_COPY */ { L"Keine sichtbaren Ausschnitte zum Kopieren", L"No visible snippets to copy", L"No hay recortes visibles para copiar" },
    /* S_COPIED_FMT */ { L"Gesamtbild (%d × %d px) in die Zwischenablage kopiert", L"Composite (%d × %d px) copied to clipboard", L"Imagen final (%d × %d px) copiada al portapapeles" },
    /* S_CLIP_FAIL */ { L"Zwischenablage konnte nicht beschrieben werden", L"Could not write to the clipboard", L"No se pudo escribir en el portapapeles" },
    /* S_NO_VISIBLE_SAVE */ { L"Keine sichtbaren Ausschnitte zum Speichern", L"No visible snippets to save", L"No hay recortes visibles para guardar" },
    /* S_PNG_FILTER */ { L"PNG-Bild (*.png)", L"PNG image (*.png)", L"Imagen PNG (*.png)" },
    /* S_SAVE_TITLE */ { L"Gesamtbild als PNG speichern", L"Save composite as PNG", L"Guardar imagen final como PNG" },
    /* S_SAVED     */ { L"Gespeichert: ", L"Saved: ", L"Guardado: " },
    /* S_HOTKEY_TAKEN_FMT */ {
        L"Der Hotkey %s ist bereits von einem anderen Programm belegt.\n\nBitte in den Einstellungen eine andere Tastenkombination wählen.",
        L"The hotkey %s is already used by another program.\n\nPlease choose a different key combination in Settings.",
        L"El atajo %s ya lo usa otro programa.\n\nElige otra combinación de teclas en Ajustes." },
    /* S_HOTKEY_TITLE */ { L"MultiSnip – Hotkey", L"MultiSnip – Hotkey", L"MultiSnip – Atajo" },
    /* S_DELAY_SET_FMT */ { L"Verzögerung: %s", L"Delay: %s", L"Retardo: %s" },
    /* S_DELAY_TAB_FMT */ { L" (Tab „%s“)", L" (tab “%s”)", L" (pestaña «%s»)" },
    /* S_DELAY_DEFAULT */ { L" (Standard)", L" (default)", L" (predeterminado)" },
    /* S_NO_DELAY  */ { L"keine Verzögerung", L"no delay", L"sin retardo" },
    /* S_TRAY_SHOW */ { L"Fenster zeigen", L"Show window", L"Mostrar ventana" },
    /* S_TRAY_HIDE */ { L"Fenster verstecken", L"Hide window", L"Ocultar ventana" },
    /* S_TRAY_DELAY_FMT */ { L"Verzögerung (%s)", L"Delay (%s)", L"Retardo (%s)" },
    /* S_DEFAULT   */ { L"Standard", L"Default", L"Predeterminado" },
    /* S_EXIT      */ { L"Beenden", L"Exit", L"Salir" },
    /* S_NEED_MOD  */ { L"Bitte mindestens einen Modifikator (Strg, Alt, Umschalt, Win) verwenden", L"Please use at least one modifier (Ctrl, Alt, Shift, Win)", L"Usa al menos un modificador (Ctrl, Alt, Mayús, Win)" },
    /* S_COMBO_TAKEN_FMT */ { L"Die Kombination %s ist bereits belegt.\nBitte eine andere wählen.", L"The combination %s is already in use.\nPlease choose another.", L"La combinación %s ya está en uso.\nElige otra." },
    /* S_ERR_GDIPLUS */ { L"GDI+ konnte nicht initialisiert werden.", L"GDI+ could not be initialised.", L"No se pudo inicializar GDI+." },
    /* S_ERR_APPDATA */ { L"Der Datenordner unter %AppData% konnte nicht angelegt werden.", L"The data folder under %AppData% could not be created.", L"No se pudo crear la carpeta de datos en %AppData%." },
    /* S_SAVE_ERR_FMT */ {
        L"Speichern fehlgeschlagen:\n%s\n\nFehlercode: %u\n\nDie Änderung konnte nicht gesichert werden. Bitte prüfen, ob der Ordner\n%s\nbeschreibbar ist.",
        L"Saving failed:\n%s\n\nError code: %u\n\nThe change could not be saved. Please check that the folder\n%s\nis writable.",
        L"Error al guardar:\n%s\n\nCódigo de error: %u\n\nNo se pudo guardar el cambio. Comprueba que la carpeta\n%s\nse pueda escribir." },
    /* S_SAVE_ERR_TITLE */ { L"MultiSnip – Speicherfehler", L"MultiSnip – Save error", L"MultiSnip – Error al guardar" },
    /* S_TAB_N_FMT */ { L"Tab %d", L"Tab %d", L"Pestaña %d" },
    /* S_CAPTURE_HINT */ { L"Bereich mit der Maus aufziehen   ·   Esc bricht ab", L"Drag to select an area   ·   Esc cancels", L"Arrastra para seleccionar un área   ·   Esc cancela" },
    /* S_COUNTDOWN_FMT */ {
        L"Aufnahme in %d s   ·   Hotkey erneut oder Klick hier bricht ab",
        L"Capturing in %d s   ·   hotkey again or click here cancels",
        L"Captura en %d s   ·   el atajo de nuevo o un clic aquí cancela" },
    /* S_CAPTURE_CANCELLED */ { L"Aufnahme abgebrochen", L"Capture cancelled", L"Captura cancelada" },
    /* S_FILE_MISSING_FMT */ { L"Datei nicht lesbar: %s", L"File not readable: %s", L"Archivo ilegible: %s" },
    /* S_MODAL_BUSY */ { L"Bitte zuerst den offenen Dialog schließen", L"Please close the open dialog first", L"Cierra primero el diálogo abierto" },
    /* S_ALL_TABS  */ { L"Alle Tabs", L"All tabs", L"Todas las pestañas" },
    /* S_TOOL_SELECT */ { L"Auswählen / Verschieben", L"Select / move", L"Seleccionar / mover" },
    /* S_TOOL_PEN    */ { L"Stift", L"Pen", L"Lápiz" },
    /* S_TOOL_MARKER */ { L"Marker", L"Highlighter", L"Marcador" },
    /* S_TOOL_LINE   */ { L"Linie", L"Line", L"Línea" },
    /* S_TOOL_ARROW  */ { L"Pfeil", L"Arrow", L"Flecha" },
    /* S_TOOL_RECT   */ { L"Rechteck", L"Rectangle", L"Rectángulo" },
    /* S_TOOL_ELLIPSE */ { L"Ellipse", L"Ellipse", L"Elipse" },
    /* S_TOOL_TEXT   */ { L"Text", L"Text", L"Texto" },
    /* S_TOOL_NUMBER */ { L"Nummer", L"Number", L"Número" },
    /* S_SET_THEME   */ { L"Design", L"Theme", L"Tema" },
    /* S_SET_THEME_D */ { L"„Automatisch“ folgt der Windows-Einstellung für App-Design", L"“Automatic” follows the Windows app theme setting", L"«Automático» sigue el tema de aplicaciones de Windows" },
    /* S_THEME_LIGHT */ { L"Hell", L"Light", L"Claro" },
    /* S_THEME_DARK  */ { L"Dunkel", L"Dark", L"Oscuro" },
    /* S_COMPOSITE_TOO_BIG */ { L"Gesamtbild zu groß für den Speicher", L"Composite too large for memory", L"Imagen final demasiado grande para la memoria" },
    /* S_UNSAVED_FMT */ { L"   ·   %d noch nicht gespeichert", L"   ·   %d not yet saved", L"   ·   %d sin guardar" },
    /* S_OCR_TITLE_FMT */ { L"Erkannter Text  –  %s", L"Recognised text  –  %s", L"Texto reconocido  –  %s" },
    /* S_OCR_RUNNING */ { L"Text wird erkannt…", L"Recognising text…", L"Reconociendo texto…" },
    /* S_OCR_NONE    */ { L"Kein Text im Ausschnitt erkannt", L"No text recognised in the snippet", L"No se reconoció texto en el recorte" },
    /* S_OCR_NO_LANG */ {
        L"Windows hat keine Sprache mit Texterkennung installiert.\n\nEinstellungen → Zeit und Sprache → Sprache → bei einer Sprache „Optionen“ → „Texterkennung“ hinzufügen.",
        L"Windows has no language with text recognition installed.\n\nSettings → Time & language → Language → “Options” on a language → add “Optical character recognition”.",
        L"Windows no tiene ningún idioma con reconocimiento de texto instalado.\n\nConfiguración → Hora e idioma → Idioma → «Opciones» en un idioma → añadir «Reconocimiento óptico de caracteres»." },
    /* S_OCR_NO_LANG_TITLE */ { L"MultiSnip – Texterkennung", L"MultiSnip – Text recognition", L"MultiSnip – Reconocimiento de texto" },
    /* S_OCR_FAILED  */ { L"Texterkennung fehlgeschlagen", L"Text recognition failed", L"Error en el reconocimiento de texto" },
    /* S_OCR_COPY_ALL */ { L"Alles kopieren", L"Copy all", L"Copiar todo" },
    /* S_OCR_COPIED_BTN */ { L"Kopiert ✓", L"Copied ✓", L"Copiado ✓" },
    /* S_CLOSE       */ { L"Schließen", L"Close", L"Cerrar" },
    /* S_OCR_HINT    */ { L"Der Text kann hier vor dem Kopieren geändert werden; gespeichert wird nichts.", L"You can edit the text here before copying; nothing is saved.", L"Puedes editar el texto aquí antes de copiarlo; no se guarda nada." },
    /* S_SNIP_COPIED_FMT */ { L"Ausschnitt #%d (%d × %d px) in die Zwischenablage kopiert", L"Snippet #%d (%d × %d px) copied to clipboard", L"Recorte #%d (%d × %d px) copiado al portapapeles" },
    /* S_TOOL_CROP   */ { L"Zuschneiden", L"Crop", L"Recortar" },
    /* S_CROP_RESET  */ { L"Zuschnitt aufheben", L"Remove crop", L"Quitar recorte" },
    /* S_CROP_INFO_FMT */ { L"   ·   Zuschnitt %d × %d px", L"   ·   crop %d × %d px", L"   ·   recorte %d × %d px" },
    /* S_CROP_SET_FMT */ { L"Zuschnitt gesetzt: %d × %d px (Original bleibt erhalten)", L"Crop set: %d × %d px (original is kept)", L"Recorte definido: %d × %d px (el original se conserva)" },
    /* S_CROP_CLEARED */ { L"Zuschnitt aufgehoben", L"Crop removed", L"Recorte quitado" },
    /* S_SET_AUTOCOPY */ { L"Nach dem Snip in die Zwischenablage", L"Copy to clipboard after a snip", L"Copiar al portapapeles tras el recorte" },
    /* S_SET_AUTOCOPY_D */ { L"Was nach jeder Aufnahme automatisch kopiert wird (Vorgabe für neue Tabs)", L"What is copied automatically after each capture (default for new tabs)", L"Qué se copia automáticamente tras cada captura (valor para pestañas nuevas)" },
    /* S_TAB_AUTOCOPY */ { L"Nach dem Snip kopieren (dieser Tab)", L"Copy after snip (this tab)", L"Copiar tras el recorte (esta pestaña)" },
    /* S_AC_OFF      */ { L"Nichts", L"Nothing", L"Nada" },
    /* S_AC_SNIP     */ { L"Nur den Ausschnitt", L"Only the snippet", L"Solo el recorte" },
    /* S_AC_TAB      */ { L"Gesamtbild des Tabs", L"Whole tab composite", L"Imagen completa de la pestaña" },
    /* S_AC_MENU     */ { L"Nach dem Snip kopieren", L"Copy after snip", L"Copiar tras el recorte" },
    /* S_AC_SET_FMT  */ { L"Nach dem Snip kopieren: %s", L"Copy after snip: %s", L"Copiar tras el recorte: %s" },
    /* S_AC_DONE_SNIP */ { L"   ·   in die Zwischenablage kopiert", L"   ·   copied to clipboard", L"   ·   copiado al portapapeles" },
    /* S_AC_DONE_TAB */ { L"   ·   Gesamtbild (%d × %d px) in die Zwischenablage kopiert", L"   ·   composite (%d × %d px) copied to clipboard", L"   ·   imagen completa (%d × %d px) copiada al portapapeles" },
    /* S_SET_NUMSCOPE */ { L"Nummern zählen", L"Number counting", L"Conteo de números" },
    /* S_SET_NUMSCOPE_D */ {
        L"Vorgabe für neue Tabs: zählt das Nummern-Werkzeug je Ausschnitt neu oder durch den ganzen Tab.",
        L"Default for new tabs: whether the number tool restarts per snippet or counts through the whole tab.",
        L"Predeterminado para pestañas nuevas: si la herramienta de números reinicia en cada recorte o cuenta en toda la pestaña." },
    /* S_TAB_NUMSCOPE */ { L"Nummern zählen", L"Number counting", L"Conteo de números" },
    /* S_NS_SNIP */ { L"Pro Ausschnitt", L"Per snippet", L"Por recorte" },
    /* S_NS_TAB */ { L"Pro Tab", L"Per tab", L"Por pestaña" },
    /* S_ED_NUM_NEXT */ { L"Nächste Nummer", L"Next number", L"Siguiente número" },
    /* S_ED_NUM_SEL */ { L"Nummer", L"Number", L"Número" },
    /* S_ED_NUM_HINT */ {
        L"Nummer ändern – auch mit + und − auf der Tastatur",
        L"Change the number – + and − on the keyboard work too",
        L"Cambiar el número: también con + y − en el teclado" },
};

static Lang g_sel = Lang::Auto;
static int g_col = 1;

static int ColumnFor(Lang l)
{
    switch (l) {
    case Lang::De: return 0;
    case Lang::Es: return 2;
    case Lang::En: return 1;
    default: break;
    }
    WORD prim = PRIMARYLANGID(GetUserDefaultUILanguage());
    if (prim == LANG_GERMAN) return 0;
    if (prim == LANG_SPANISH) return 2;
    return 1;
}

void Lang_Set(Lang l) { g_sel = l; g_col = ColumnFor(l); }
Lang Lang_Get() { return g_sel; }
Lang Lang_Effective() { return g_col == 0 ? Lang::De : (g_col == 2 ? Lang::Es : Lang::En); }

const wchar_t* TR(StrId id)
{
    if (id < 0 || id >= S__COUNT) return L"";
    return kStr[id][g_col];
}

std::wstring TRF(StrId id, ...)
{
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, id);
    _vsnwprintf_s(buf, _TRUNCATE, TR(id), ap);
    va_end(ap);
    return buf;
}
