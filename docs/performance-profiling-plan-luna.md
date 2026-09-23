# OBVR: Performance-Profil – Umsetzungsauftrag für Luna

Stand: 2026-09-20. Dieser Text ist ein Implementierungsplan, keine bereits implementierte Funktion und kein gemessenes Performance-Ergebnis.

## 1. Auftrag und Ergebnis

Implementiere einen abschaltbaren Profiler für den bestehenden OBVR-Renderer. Er soll beantworten:

1. Wie viel verstrichene Zeit beanspruchen erster und zweiter Szenenrender auf dem Spielthread?
2. Welche GPU-Zeitintervalle gehören zu diesen Renderdurchläufen und den Augenkopien?
3. Wie viel Zeit vergeht bei Pose-Warten, DXVK-Flush, Queue-Lock, Submit und Monitor-Present?
4. Welche Kosten kommen von HUD und Kamerawechsel zwischen den Augen?
5. Welche Messungen sprechen für doppelte CPU-Vorbereitung, GPU-Limit oder Synchronisation? Was bleibt unbestimmt?

Der Nutzer hat den Head-Gaze-Modus ingame als sehr gutes VR-Erlebnis bestätigt. Dieses Verhalten ist die Ausgangsbasis. In diesem Auftrag keine Renderparallelisierung, keine Änderung der Spielzeit, Kamera, Queue-Synchronisation oder Stereoqualität und keine Reparatur der zwei separat gefundenen SubmitPolicy-Fehler. Ziel ist zunächst belastbare Beobachtung.

Liefern: Instrumentierung, pure Zustandslogik, Tests, Auswertungsskript, kurze Bedienungsanleitung, synthetisches Beispielexportformat und ein ausdrücklich als synthetisch markierter Beispielbericht. Echte Messergebnisse erst nach einem tatsächlich ausgeführten Spieltest. Ohne verfügbares Headset/Spiel bleibt die Laufzeitabnahme offen; den implementierbaren Teil trotzdem vollständig fertigstellen.

## 2. Verbindliche Arbeitsweise

- Lies die geltenden AGENTS.md-Anweisungen und den aktuellen Code. Zeilennummern dieses Plans sind Orientierung; Funktionsnamen sind die Anker.
- Ändere keine fremden Arbeitsstände. Keine Installation in die aktive Spielinstallation und kein Start eines Benchmarks im laufenden Spiel allein aufgrund dieses Plans.
- Externe APIs zuerst anhand verfügbarer Dokumentations-MCPs nachschlagen: exa, bei Interna DeepWiki, für exakte Signaturen Context7. Exa war bei der Planerstellung nicht verfügbar; deshalb wurden offizielle Webquellen sowie DeepWiki und Context7 verwendet. Verfügbarkeit bei Umsetzung erneut prüfen, fehlende Quellen offen benennen.
- Alle Entscheidungen als pure Funktionen/Zustandsmaschinen über normale Werte extrahieren; jede semantisch unterschiedliche Erfolgs-, Fehler-, Ablehnungs- und Übergangsfolge testen.
- Neue Funktions-, Datei-, Config- und CLI-Namen unten sind Entwurfsvorgaben, keine Behauptung über bereits vorhandene Schnittstellen.
- Kleine nachvollziehbare Schritte: CPU-Grundlage → GPU-Messung → Export/Auswertung → Abnahme. Keine neue Engine-Architektur nebenbei.

## 3. Gelesene Ausgangspunkte

