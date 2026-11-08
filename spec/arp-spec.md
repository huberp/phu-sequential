# ARP Plugin Spezifikation v2

## 1. Überblick

Das Plugin ist ein Arpeggiator-Sequencer als VST3-MIDI-Effect. Das Plugin empfängt MIDI-Noten als Eingabe, sortiert sie und speist sie in genau 8 unabhängige Sequencer-Lanes. Jede Lane erzeugt aus ihrer Eingabenote eine eigene Sequenz von Ausgabenoten. Das Plugin besitzt keinen Audiopfad. Das Plugin verarbeitet ausschließlich MIDI-Events.

## 2. Plugin-Format und Kategorie

Das Zielformat ist VST3. Das Plugin wird als MIDI-Effect implementiert (Event-Input, Event-Output, kein Audio-Pfad). Das Plugin nutzt die VST3-Kategorie für MIDI-Effekte. Das Plugin unterstützt State-Saving gemäß Abschnitt 13.

## 3. Eingangsverarbeitung

### 3.1 Chord-Definition

Das aktuell gespielte Chord besteht ausschließlich aus gehaltenen Noten. Eine Note gelangt beim Note-On in das Chord. Eine Note verlässt das Chord beim Note-Off. Ein Note-On mit Velocity 0 gilt als Note-Off (MIDI-Konvention). Das Chord ist damit jederzeit die Menge aller aktuell gehaltenen Noten. Das Chord enthält jeden Pitch höchstens einmal. Ein Note-On eines bereits gehaltenen Pitches ersetzt die bestehende Note im Chord. Das Sustain-Pedal (CC64) gilt pro Channel. Steht das Pedal eines Channels an (Wert mindestens 64), bleiben die Noten dieses Channels auch nach ihrem Note-Off als gehalten gelten, bis das Pedal ausgeschaltet wird (Wert unter 64). Beim Ausschalten des Pedals verlassen alle nicht mehr tastengehaltenen Noten das Chord. Die Pedal-Logik gilt für den Chord-Input-Channel und für die Channels der Channel-Mode-Lanes gleichermaßen.

### 3.2 Input-Channel-Filter

Der Chord-Follow-Mode (Abschnitt 4.2) hört auf genau einen global einstellbaren Chord-Input-Channel (1 bis 16). Nur Noten auf diesem Channel bilden das Chord. Der Chord-Input-Channel ist ein automatisierbarer globaler Parameter (Abschnitt 12). Der Channel-Mode (Abschnitt 4.3) hört unabhängig davon auf die Channels 1 bis 16 je Lane-Einstellung.

### 3.3 Durchreichung

Das Plugin kann die Eingangsnoten an den Output durchreichen. Die Durchreichung wird durch den globalen Parameter Thru ein- und ausgeschaltet und gilt für alle 16 MIDI-Channels. Bei ausgeschalteter Durchreichung gibt das Plugin ausschließlich die generierten Sequencer-Noten aus.

Zusätzlich besitzt das Plugin den globalen Parameter Consume. Ist Consume eingeschaltet, werden Eingangsnoten auf allen Channels, denen mindestens eine Lane folgt (Chord-Input-Channel sowie alle Follow-Channels der Channel-Mode-Lanes), nicht durchgereicht – unabhängig vom Thru-Parameter. Consume verhindert Doppel-Trigger, wenn der Output-Channel einer Lane dem gefolgten Channel entspricht.

Nicht-Noten-Events (CC, Pitch Bend, Aftertouch, Program Change) werden unabhängig von Thru und Consume immer unverändert durchgereicht. CC64 wird zusätzlich für die Halte-Logik interpretiert (Abschnitt 3.1). CC120 (All Sound Off) und CC123 (All Notes Off) werden lediglich durchgereicht und lösen keine interne Sonderbehandlung aus; die Stimmverwaltung folgt ausschließlich Abschnitt 9. Thru und Consume sind automatisierbar.

