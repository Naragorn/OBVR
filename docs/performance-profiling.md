# OBVR Performance-Profiling

Der Profiler ist standardmäßig aus. Er verändert weder die Stereo-Reihenfolge, die
Frame-Zeit, die Kamera noch die DXVK-Synchronisation. Er schreibt nur dann Messdaten,
wenn der Capture-Schalter gesetzt ist.

## Capture starten und finden

In `OBVR.ini` ergänzen oder den vorhandenen Abschnitt ändern:

```ini
[Performance]
Enabled=1
GpuTiming=0
CaptureSeconds=60
MaxRecords=65536
StartAfterSeconds=0
```

`bone_lock` (seit 2026-10-18) ist die Zeit, die der Bone-Lock in einem Szenen-Pass selbst
verbraucht (InterfaceRenderHook, HandleBoneUpload), je Pass summiert und am Pass-Ende als
eine Spanne markiert; ihr Anteil an `scene_pass` sagt, ob Leute im Bild unser Code oder
die Engine kostet. `StartAfterSeconds` (0 bis 3600, seit 2026-10-18) lässt das per `Enabled=1` scharf
gestellte Capture erst so viele Sekunden später beginnen - damit ein Tester die Stelle
erreicht, an der es ruckelt, bevor der 32-MiB-Puffer voll ist (bei 75 Hz reichen die
Datensätze für etwa 100 Sekunden). Das Log sagt "capture armed, it starts in N
seconds" und dann "capture 1 started".

OBVR liest die Einstellungen beim Start. Falls `ReloadEveryFrames` aktiv ist, wird
`Enabled=0` zum Stoppen verwendet; für ein weiteres Capture muss anschließend wieder
`0` und danach `1` gesetzt werden. `CaptureSeconds` wird auf 5–300 begrenzt,
`MaxRecords` auf 1024–262144 und zusätzlich auf die feste 32-MiB-Puffergrenze des
32-Bit-Prozesses. Ein Capture endet bei Zeit- oder Kapazitätslimit selbst.

Nach dem Ende steht der Export in einem eindeutigen Ordner neben `OBVR.log`, zum Beispiel:

```text
OBVR-performance-1/
  manifest.json
  cpu_events.csv
  gpu_samples.csv
  frames.csv
```

Ein fehlender Schreibzugriff beendet nur das Profiling; der VR-Renderer läuft weiter.
Die Dateien werden nach dem Capture in einem Writer-Thread geschrieben. Bei Shutdown
wird dieser Thread außerhalb des Renderpfads beendet.

## Auswertung

Die Auswertung verändert die Rohdaten nicht und braucht keine Zusatzpakete:

```text
python tools/analyze-performance.py OBVR-performance-1 --output performance-report
```

Erzeugt werden `summary.json` und `report.md`. CPU-Werte sind verstrichene Wall-Zeit
auf dem Spielthread. GPU-Werte sind Intervalle zwischen D3D9-Timestamp-Markern; beide
Uhren werden getrennt ausgewertet. Verschachtelte CPU-Spannen werden nicht addiert.
`p95` und `p99` verwenden nearest-rank. Ein Ergebnis ist zunächst eine Beobachtung,
keine automatische CPU-/GPU-Bottleneck-Diagnose.

Ein echter Profilerexport trägt `runtime_measurement=true`; das Beispiel unter
`docs/performance-example` trägt bewusst `false`.

`frames.csv` hält zusätzlich den tatsächlichen Pose-Rückgabecode sowie die Rückgaben
von Submit für linkes und rechtes Auge fest. `-1` bedeutet, dass dieser Aufruf für
den Frame nicht stattgefunden hat.

Bei den Zeilen `interop_flush`, `interop_lock` und `interop_held` steht in `pass_index`
der Besitzer der Klammer (`src/render/BracketOwner.h`): 1 poses, 2 game_frame,
3 eye_mirror, 4 hud, 5 crosshair, 6 hand_hud, 7 vignette, 8 canvas, 9 settings_menu,
0 unbenannt. Eingeführt nach der ersten Ingame-Aufnahme am 2026-10-06, in der zwei
Frames je ~100 ms in einer Klammer verbrachten, ohne dass die Datei sagen konnte, in
welcher.

Der Bericht trennt CPU-Spannen nach Liefermodus, GPU-Intervalle nach Liefermodus
und stellt Pass 0 gegen Pass 1 nur bei eindeutig gepaarten `world_dual`-Frames
gegenüber. Verschachtelte Spannen werden dabei nicht addiert.

## Messprotokoll

Pro Modus zuerst 30 Sekunden warm laufen lassen, danach drei 60-Sekunden-Läufe mit
gleicher Auflösung und identischer Diagnosekonfiguration aufnehmen:

- ruhiger Innenraum, Außenbereich mit Vegetation und NPC-Szene;
- Menü öffnen/schließen, Laden und erste/dritte Person;
- `Stereo=dual`, `Stereo=aer` und Mono als getrennte Vergleichsgruppen;
- Wasser separat testen, weil Wasserpfade eigene Kopien und Übergänge auslösen können.

Zuerst `GpuTiming=0`, danach `GpuTiming=1` messen. Die Eigenkosten müssen aus einem
identischen Szenario mit Profiler aus und aktivem CPU-only/GPU-Profil bestimmt werden.
Ein synthetischer Export oder ein einzelner Lauf beweist keine Ingame-Performance.

## Befunde aus den ersten Ingame-Aufnahmen (2026-10-06)