| Datei / Funktion | Relevanz |
|---|---|
| `src/render/SceneRenderHook.cpp`, `HookedRenderScene` | Erster und zweiter Aufruf von `g_original`; Capture/Replay-Zustände; globale Frame-Zeit nur im zweiten Durchlauf nullgesetzt |
| `src/camera/CameraHook.cpp`, `BetweenScenePasses`, `AfterSecondScenePass` | Erste/zweite Augenkopie, HUD, Kameraverschiebung und Wiederherstellung |
| `src/render/HeadsetRenderer.cpp`, `BeginFrame`, `EndFrame` | Pose-Abfrage, Routing zu Dual/AER/Mono/Held/Flat und Bildübergabe |
| `src/render/HeadsetRenderer.cpp`, `CaptureEye`, `SubmitDualEyes`, `SubmitHeldEyes`, `SubmitAlternateEyes`, `SubmitMono`, `Reset` | Tatsächliche Ergebnisse und Fallbacks; Ressourcen-Lebenszyklus |
| `src/render/InteropBracket.cpp`, `Begin`, `Release` | `FlushRenderingCommands`, `LockSubmissionQueue`, Layoutwechsel, Freigabe |
| `src/render/EyeMirror.cpp`, `CopyBackBuffer` | GPU-Kopieraufrufe einschließlich Filter-Fallback und möglicher Kopie in beide Augen |
| `src/render/PresentHook.cpp`, `HookedPresent` | Frame-End-Callback und danach der originale Present-Aufruf |
| `src/camera/CameraHook.cpp`, `OnFrameEnd`, `PrepareMenuFrameIfNeeded` | Frames ohne normalen Kamera-Update und unterschiedliche Lieferpfade |
| `src/render/D3D9Types.h`, `src/platform/Win32Min.h`, `cmake/imports/` | Eigene ABI-Deklarationen und Build ohne Windows SDK berücksichtigen |
| `src/core/Config.h`, `Config.cpp`, `OBVR.ini` | Konfiguration und Hot Reload |
| `tests/CMakeLists.txt`, `tests/FrameLogicTest.cpp`, `tests/PresentHookTest.cpp` | Bestehende Testmuster und Registrierung |

Vor dem Editieren die tatsächlichen Eintrittspunkte/Signaturen prüfen. Nicht anhand dieses Plans neue Hook-Adressen erfinden.

## 4. Messmodell: zuerst implementieren

Vorgeschlagene Module:

- `src/perf/ProfileLogic.h/.cpp`: Zustandsmaschine, IDs, Gültigkeit, Kapazitäten; ohne Windows/D3D/OpenVR.
- `src/perf/Profiler.h/.cpp`: Laufzeitadapter, CPU-Zeitstempel, feste Speicherpuffer, Export.
- `src/perf/GpuQueries.h/.cpp`: D3D9-Adapter mit injizierbarer Query-Schnittstelle für Tests.
- `tools/analyze-performance.py`: ausschließlich Auswertung gespeicherter Daten, keine Spielsteuerung.
- `docs/performance-profiling.md`: Bedienung, Messprotokoll, Grenzen.

### Identität und Grenzen

Nicht Kameraaufrufe, Szenenaufrufe und VR-Frames gleichsetzen. Definiere separat:

- `session_id`: ein Capture; zusätzlich Schema-Version und Gerätegeneration.
- `present_id`: monotoner Zähler am Eintritt des äußersten `HookedPresent`.
- `scene_id` und `pass_index`: einzelner Szenenaufruf und dessen Renderdurchlauf.
- `vr_frame_id`: jeder Versuch eines `BeginFrame`, inklusive Ergebnis.
- `sample_id`: eigene unverwechselbare ID für asynchrones GPU-Ergebnis.

CPU-Ereignisse besitzen absoluten monotonen Tick, Start/Ende, Ereignistyp, IDs und gegebenenfalls Elternspanne. Nicht zuordenbare erste/letzte Ereignisse bleiben explizit unzugeordnet; niemals dem nächsten Frame unterschieben.

Der Present-Aufruf bekommt seine ID vor `g_callback`; Zeit um `g_original` getrennt erfassen. Renderereignisse davor gehören zum nächsten Present. Ereignisse im Callback können zusätzliche VR-/Menüarbeit erzeugen und gehören zum aktuellen Present. Diese Bindung als pure Logik testen. Mehrere Szenen oder BeginFrame-Versuche vor einem Present erhalten eigene Unter-IDs. Bei Rekursion verschachtelte Aufrufe markieren, keinen zweiten äußeren Frame erzeugen.