### 3.4 Sortierung

Noten werden für die Index-Verwendung aufsteigend nach Pitch sortiert. Index 0 bezeichnet die tiefste Note. Index 1 bezeichnet die nächsthöhere Note.

## 4. Sequencer-Lanes

Das Plugin besitzt genau 8 Sequencer-Lanes. Jede Lane ist unabhängig konfigurierbar. Jede Lane besitzt einen Mute-Schalter. Jede Lane verwendet genau einen Input-Modus.

### 4.1 Nicht-Exklusivität

Input-Quellen sind nicht exklusiv. Mehrere Lanes dürfen denselben Index oder denselben Channel folgen. Zwei Lanes dürfen gleichzeitig der tiefsten Chord-Note folgen.

### 4.2 Chord-Follow-Mode

Der Benutzer setzt einen Follow-Index im Bereich 0 bis 15. Die Lane folgt der Note des Chords an diesem Index. Ist der Index größer oder gleich der aktuellen Chord-Größe, schweigt die Lane. Die Lane spielt wieder, sobald das Chord genug Noten enthält. Die Lane schweigt ebenfalls, solange das Chord leer ist.

Wird eine Chord-Note losgelassen, während der Chord noch weitere Noten enthält, verschiebt sich die Sortierung (Abschnitt 3.4): Der Follow-Index zeigt danach ggf. auf eine andere, noch gehaltene Note. Dies ist ein Pitch-Wechsel, kein Release – die Lane wechselt quantisiert zum nächsten Step-Start auf die neue Note (Abschnitt 4.4). Ein sofortiger Stopp erfolgt nur, wenn die Quelle vollständig leer wird (Chord leer oder Index außerhalb der Chord-Größe).

### 4.3 Channel-Mode

Der Benutzer wählt einen MIDI-Channel von 1 bis 16. Die Lane folgt der zuletzt gespielten Note auf diesem Channel. Die Note wirkt nur, solange sie gehalten wird. Es gibt keinen Note-Stack: Wird die gefolgte Note losgelassen, stoppt die Lane sofort (Abschnitt 9.2), auch wenn ältere Noten auf diesem Channel noch gehalten sind. Vor der ersten Note auf dem Channel schweigt die Lane.

### 4.4 Übernahme neuer Eingaben

Ein Pitch-Wechsel der Eingabe (neue Note oder Re-Sortierung gemäß Abschnitt 4.2) wird erst beim nächsten Step-Start der jeweiligen Lane übernommen. Ein laufender Step spielt seine Note bis zu ihrem geplanten Ende aus. Ein Release der Quelle (die Quelle liefert keine Note mehr) hat Vorrang vor dieser Quantisierung und stoppt die Lane sofort gemäß Abschnitt 9.2.

### 4.5 Position bei Stille

Jede Lane besitzt einen umschaltbaren Reset-Mode. Stille bedeutet in diesem Abschnitt: Die Quelle der Lane liefert keine Note (Chord leer, Index außerhalb der Chord-Größe oder keine Note auf dem Channel gehalten). Reset-Mode free-running: Die Step-Position läuft während Stille weiter; bei Wiederaufnahme setzt die Lane an der fortgelaufenen Position fort. Reset-Mode reset-on-input: Nach vollständiger Stille der Quelle startet die nächste Eingabenote die Lane beim nächsten Step-Start von vorn an Step 1. Chord-Änderungen während gehaltener Noten (Legato-Anschlüsse und Re-Sortierungen) lösen keinen Reset aus. Mute ist keine Stille in diesem Sinne: Die Step-Position läuft während Mute weiter. Der Reset-Mode ist pro Lane einstellbar und automatisierbar.

### 4.6 Latch-Modus