- `GpuTiming=1` lieferte unter DXVK keine GPU-Zeiten: die D3D9-Timestamp-Werte sind
  Host-Nanosekunden seit Epoche (Differenz Begin/End ~1 µs). DXVK bedient
  `D3DQUERYTYPE_TIMESTAMP` nicht aus GPU-Zeitstempeln. Für GPU-Zeiten bleibt fpsVR bzw.
  SteamVRs eigene Frame-Timing-Anzeige.
- Die ~100-ms-Hänger lagen in den Overlay-Klammern (`interop_held`, Besitzer hand_hud,
  crosshair, vignette, hud, eye_mirror) und endeten jeweils auf einer Zeile `Created
  shared texture 'Scene create Vulkan, N'` in `vrclient_Oblivion.txt`: DXVKs
  Defragmentierer hatte die Textur verschoben, SteamVR importierte das neue VkImage.
  Abhilfe `dxvk.enableMemoryDefrag = False` (siehe docs/vr-modding/ui-and-hud.md).
- `poses_wait`-Stalls von 23-28 ms fielen auf die Millisekunde mit `***** Reloading
  Shaders Begin` in `vrcompositor.txt` zusammen; der Compositor lädt seine Shader alle
  paar Sekunden neu, auch ohne laufende App. Auslöser noch offen (Verdacht: der
  Custom-Shader des sboys3-Treibers, der bei jedem Config-Reload die Compositor-Shader
  ersetzt).

## Grenzen

Ein großer `poses_wait_wall_ms`-Wert kann normales Compositor-Pacing sein. Ein großer
`scene_pass_wall_ms`-Wert kann Engine-Arbeit oder Treiberwartezeit enthalten. GPU-Marker
zeigen keine Shaderauslastung und keine exklusive GPU-Zeit. Reprojection oder verfehlte
Headset-Frames dürfen nur mit separater SteamVR-Evidenz behauptet werden.

Das Profiling selbst parallelisiert keine Augen. Die vorhandene Dual-Pass-Logik bleibt
unverändert; eine spätere Render-Parallelisierung wäre ein eigenes Architekturprojekt.

## Gemessen 2026-10-18: Leute im Bild (Harness, Linux/Proton, 75 Hz)

Anlass: ein Quest-3-Tester (120 Hz, RTX 5070 Ti) ruckelt in Jauffres Haus mit zwei
sprechenden NPCs, besser im Dialog (die Welt pausiert) und kurz, wenn einer wegschaut.
Szenarien `tools/hand-scripts/perf-people-hall.txt` (die Halle des Harness-Saves, dann
sieben per `player.placeatme` gesetzte Leute - Jauffre, Piner, Maborel, Eronor, ein
Legionär - im Ring um den festgehaltenen Spieler) und `perf-jauffre.txt` (Weynon Priorys
Haus); Auswertung je Phase mit `tools/perf-phases.py <Lauf> <Marken...>`.

| | Halle leer | Halle, 7 Leute | Haus, leer wie voll |
|---|---:|---:|---:|
| Frame-Zeit Mittel | 13,33 ms | 13,9 ms | 26,7 ms |
| scene_pass je Auge | 0,70 ms | 4,3 bis 5,1 ms | 6,9 bis 7,8 ms |
| poses_wait (Leerlauf vor dem Compositor) | 10,0 ms | 1,5 bis 2,7 ms | 8,6 bis 10,5 ms |
| Frames über 20 ms | 0 % | 4 bis 6 % | 100 % |
| Draws je Pass | 322 | 550 bis 660 | 1700 bis 1850 |
| geskinnte Draws je Pass | 2 | 62 bis 88 | 86 bis 120 |
| bone_lock je Pass | - | 0,056 ms (max 0,27) | - |

Befund: sieben Leute kosten hier rund 4,3 ms CPU je Augen-Pass, zwei Pässe je Frame,
etwa 0,6 ms je Person und Auge; der Bone-Lock trägt davon 1 %. Die Zeit liegt im
Szenen-Render der Engine selbst (ihre Knochenpaletten und Draws je Person, unter DXVK),
den der Dual-Pass zweimal fährt. OBVRs Arbeit außerhalb der Pässe (frame_end_callback
0,6 ms) ändert sich nicht. Das Haus kostet schon leer zwei Vsync-Perioden, dort versteckt
sich jeder Zuschlag unter der Quantisierung; die Halle hat Luft und zeigt ihn.

Daraus folgt für 120 Hz (8,3 ms Budget): zwei bis drei Leute im Bild reichen, um über
das Budget zu kommen, das passt zum Bericht. Hebel, ungebaut: `Render.Stereo=aer`
(Alternate Eye Rendering, je Frame ein Auge - halbiert den Pass-Anteil, gibt es schon,
Kosten des Versuchs null); den zweiten Pass von Arbeit entlasten, die das erste Auge
schon getan hat (Paletten, Kulling) - groß, Engine-Innereien; die Engine-Kosten je NPC
selbst sind Vanilla.

Nebenbefunde der Läufe: Oblivions Auto-Vanity-Kamera kreist nach 120 s ohne Eingabe um den
Spieler, die Harness-Skripte setzen `SetNumericINISetting "fAutoVanityModeDelay:Camera"
100000` (xOBSE); ein eigener Block in OBVR für VR steht aus (klein). Das Bewegen
persistenter Referenzen per Konsole (`JauffreRef.moveto player 0 180 0`) brachte in
sieben Läufen niemanden ins Bild, `player.placeatme <Basis-ID> N` tut es; die Basis-IDs
stehen in den zlib-komprimierten NPC_-Records der esm (Jauffre 00023999, BrotherPiner
000223AD, PriorMaborel 000223AB, Eronor 000223AF, ImperialLegionSoldier 0018B118).