Pro Frame erfassen: angeforderter Stereomodus, tatsächlicher Lieferpfad, Welt/Menü/Laden soweit im bestehenden Code eindeutig bestimmbar, Capture-Erfolg je Auge, Passreihenfolge und tatsächliches Auge, Submit-Rückgaben, Pose-Ergebnis. Unbekannte Kategorien heißen `unknown`. Wechsel von Config, Gerät oder Modus erzeugen eine neue Auswertungsgruppe/Generation.

### Zeitbedeutung

- CPU-Spannen über `QueryPerformanceCounter`, Frequenz einmal initialisieren. Namen enden auf `_wall_ms`: verstrichene Zeit auf dem Thread, NICHT reine CPU-Rechenzeit.
- `present_interval_ms`: Abstand zweier äußerer Present-Eintritte, keine Behauptung über tatsächlich im Headset angezeigte Bilder.
- Verschachtelte Spannen nicht addieren. Gesamtspanne und Kinder als solche exportieren. Exklusive Zeit nur über die Vereinigungsmenge gültiger Kinderintervalle berechnen.
- CPU und GPU können sich überlappen; ihre Zeiten NICHT zu einer Framezeit addieren. Keine direkte Subtraktion von CPU- und GPU-Zeitstempeln unterschiedlicher Uhren.
- Uninstrumentierte Zeit heißt `unattributed_wall_ms`, nicht `simulation_ms`. Der aktuelle Hook reicht nicht aus, die gesamte Simulation isoliert zu messen.
- Fehlende/ungültige Werte sind leer plus Status/Grund, niemals eine künstliche Null.

## 5. Konkrete Instrumentierung

| Messung | Exakte Einfügestelle | Bedeutung / Vorsicht |
|---|---|---|
| `scene_pass_wall_ms` | Direkt vor/nach jedem originalen Szenenrender in SceneRenderHook | Pass 0/1 und reales Auge; normalen Single-Pass ebenfalls erfassen; keine Rekursion doppelt aggregieren |
| `between_passes_wall_ms` | Um `g_callbacks.betweenPasses()` | Elternspanne der folgenden Teilmessungen |
| `eye_capture_wall_ms` | Um `CaptureEye`, Ergebnis festhalten | CPU-Aufrufdauer; nicht die GPU-Kopierdauer |
| `hud_between_wall_ms` | Um `RunHudPassWithCrosshairView` | HUD-Aufwand separat sichtbar |
| `eye_camera_shift_wall_ms` | Kameraverschiebung plus `UpdateNodeTransforms` in BetweenScenePasses | Gemeinsame Zustandsänderung unangetastet lassen |
| `eye_camera_restore_wall_ms` | Wiederherstellung plus Transform-Update in AfterSecondScenePass | Nicht versehentlich dem zweiten Szenenrender zuschlagen |
| `after_second_wall_ms` | Um AfterSecondScenePass-Callback | Elternspanne der zweiten Kopie und Wiederherstellung |
| `begin_frame_wall_ms` | Um BeginFrame | Einschließlich Setup; Setup-Frames markieren |
| `poses_wait_wall_ms` | Ausschließlich um `backend.WaitGetPoses()` | Synchronisation/Pacing; nicht automatisch ein Performancefehler |
| `interop_flush_wall_ms` | Um FlushRenderingCommands in InteropBracket::Begin | Zeit kann Backend-Aufarbeitung/Warten enthalten |
| `interop_lock_wall_ms` | Um LockSubmissionQueue | Erwerbsdauer; getrennt von Dauer des gehaltenen Locks |
| `interop_held_wall_ms` | Erwerb erfolgreich bis Freigabe, IDs für Bracket und Aufrufer | Layoutwechsel und Submit können darin liegen; verschachtelt ausweisen |
| `submit_left/right_wall_ms` | Um tatsächliche SubmitEye-Aufrufe | Versuch, API-Rückgabe, Texturpfad und VR-ID; keine GPU-Renderzeit |
| `frame_end_callback_wall_ms` | Um g_callback in HookedPresent | Enthält je nach Pfad Menüarbeit und Submit |
| `monitor_present_wall_ms` | Um originales Present, Rückgabe unverändert zurückreichen | Mögliche Monitor-/Treiber-Wartezeit getrennt vom VR-Submit |