Jede Lane besitzt einen umschaltbaren Latch-Parameter. Latch aus: Die Lane stoppt gemäß Abschnitt 9.2 bei Release der Eingabenote. Latch an: Die Lane läuft bei Release der Eingabenote auf der zuletzt gefolgten Note weiter. Bei Latch an aktualisiert jede neu gespielte Note der Quelle die gefolgte Note zum nächsten Step-Start. Latch an endet durch Transport-Stopp, Mute, Bypass (Abschnitt 21) oder Ausschalten von Latch. Beim Beenden von Latch erhalten alle klingenden Noten der Lane sofort Note-Offs. Der Latch-Parameter ist automatisierbar.

## 5. Timing

### 5.1 Basis-Timing

Eine Lane verwendet ein Basis-Timing aus einer Liste. Die Liste enthält 1/32, 1/16, 1/8, 1/4, 1/2 und 1/1. Zusätzlich wählt der Benutzer eine Unterteilung: straight, triplet oder dotted. Ein Swing-Anteil von 0 bis 100 Prozent ist einstellbar. Swing verschiebt ausschließlich die Start-Position von Steps mit ungeradem Index (der 2., 4., 6., … Step, 1-basiert gezählt) nach hinten; Step 1 wird nie verschoben. Die Zählung beginnt pro Loop neu bei Step 1. Die Verschiebung beträgt maximal die Hälfte der Step-Länge: 100 Prozent Swing entsprechen einer Verschiebung um 50 Prozent der Step-Länge. Die Notenlängen bleiben unverändert; die Raster-Startpositionen aller anderen Steps bleiben unverändert. Dadurch kann eine verschobene Note in den folgenden Step hineinragen; diese Überlappung ist erlaubt (Stimmverwaltung gemäß Abschnitt 9). Swing wirkt nur im Basis-Timing und nicht im Text-Grid. Das Basis-Timing und die Unterteilung bestimmen Raster und Länge jedes Steps.

### 5.2 Benutzerdefiniertes Grid (Text-Grid)

Der Benutzer kann alternativ ein Grid als Text eingeben. Das Format ist eine komma-separierte Liste von n/d-Brüchen. Ein optionaler Suffix t kennzeichnet Triplet, ein optionaler Suffix . kennzeichnet Dotted; beide Suffixe schließen sich gegenseitig aus. Beispiel: "1/8t, 1/8., 1/4" definiert 3 Steps. Erlaubt sind ausschließlich musikalische Werte: binäre Unterteilungen (Nenner ist Zweierpotenz, einschließlich 1/1), Triolen-Varianten (Suffix t) und Dotted-Varianten (Suffix .). Andere Brüche sind ungültig. Ein Text-Grid umfasst höchstens 64 Einträge. Jeder Eintrag definiert Länge und Raster eines Steps. Die Loop-Länge der Lane ist die Summe aller Step-Längen. Beim Übernehmen eines neuen Text-Grids werden die Step-Daten (On/Off und Velocity-Werte) positionsweise auf die neuen Steps übertragen; überschüssige Daten entfallen, fehlende Steps erhalten Defaults (eingeschaltet, Default-Velocity des aktiven Modus).

### 5.3 Text-Grid-Validierung

Ungültige Einträge werden in der UI rot markiert. Ein Grid mit ungültigen Einträgen wird nicht übernommen. Die Lane behält ihr zuletzt gültiges Grid.

### 5.4 Step-Anzahl

Eine Lane mit Basis-Timing besitzt eine konfigurierbare Step-Anzahl. Die Step-Anzahl liegt zwischen 1 und 64. Die Step-Anzahl ist pro Lane frei wählbar. Beispiel: eine Lane mit 16 Steps, eine andere Lane mit 7 Steps. Die Loop-Länge ist die Step-Anzahl mal der Step-Länge. Bei einer Vergrößerung der Step-Anzahl erhalten die neuen Steps den Zustand eingeschaltet und Default-Velocity-Werte. Bei einer Verkleinerung werden die Daten der entfallenden Steps verworfen; eine spätere Vergrößerung restauriert keine alten Daten. Greift die Änderung, während die aktuelle Position jenseits der neuen Step-Anzahl liegt, wrappt die Position beim nächsten Step-Start modulo der neuen Step-Anzahl.