Scopes müssen auch bei allen frühen Rückgaben korrekt enden. Für Interop einen optionalen Kontext-Tag weiterreichen (Poses/Submit/sonstige); der Profiler darf selbst keine Queue-Sperre oder Flush ergänzen. Keine Grafik-API vom Exportthread aufrufen.

Bereits vorhandene Draw-/State-Zähler können als Differenzen verwendet werden. Nicht jeden Draw mit QPC instrumentieren und keine neue teure Geometrie-/Buffer-Inspektion einschalten. Vorhandene Diagnostik identisch in Vergleichsläufen halten und im Manifest dokumentieren.

## 6. GPU-Zeitintervalle ohne erzwungenes Warten

Verwende D3D9-Timestamp-Queries über das vorhandene Spielgerät. Die offiziellen Query-Unterlagen dokumentieren TIMESTAMP/END/UINT64, TIMESTAMPDISJOINT/BEGIN+END/BOOL und TIMESTAMPFREQ/END/UINT64 [S2]. Exakte COM-Slots, Calling Convention, Layouts und HRESULT-Werte vor Umsetzung aus SDK-Headern belegen und ABI-Tests hinzufügen. Nicht aus Erinnerung abschreiben.

Pro aufgenommenem Weltframe zunächst Markerpaare für Szenenpass 0, tatsächliche erste Augenkopie, Szenenpass 1 und tatsächliche zweite Augenkopie. Marker um die GPU-Arbeit der Kopie, einschließlich tatsächlicher Fallback-Kopien; CPU-only Vorbereitungen nicht als Kopierkosten etikettieren. Single-/Flat-/AER-Aufrufe erhalten ihre tatsächliche Operation, keine erfundenen zweiten Szenenwerte.

Vorgehen:

1. Beim Capture-Start einmal Query-Unterstützung prüfen; Pool vorab begrenzt allokieren (Startwert 16 ausstehende Frames, feste maximale Markerzahl). Teilweise fehlgeschlagene Erstellung vollständig aufräumen.
2. Disjoint-Bereich und Zeitstempel gemäß dokumentiertem Protokoll ausgeben. Keine Queries innerhalb eines von OBVR bereits gehaltenen Interop-Queue-Locks ausgeben oder lesen. Marker außerhalb der vorhandenen Sperrbereiche platzieren.
3. Erst in späteren Frames fertig ausgegebene Query-Sätze abfragen. `GetData(..., 0)` ohne FLUSH verwenden, keine Warteschleife. Pro Frame höchstens ein begrenztes Budget alter Sätze prüfen, z. B. vier.
4. `S_FALSE` bedeutet noch nicht verfügbar. ID und Queries behalten. Andere Fehler, Disjoint=true, Frequenz=0, fehlende Marker und rückwärts laufende Ticks machen das Sample ungültig.
5. Pool voll: neue GPU-Messung auslassen und `gpu_pool_full` zählen; CPU-Messung fortsetzen. Kein erzwungener Flush, kein Überschreiben ausstehender Queries.
6. Nach festem Alterslimit (Vorgabe 240 Presents) nicht fertige Sätze als Timeout markieren; Query-Generation kontrolliert deaktivieren/aufräumen, keine endlose Reinitialisierung. DXVK kann bei verlorenem Gerät mit Flags=0 weiterhin S_FALSE liefern [S3]; deshalb Timeout auch ohne expliziten Device-Lost-Code testen.
7. Gerätwechsel, bekannter Reset/Loss, Profiler-Stop und Shutdown invalidieren Zuordnungen. Bestehende Ressourcen-Lebenszyklen zuerst kartieren. Keine neue umfassende Reset-Hook-Architektur nur für den Profiler; wenn ein Lebenszyklus nicht sicher behandelbar ist, GPU-Messung für diese Generation beenden und neu starten erst nach sicherem Geräte-Setup.
8. Ergebnis mit ursprünglicher sample_id/frame_id ausgeben, zusätzlich Alter in Presents. Späte Ergebnisse niemals am Abfrageframe verbuchen. Beim Capture-Ende offene Ergebnisse als pending_at_stop exportieren; nicht warten.

Umrechnung: `(end_tick - start_tick) * 1000.0 / frequency` mit validierter Reihenfolge und breiter Arithmetik. GPU-Werte heißen `gpu_interval_ms`: sie belegen Zeit zwischen GPU-Markern, keine Shaderauslastung und keinen garantierten exklusiven GPU-Servicebedarf. Pipelineblasen/Abhängigkeiten bleiben möglich. Nicht aus großen Markerabständen allein ein GPU-Limit ableiten.

## 7. Capture, Speicher und Export

Neue, ausdrücklich erst zu implementierende Einstellungen:

```ini
[Performance]
Enabled=0
GpuTiming=0
CaptureSeconds=60
MaxRecords=65536
```

- Enabled 0→1 startet genau ein Capture nach sicherer Initialisierung; nach Zeitlimit automatisch abgeschlossen, keine automatische Endlosschleife bei weiter gesetztem Enabled=1. Ein erneutes Capture braucht 1→0→1. Änderungen während Capture als stop/restart an sicherer Grenze behandeln und markieren.
- Vorgeschlagene Grenzen: CaptureSeconds 5..300; MaxRecords 1024..262144. Fehlende/ungültige Werte auf dokumentierte Defaults begrenzen; sämtliche Parserpfade testen. MaxRecords bezeichnet Ereignis-/Ergebnisdatensätze, nicht Frames. Speicherbedarf vor Start berechnen und begrenzen; höchstens 32 MiB Profilerpuffer im 32-Bit-Prozess. Erreichtes Limit beendet Capture mit `truncated_capacity`, ohne den Renderer anzuhalten.
- Standard aus: keine QPC-Aufrufe, Queries, Pufferallokationen oder Exportarbeit im Framepfad. Ein billiger Aktivitätscheck ist zulässig.
- Im aktiven Framepfad ausschließlich feste Binärdatensätze schreiben. Keine Dateiausgabe, Stringformatierung oder Speicherallokation pro Frame.
- Nach Capture einen unveränderlichen Puffer an einen dedizierten Writer übergeben. Nur dieser schreibt Exportdateien; keine Engine-/D3D-/OpenVR-Zugriffe. Puffer erst nach abgeschlossenem Schreiben wiederverwenden. Bei erneutem Start während Export `busy` zurückmelden. Join/Warten nie im Renderpfad; sicheren Shutdown und DLL-Lebensdauer explizit lösen. Keine Nutzung des bestehenden Watchdogs als Writer.
- Ausgabe pro Session: `manifest.json`, `cpu_events.csv`, `gpu_samples.csv`, `frames.csv`. Gemeinsame IDs statt verspätete GPU-Werte in schon geschriebene Zeilen zu patchen. Unvollständige Dateien als `.partial`, Abschlussstatus im Manifest; keine früheren Captures überschreiben.
- Ausgabeort als neuer Unterordner neben dem existierenden OBVR.log-Ort, session-eindeutig. Bei fehlenden Schreibrechten Profiler mit einmaliger Meldung beenden; VR läuft weiter. Kein unkontrollierter Pfad-Fallback.
- Manifest: Schema, Build-Version/Commit soweit zuverlässig eingebettet, Startzeit, QPC-Frequenz, Settings-Snapshot, Query-Status, Kapazitäts-/Fehlerzähler, echte Renderzielgrößen, Szenario-Label. Headset-Hz, GPU/CPU, Treiber-, DXVK- und SteamVR-Version aus belegbaren vorhandenen Quellen oder als manuell ergänzte Angaben; unbekannt bleibt unbekannt. Keine neuen Fremd-APIs nur zum Füllen eines Feldes.
- Handbuch braucht klare Start-/Stop-/Dateifinden-Schritte und einmalige Statusmeldungen in OBVR.log. Nutzer muss keine internen IDs bedienen.

## 8. Auswertung