## 6. Steps

Jeder Step besitzt einen On/Off-Schalter. Ein Off-Step erzeugt keine Note und schweigt für seine Dauer. Ein On-Step spielt eine Note. Jede Lane besitzt einen Gate-Parameter in Prozent. Die Notenlänge ist die Step-Länge mal dem Gate-Wert, geteilt durch 100. Der Gate-Wert liegt zwischen 1 und 200. Der Default ist 100 Prozent. Bei Gate über 100 Prozent überlappen sich Noten; Überlappung ist erlaubt. Eine Lane ist polyphon. Der Gate-Parameter ist pro Lane einstellbar und automatisierbar.

## 7. Velocity

Jede Lane besitzt genau einen Velocity-Modus. Der Standardmodus ist "incoming velocity". Jeder Step speichert pro Velocity-Modus einen eigenen Wert; ein Wechsel des Modus restauriert die zuletzt in diesem Modus gesetzten Step-Werte. Kein Moduswechsel überschreibt die Werte eines anderen Modus.

### 7.1 Modi

Incoming velocity: Die Note erhält die Velocity der Eingabenote zum Step-Trigger-Zeitpunkt; die Step-Werte dieses Modus bleiben ungenutzt. By step: Jeder Step besitzt einen absoluten Velocity-Wert von 0 bis 127. Delta by step: Jeder Step besitzt einen Delta-Wert von -127 bis +127. Der Delta-Wert wird zur Velocity der Eingabenote addiert. Relative: Jeder Step besitzt einen Prozentwert von 0 bis 200. Die Note erhält die Eingabe-Velocity mal dem Prozentwert, geteilt durch 100.

### 7.2 Klemmung

Alle berechneten Velocity-Werte werden auf 0 bis 127 geklemmt. Eine Note mit berechneter Velocity 0 wird nicht gesendet.

### 7.3 Abtastung

Die Eingabe-Velocity wird beim Step-Trigger abgetastet; bei Latch (Abschnitt 4.6) gilt die Velocity der zuletzt gefolgten Note. Velocity-Änderungen während eines klingenden Steps beeinflussen die bereits ausgelöste Note nicht.

## 8. Output-Routing

Jede Lane besitzt einen Output-Channel-Parameter von 1 bis 16. Der Default ist der Input-Channel der gefolgten Quelle zum Zeitpunkt der Initialisierung: im Chord-Follow-Mode der Chord-Input-Channel, im Channel-Mode der gefolgte Channel. Eine spätere Änderung der Quelle ändert den Output-Channel nicht automatisch. Jede Lane besitzt einen Transpose-Parameter von -24 bis +24 Halbtönen. Der Output-Pitch ist der Pitch der Eingabenote plus dem Transpose-Wert. Liegt der transponierte Pitch außerhalb des MIDI-Bereichs 0 bis 127, wird die Note nicht gesendet (kein Clamping). Jede Ausgabenote erhält intern eine eindeutige Stimmen-Kennung (Note-Tracking, Abschnitt 9.1).

## 9. Note-Off und Stimmverwaltung

### 9.1 Stimmen-Tracking

Das Plugin verwaltet jede Ausgabenote intern als eigene Stimme. Ein Note-Off betrifft ausschließlich seine zugehörige Stimme. Da MIDI identische Pitches auf einem Channel nicht unterscheiden kann, gilt folgende Retrigger-Policy: Wird eine neue Note ausgelöst, während auf demselben Output-Channel bereits eine Stimme mit demselben Pitch klingt, sendet das Plugin zuerst ein Note-Off für die klingende Stimme und unmittelbar danach das Note-On der neuen Note. Dies verhindert hängende und vorzeitig beendete Noten beim Empfänger. Ausgehende Note-Offs verwenden Note-Off-Velocity 0.