Implementiere `tools/analyze-performance.py CAPTURE_DIRECTORY --output REPORT_DIRECTORY` als neue CLI, mit Standardbibliothek soweit praktikabel. Dokumentiere tatsächliche CLI im Handbuch. Erzeuge Markdown-Bericht und maschinenlesbares Summary-JSON; keine weiteren Pakete für eine reine Tabelle installieren.

- Schema/IDs/Einheiten prüfen; beschädigte, doppelte und fremde Session-/Geräte-IDs mit Grund zurückweisen. Rohdaten nicht verändern.
- Welt-Dual, Welt-AER, Mono/Fallback, Live-Menü, Held-Menü, Flat/Laden, Setup und Übergänge getrennt auswerten. Unbekannte Fälle separat zählen.
- Je Metrik Anzahl gültig/fehlend, Mittelwert, Median, p95, p99 und Maximum. Quantilmethode exakt festlegen und testen (nearest-rank für n>0 genügt).
- Pass 0 gegen Pass 1 nur innerhalb desselben gültigen Dual-Frames vergleichen. Tatsächliche Augenreihenfolge berücksichtigen. Separate Diagramm-/Tabellenwerte für CPU-Wall und GPU-Intervalle.
- Keine Summe verschachtelter Spannen. GPU-Intervalle nur addieren, wenn die Messgrenzen deren Nichtüberlappung tatsächlich sichern; ansonsten getrennt zeigen.
- Framebudget nur mit bekannter Headset-Hz als `1000/Hz`. Zahl überschrittener App-Present-Intervalle ist KEINE Anzahl verfehlter Headset-Frames. Reprojection/Dropped-Frames nur mit separat vorhandener SteamVR-Evidenz behaupten.
- Ergebnisformulierungen: Beobachtung → Hypothese → Gegenprüfung. Zunächst keine automatische definitive CPU-/GPU-Bottleneck-Klassifikation.
- Beispiele: große poses_wait-Spanne kann normales Pacing sein; große scene_pass_wall-Spanne kann CPU-Arbeit oder Treiberwartezeit sein; große GPU-Intervalle können Pipelineblasen enthalten. Für reine CPU-Arbeit/Thread-Auslastung wäre danach ein gesondertes Sampling-Profil erforderlich, dessen Toolbedienung erst bei Bedarf dokumentiert recherchiert wird.

## 9. Tests: vollständige Flows, nicht nur Prozentrechnung

Neue Testdateien passend zu den Modulen registrieren, einschließlich Python-Auswertungstest. Produktionsadapter über Fake-Clock, Fake-Queries und Fake-Writer testbar machen.

Pflichtfälle:

1. Profiler aus: null Aufrufe an Clock/GPU/Writer; Start/Stop/Neustart, automatisches Ende ohne Wiederanlauf, busy Export, ungültige Config und Speicherlimit.
2. Frames: zwei Augen normal und vertauscht, AER, Mono, Flat, Held, Live-Menü ohne Kamera, mehrere Szenen je Present, fehlender Present, doppelter BeginFrame, Rekursion, Stop mitten in einer Spanne, fehlende Endmarker.
3. IDs: Start vor erstem Present, Callback erzeugt Renderarbeit, GPU-Ergebnis erst N Presents später, Reihenfolge vertauscht, alte Gerätegeneration, Zählergrenzen; keine stillen Fehlzuordnungen.
4. Zeit: monotone/gleiche/rückwärts laufende Ticks, Frequenz null, große Tickwerte, Überlaufgrenzen, verschachtelte und überlappende Kindspannen, fehlende Spannen; keine negativen erfundenen Restzeiten.
5. GPU: alle Querytypen unterstützt/fehlend; Teilallokation scheitert; Issue-Fehler an jeder Markerposition; pending→ready, dauerhaft pending, GetData-Fehler, Disjoint, Frequenzfehler, Pool voll, Timeout, Device Loss/Reset und Stop mit ausstehenden Queries. Fake muss FLUSH-Flags und blockierende Pollschleifen erkennen/ablehnen; Freigaben exakt einmal.
6. Export: normal, Kapazität erreicht, Writer-Start fehlgeschlagen, Zugriff verweigert, Teilwrite/Datenträgerfehler, Shutdown während Export, erneuter Start während Export. Ein kaputter Profiler darf Rendering nicht stoppen.
7. Auswertung: leere/partielle Dateien, falsches Schema, ungültige Zahlen, doppelte IDs, gemischte Modi, Ausreißer, Quantile bei n=1/2, unterschiedliche Samplezahlen, fehlende GPU-Werte, keine Hz und CPU/GPU-Überlappung. Keine falsche definitive Diagnose aus synthetischen Wartezeiten.
8. ABI: SDK-kompatible Größen und Query-Signaturen für Win32; bestehende D3D9Types-/PresentHook-Tests erweitern, bestehende No-SDK-Buildpfade nicht brechen.

Führe zuerst betroffene Tests aus, danach DLL-Release-Build und die vollständige Testsuite. Bestehendes Muster: `cmake -S tests -B <test-build> -A Win32`, `cmake --build <test-build> --config Release`, `ctest --test-dir <test-build> -C Release --output-on-failure`. Einen vorhandenen Build nur mit passendem Generator weiterverwenden. Python explizit korrekt erkennen lassen; fehlende Python-Tests nicht als vollständige Suite verkaufen.

Referenz aus dem Review dieses Datums: 65/65 bestehende Tests bestanden nach Win32-Testbuild. Das ist kein Beleg für die neue Instrumentierung. Lokaler PATH/Path-Duplikatfehler betraf MSBuild-Umgebung; Python-tempfile-Zugriffe scheiterten in Sandbox und funktionierten außerhalb. Falls erneut auftretend, Ursache behandeln und Testlog berichten, keine Tests überspringen oder abschwächen.

## 10. Ingame-Protokoll und Eigenkosten

Erst nach erfolgreichem Build und Tests eine konkrete Testversion mit Anleitung bereitstellen. Für die Laufzeitmessung dieselben Saves, Grafik-/VR-Einstellungen und Modliste verwenden. Keine unangefragten Settings-Änderungen.

### Stufe A: Instrumentierung abnehmen

- 30 Sekunden Aufwärmen, danach 60 Sekunden Capture je Szene. Das sind Protokollvorgaben, keine Behauptung über ausreichendes Shader-Warmup auf jeder Maschine; sichtbar laufende Kompilierung/Loading kennzeichnen und Versuch wiederholen.
- Szenen: ruhiger Innenraum; Außenansicht mit Vegetation; gleiche Außenansicht mit NPCs soweit reproduzierbar; Menü öffnen/schließen; separater Ladeübergang. First-/Third-Person separat markieren.
- Eine kurze bekannte Bewegung/Kameraänderung als Plausibilitätskontrolle; zwei Augen bleiben korrekt, Head-Gaze, HUD, Menüs und Recenter funktionieren wie zuvor.
- Wasser-Umschaltung separat als Fehler-/Übergangsfall, nicht in den stabilen Performance-Mittelwert mischen. Der Rendering-Checkpoint dokumentiert dort offene Fehler.

### Stufe B: Overhead prüfen

- Drei Modi derselben Build vergleichen: aus, CPU-only, CPU+GPU. Pro Modus mindestens drei 60-s-Wiederholungen derselben statischen Ansicht, Reihenfolge abwechseln.
- Für den Modus aus eine unabhängige identische Messquelle für alle drei Modi nutzen, etwa SteamVR-Frame-Timing-Erfassung, deren konkrete Bedienung vor dem Test anhand offizieller Dokumentation festzuhalten ist. Der abgeschaltete Profiler kann seine eigenen Kosten nicht messen.
- Vorgeschlagenes Abnahmeziel, keine Vorhersage: Median-Mehrkosten höchstens 0,2 ms und 2 %, p95 höchstens 0,5 ms und 5 % für vergleichbare extern gemessene Framezeiten. Streuung mitberichten. Reicht das Verfahren nicht zum Nachweis, Status `Overhead nicht nachgewiesen`, nicht bestanden erfinden.
- Bei relevanten Eigenkosten Markerzahl/Sampling reduzieren und Samplingrate im Manifest speichern. Langsame Frames nicht selektiv verwerfen. Ist Sampling nötig, Auswertung darf keine vollständige p99-GPU-Verteilung behaupten.