### 9.2 Verhalten bei Release der Eingabenote

Wird die Eingabenote einer Lane losgelassen (die Quelle liefert keine Note mehr, Abschnitt 4.4), stoppt die Lane sofort, sofern Latch aus ist (Abschnitt 4.6). Das Plugin sendet sofort Note-Offs für alle aktuell klingenden Noten dieser Lane. Dies gilt für beide Input-Modi. Im Channel-Mode beendet der Note-Off der gefolgten Note die Lane sofort (Abschnitt 4.3, kein Note-Stack). Im Chord-Follow-Mode gilt ein Re-Sortieren des Chords nicht als Release (Abschnitt 4.2). Bei Latch an läuft die Lane auf der zuletzt gefolgten Note weiter.

### 9.3 Garantierte Note-Offs

Jede gesendete Note erhält unter allen Bedingungen ein Note-Off. Dies gilt insbesondere bei Grid-Wechsel, Step-Anzahl-Änderung, Transport-Stopp, Mute, Latch-Ende und Bypass.

### 9.4 Stimmen-Obergrenze

Pro Lane sind höchstens 32 gleichzeitig klingende Stimmen erlaubt. Bei Überschreitung erhält die älteste klingende Stimme sofort ein Note-Off, bevor die neue Note ausgelöst wird.

## 10. Transport und Synchronisation

Der Sequencer läuft host-synchron. Der Sequencer folgt Start und Stop des Host-Transports. Bei Transport-Start setzen sich alle Lanes auf Step 1 zurück. Bei Transport-Stopp stoppen alle Lanes sofort, und alle klingenden Noten erhalten Note-Offs. Ohne laufenden Host-Transport sind alle Lanes stumm; das Plugin besitzt keine interne Clock als Fallback.

Die Step-Position einer Lane wird in Beats (musikalische Zeit, PPQ) aus dem Host-Playhead abgeleitet, nicht in Samples. Da Positionen und Notenlängen in Beats definiert sind, wirken Tempo-Wechsel automatisch und sofort auf Step-Positionen und auf die Note-Off-Zeitpunkte bereits klingender Noten. Host-Loops verschieben den Playhead; die Lane-Position folgt dem Playhead und setzt sich daher bei einem Loop-Sprung entsprechend zurück. Nur Transport-Start setzt explizit auf Step 1 zurück. Events werden mit sample-genauen Offsets innerhalb des Buffers gesendet.

## 11. Live-Änderungen

Alle Änderungen während laufendem Sequencer sind glitch-frei. Änderungen von Step-Anzahl oder Grid greifen am nächsten Step-Start der jeweiligen Lane (Position-Wrapping gemäß Abschnitt 5.4). Änderungen von Step On/Off und Velocity-Werten greifen sofort. Änderungen am Input-Modus greifen am nächsten Step-Start. Änderungen von Transpose, Gate, Swing und Output-Channel wirken auf neu ausgelöste Noten; bereits klingende Noten behalten ihren Pitch, ihre Velocity und ihren geplanten Note-Off-Zeitpunkt, damit die Note-Off-Garantie (Abschnitt 9.3) eingehalten wird.

## 12. Parameter und Automatisierung

Automatisierbar sind ausschließlich endliche, benannte Parameter.

Globale Parameter: Thru, Consume, Chord-Input-Channel.

Lane-Parameter (pro Lane): Mute, Latch, Input-Modus, Follow-Index (0 bis 15), Follow-Channel, Output-Channel, Transpose, Reset-Mode, Gate, Basis-Timing, Unterteilung, Swing-Anteil, Step-Anzahl, Velocity-Modus.

Step-Daten (On/Off, Velocity-Werte pro Step und Modus) und das Text-Grid sind nicht automatisierbarer Plugin-State (Abschnitt 13). Das Text-Grid ist kein Host-Parameter.

## 13. State und Presets

Das Plugin speichert den vollständigen Zustand im VST3-State: alle globalen Parameter, alle Lane-Parameter und alle Step-Daten aller Lanes. Das Plugin lädt den Zustand beim Projekt-Opening. Das Plugin liefert fest eingebaute Factory-Presets mit. Das Plugin lädt und speichert zusätzliche Preset-Dateien aus einem User-Ordner; das Dateiformat ist JUCE-ValueTree-XML. Presets umfassen denselben Umfang wie der Plugin-State, also auch die globalen Parameter (Thru, Consume, Chord-Input-Channel).

## 14. UI

Jede Lane besitzt eine Step-Anzeige. Die Step-Anzeige zeigt die Breiten-Proportionen des gewählten Timings. Jeder Step ist klickbar On/Off schaltbar. Jeder Step besitzt eine vertikale Drag-Bar zur Editierung seines Velocity-Werts; die Drag-Bar editiert und zeigt den Wert des aktuell aktiven Velocity-Modus (im Modus incoming velocity ist die Drag-Bar deaktiviert). Bei vielen Steps (z.B. 16) bleibt die Lane-Anzeige horizontal scrollbar oder umschaltbar. Jede Lane zeigt einen Laufpositions-Indikator. Das Text-Grid wird pro Lane editierbar dargestellt und validiert (Abschnitt 5.3).

## 15. Implementierung und Werkzeuge

Die Sprache ist C++20. Das Framework ist JUCE. Das Ziel ist VST3 auf Windows, macOS und Linux. Direkte Steinberg-SDK-Aufrufe sind nur zulässig, wo JUCE keine Deckung bietet. Der eigentliche Editor wird als eigener JUCE-Editor nativ implementiert.

## 16. Architektur

### 16.1 Schichten

Der Code trennt wiederverwendbaren Code strikt von plugin-spezifischem Code. Wiederverwendbare Module sind für den Einsatz in anderen Plugins gedacht und sind daher reines C++20 ohne JUCE-Includes (Voraussetzung für die Host- und JUCE-freie Testbarkeit gemäß Abschnitt 18). Wiederverwendbar sind mindestens: Timing-Mathematik, Grid-Parser, Velocity-Berechnung, Index-Ermittlung, Chord-Verwaltung, Stimmenverwaltung. Plugin-spezifischer Code verdrahtet die wiederverwendbaren Module ausschließlich per Komposition. Die JUCE-Integration (AudioProcessor, Parameter-Binding, MIDI-I/O, Editor) bildet eine eigene Adapter-Schicht; nur diese Schicht enthält JUCE-Abhängigkeiten.

### 16.2 Timing-Mathematik

Die Timing-Werte 1/32 bis 1/1 sind deklarativ an genau einer Stelle als Daten definiert. Straight, triplet und dotted sind Modifier auf ein straightes Basis-Timing. Die Modifier-Faktoren (triplet = 2/3, dotted = 3/2) sind als benannte Konstanten an genau einer Stelle definiert. Die Umrechnung einer Notenlänge (in Beats) in Samples über BPM und Sample-Rate ist eine eigene, reine Berechnungseinheit an der Grenze zur Ausgabe. Die Umrechnung ist frei von Seiteneffekten und frei von Host-Abhängigkeiten.

### 16.3 Magic Constants

Der Code enthält keine Magic Constants. Alle Zahlenwerte (Timing-Liste, Modifier-Faktoren, Velocity-Grenzen, Channel-Anzahl, Lane-Anzahl, Step-Grenzen, Follow-Index-Maximum, Stimmen-Obergrenze) sind benannte Konstanten an zentraler Stelle. Die Velocity-Grenzen (0, 127, Delta-Bereich) sind an genau einer Stelle definiert.

### 16.4 Auswahl-Logik statt Verzweigungs-Orgien