### Stufe C: Hypothesen gezielt prüfen

- Falls GPU-Limit vermutet: gleicher Save/Blick mit zwei dokumentierten tatsächlichen Renderauflösungen; alle anderen Einstellungen gleich. Auflösung nur durch einen existierenden verifizierten Einstellweg ändern.
- Falls zweite CPU-Vorbereitung teuer: gepaarte Szenenpass-Zeiten, Drawzahlen und Warteanteile vergleichen; danach gezielte feinere Scopes in genau diesem Pfad statt pauschal neue Threads.
- Optional Dual/AER als Zusatzversuch. Unterschiede der Qualität und Zahl frisch gerenderter Augen ausdrücklich benennen; AER ist kein identischer Qualitätsvergleich.
- Pacing, feste Refresh-Grenzen und Reprojection können FPS-Unterschiede verdecken. Deshalb Zeitintervalle und unabhängige SteamVR-Beobachtung gemeinsam berichten.

## 11. Fertig-Kriterien und Abschluss an den Nutzer

Implementierung fertig, wenn alle genannten Module verbunden sind, Config/CLI dokumentiert sind, neue Flows und vorhandene Tests bestehen, DLL baut, synthetischer Export vollständig auswertbar ist und alle instrumentierten Originalaufrufe ihre Reihenfolge/Argumente/Rückgaben behalten.

Laufzeitprofil belastbar erst, wenn echte Captures gültige Zuordnungen/GPU-Werte zeigen, vorhandenes VR-Verhalten bestätigt wurde, Eigenkosten geprüft wurden und Bericht samt Rohdaten reproduzierbar ist. Fehlt dieser Schritt, Abschluss ausdrücklich: **Implementiert und automatisiert geprüft; Ingame-Messung und Overhead-Abnahme noch offen.**

Abschluss enthält: veränderte Dateien, tatsächliche Build-/Testbefehle und Ergebnisse, genaue Aktivierungsanleitung, Speicherort der Exporte, bekannte Grenzen und höchstens drei durch Messungen priorisierte nächste Untersuchungen. Keine FPS-Versprechen und keine automatische Parallelisierung.

## 12. Quellen und offene Verifikation

- [S1: Microsoft – QPC misst verstrichene Zeit](https://devblogs.microsoft.com/oldnewthing/20080908-00/?p=20963): Grundlage für die Bezeichnung CPU-Wall statt CPU-Rechenzeit.
- [S2: Microsoft – D3D9 Queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries): Query-Tabelle, Issue-Protokoll, Datentypen, S_OK/S_FALSE. Context7 lieferte daneben eine widersprüchliche Kurzbeschreibung von TIMESTAMPFREQ; die konsistente Query-Tabelle und SDK-Header haben Vorrang.
- [S3: DXVK – d3d9_query.cpp](https://github.com/doitsujin/dxvk/blob/master/src/d3d9/d3d9_query.cpp): Query-Unterstützung, GetData und Flags, Frequenz, Disjoint-Implementierung. Bei Planung master gelesen; Verhalten der tatsächlich eingesetzten DXVK-Version vor Laufzeitabnahme gegen ihren Tag/Commit prüfen. DeepWiki-Zusammenfassungen ersetzen diesen Quelltext nicht.
- [S4: Microsoft – Profiling von D3D9-Aufrufen](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls): CPU-Aufrufdauer nicht mit GPU-Ausführung gleichsetzen. Synchronisierende Microbenchmark-Verfahren nicht in den normalen VR-Framepfad übernehmen.
- [S5: Microsoft – D3D9 Threading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-differences): Thread-Sicherheit ist keine automatische Renderparallelisierung.
- Repo-Evidenz: oben genannte Quelldateien sowie `docs/rendering-checkpoint-2026-09-20.md`. Die aktive installierte DLL, aktuelle Runtime-Versionen und konkrete CPU-/GPU-Engpässe wurden bei der Planerstellung nicht gemessen.