Velocity-Modi und Index-Ermittlung sind datengetrieben oder als Strategie-Objekte implementiert. Jede Option ist eine eigene, benannte Einheit. Die verfügbaren Optionen sind an genau einer Stelle registriert und sichtbar. Neue Modi erfordern eine neue Einheit plus Registrierung, keine Änderung bestehender Verzweigungen. Lange if/else- oder switch-Ketten über Modi sind unzulässig.

### 16.5 Separation of Concerns

Jedes Modul besitzt genau eine Verantwortlichkeit. Input-Verarbeitung, Chord-Verwaltung, Lane-Engine, Timing, Output-Erzeugung und UI sind getrennte Einheiten. Kein Modul kennt die Interna eines anderen. Kommunikation erfolgt über schmale, definierte Schnittstellen.

## 17. Robustheit und Real-Time

Der Audio-Thread führt keine Allokationen, Locks oder blockierenden Aufrufe aus. Parameter-Änderungen gelangen vom UI-Thread über atomare Werte oder lock-freie Warteschlangen zum Audio-Thread. Der Kern speichert Zeit ausschließlich in Beats (PPQ), nie in Samples und nie als Wanduhr; die Umrechnung in Samples erfolgt ausschließlich an der Ausgabegrenze für Buffer-Offsets (Abschnitte 10, 16.2). Der Kern validiert alle Eingabewerte; ungültige Werte werden abgewiesen oder geklemmt. RAII gilt überall; rohe new/delete sind unzulässig. Der Kern wirft keine Exceptions im Audio-Pfad.

## 18. Testbarkeit

Unit-Tests sind Pflicht. Das Test-Framework ist Catch2. Die wiederverwendbaren Module sind ohne Host und ohne JUCE testbar (Abschnitt 16.1). Timing-Mathematik, Grid-Parser, Velocity-Berechnung, Index-Ermittlung und Stimmenverwaltung besitzen eigene Tests. Edge-Cases der Spezifikation (Index größer Chord, Chord-Re-Sortierung, Velocity-Klemmung, Überlappung, Retrigger-Policy, Transport-Reset, Host-Loop-Rücksprung, Note-Off-Garantie, Stimmen-Obergrenze) sind Testfälle.

## 19. Code-Stil

Der Code ist idiomatisch, performant, modular, robust, wartbar und erweiterbar. Eine Verantwortlichkeitseinheit pro Datei. Wiederverwendbare Module liegen in einem eigenen Verzeichnis und Namespace. Plugin-spezifischer Code liegt in einem getrennten Verzeichnis und Namespace. Namen sind selbstbeschreibend und ohne Abkürzungen. Includes sind minimal, Forward-Deklarationen werden genutzt, wo möglich. Kommentare begründen Entscheidungen und beschreiben keine offensichtliche Syntax.

## 20. Initialzustand

Jede Lane startet mit Basis-Timing 1/16, Unterteilung straight, Swing 0, Step-Anzahl 16 und allen Steps eingeschaltet. Der Velocity-Modus ist incoming velocity. Der Input-Modus ist Chord-Follow mit Index 0. Der Chord-Input-Channel ist 1. Der Output-Channel ist der Input-Channel der Quelle (Abschnitt 8). Transpose ist 0. Gate ist 100 Prozent. Reset-Mode ist free-running. Latch ist aus. Mute ist aus. Thru ist aus. Consume ist an.

## 21. Bypass-Verhalten

Bei Host-Bypass leitet das Plugin alle Eingangs-Events unverändert an den Output durch. Alle Lanes stoppen sofort; Latch endet (Abschnitt 4.6). Alle klingenden Noten der Lanes erhalten sofort Note-Offs. Beim Beenden des Bypass übernimmt jede Lane die aus dem Host-Playhead abgeleitete Position (Abschnitt 10). Es findet kein Reset auf Step 1 statt.
