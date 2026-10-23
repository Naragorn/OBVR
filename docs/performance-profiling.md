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
  compositor.csv
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

`compositor.csv` (seit 2026-10-19) ist die Sicht des Compositors auf jeden fertigen Frame:
SteamVRs `Compositor_FrameTiming`, nach jedem WaitGetPoses für die ein und zwei Frames zurück
liegenden Frames gelesen, jeder Compositor-Frame-Index einmal (vr/OpenVRTypes.h
`CompositorFrameTiming`, render/HeadsetRenderer.cpp). Spalten: wie oft der Frame gezeigt wurde
(`num_presents`), ob auf einem anderen Vsync als vorhergesagt (`mis_presented`), wie oft der
vorige Frame zusätzlich gezeigt wurde (`dropped_frames`), die Reprojektions-Flags (0x04 steht auf
jedem Frame und heißt nur "async an"; 0x01 CPU, 0x02 GPU, 0x08 Motion Smoothing sind Gründe), die
GPU-Zeit der App vor dem zweiten Submit aus Sicht des Compositors (`pre_submit_gpu_ms` - die
einzige GPU-Zeit, die OBVR unter DXVK hat) und die Momente von WaitGetPoses, Posen und zweitem
Submit relativ zum Vsync des Frames (`wait_get_poses_called_ms`, `new_poses_ready_ms`,
`new_frame_ready_ms`; negativ = davor). `present_id` ist OBVRs Present beim Lesen, ein bis zwei
Frames nach dem beschriebenen. `tools/perf-phases.py` zeigt je Phase Drops / Fehl-Präsentationen /
Reprojektionen mit Grund, die App-GPU-Zeit und den Submit-Zeitpunkt relativ zum Vsync.

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

Nebenbefunde der Läufe: Oblivions Auto-Vanity-Kamera kreist nach einer Weile ohne Eingabe um
den Spieler; die Harness-Skripte setzten `SetNumericINISetting "fAutoVanityModeDelay:Camera"
100000` (xOBSE) - das ist Skyrims Name, Oblivions Setting heißt `fVanityModeAutoDelay`
(Konstruktor bei 0x009E7E7A, Code-Default 30), die Zeile bewirkte nichts. Seit 2026-10-18 hält
OBVR die Kamera selbst fern (`game/VanityCamera.h`: die Leerlauf-Uhr der Engine bei 0x00B3BB08
wird jeden Frame genullt, `[Look] AutoVanityCamera=0`; Harness `vanity-hold.txt` und die
Kontrolle `vanity-allowed.txt`). Das Bewegen
persistenter Referenzen per Konsole (`JauffreRef.moveto player 0 180 0`) brachte in
sieben Läufen niemanden ins Bild, `player.placeatme <Basis-ID> N` tut es; die Basis-IDs
stehen in den zlib-komprimierten NPC_-Records der esm (Jauffre 00023999, BrotherPiner
000223AD, PriorMaborel 000223AB, Eronor 000223AF, ImperialLegionSoldier 0018B118).


## Gemessen 2026-10-19: drei Klingen in der Gefängniszelle, Dual gegen aer (Harness, headless, 75 Hz)

Anlass: Nadi merkt den Drop im Start-Dungeon, "sobald die Truppe aus 3 Leuten vor mir steht".
Szenarien `tools/hand-scripts/perf-dungeon-dual.txt` und `perf-dungeon-aer.txt`: `coc
ImperialDungeon01` (die Zelle, in der das Spiel beginnt), 30 s leer, dann Baurus, Glenroy und
Captain Renault (`player.placeatme 00023F2A / 00023F2B / 0002349F`), die zum Spieler laufen; 8 s
später `tai`, damit sie stehen bleiben (im ersten Lauf mit KI wanderten sie, 27 bis 72 geskinnte
Draws je Pass). Danach stabil: 60 geskinnte Draws je Pass (20 je Klinge in Rüstung, ein Mönch der
Halle hatte etwa 10), 532 Draws je Pass gegen 240 leer.

| Phase, je 15 bis 30 s stehend | Dual: Frame-Mittel | über 20 ms | Leerlauf (poses_wait) | scene_pass je Auge | aer: Frame-Mittel | über 20 ms | Leerlauf |
|---|---:|---:|---:|---:|---:|---:|---:|
| Zelle leer | 13,40 ms | 0,5 % | 9,5 ms | 1,02 ms | 14,99 ms | 12,3 % | 13,2 ms (p95 21,2) |
| drei Klingen kommen (8 s) | 14,03 ms | 5,3 % | 2,9 ms | 4,43 ms | 13,44 ms | 0,7 % | 7,5 ms |
| drei Klingen stehen | 13,33 bis 13,36 ms | 0,0 % | 2,6 ms | 4,29 bis 4,30 ms | 13,42 bis 13,47 ms | 0,6 bis 0,9 % | 8,0 ms |
| SteamVR, ganzer Lauf | reprojiziert 5,6 % (3,1 % mit KI) | | | | 5,1 % (13,0 % im ersten aer-Lauf) | | |

Befund: drei Klingen kosten 3,3 ms je Augen-Pass, also 1,1 ms je Person und Auge - fast das
Doppelte eines Mönchs (0,6 ms), die Rüstung hat doppelt so viele Draws. Unter Dual Pass sind das
6,6 ms je Frame; auf diesem Rechner bei 75 Hz (13,3 ms) passt es noch, der Leerlauf fällt von 9,5
auf 2,6 ms, und nur in den Sekunden, in denen die drei herankommen, fallen 5 % der Frames auf den
nächsten Vsync. Bei 90 Hz (11,1 ms) wäre der Leerlauf 0,4 ms, bei 120 Hz weg - dort reicht die
Truppe allein. aer gibt dem Frame 5,4 ms Leerlauf zurück (8,0 statt 2,6 ms, den ganzen zweiten Pass
samt Nebenkosten), verliert aber trotzdem mehr Frames als Dual (0,6 bis 0,9 % gegen 0,0 %, und in
der leeren Zelle 12,3 % mit poses_wait-Spitzen von 21 ms - der Compositor hält den Frame eine
Periode zurück; Ursache offen, im ersten aer-Lauf stand der ganze Lauf bei 13 % Reprojektion). Für
Nadis Drop am Beyond (75 Hz) reicht die CPU-Seite dieser Szene nicht als Erklärung: die echte
Tutorial-Szene hat fünf Leute (Kaiser, drei Klingen, Valen Dreth) und läuft in Nadis Auflösung,
GPU ungemessen - eine Capture aus Nadis eigener Session (`[Performance] Enabled=1`, in der Zelle)
ist der nächste Schritt.

Werkzeug: die Mark-Zeile des Hand-Skripts trägt seit diesem Tag den Szenenaufruf und den
QPC-Tick (`HandScript: mark people (t=80.02 s, line 48, scene call 5791, tick 159837439985)`);
`tools/perf-phases.py` schneidet die Capture damit exakt und in jedem Stereo-Modus (vorher nur über
die "Dual draws at scene call"-Herzschlagzeilen, die es unter aer nicht gibt; Test
`tests/PerfPhasesTest.py`). Unter aer wird der Szenen-Render-Hook nicht installiert
(camera/CameraHook.cpp, nur für Stereo=dual), die Capture hat dort keine `scene_pass`-Spannen und
die Frames stehen auf Modus `unknown` - vergleichbar sind Frame-Abstand, Leerlauf und die
SteamVR-Summe.

### Nachtrag: die ganze Eröffnung, fünf Leute (2026-10-19, headless, 75 Hz)

Nadis Wunsch: die echte Szene im Harness. `perf-dungeon-five.txt` und `perf-dungeon-five-aer.txt`:
dieselbe Zelle, dazu Kaiser Uriel Septim (00023F2E) und Valen Dreth (00025200) - fünf Leute,
118 geskinnte Draws je Pass (Kaiser im Mantel und Valen zusammen 58), 748 Draws je Pass gegen 240 leer.

| Phase | Dual: Frame-Mittel | über 20 ms | Leerlauf | scene_pass je Auge | aer: Frame-Mittel | über 20 ms | Leerlauf |
|---|---:|---:|---:|---:|---:|---:|---:|
| Zelle leer | 13,38 ms | 0,4 % | 9,4 ms | 1,04 ms | 15,26 ms | 14,4 % | 13,5 ms (p95 21,2) |
| fünf kommen (8 s) | 26,01 ms | 94,8 % | 8,5 ms | 7,39 ms | 13,35 ms | 0,0 % | 5,3 ms |
| fünf stehen (60 s) | 26,71 bis 26,90 ms | 100 % | 9,1 ms | 7,60 bis 7,67 ms | 13,36 bis 13,48 ms | 0,2 bis 0,9 % | 7,0 bis 7,1 ms |
| SteamVR, ganzer Lauf | reprojiziert 32,3 % | | | | 4,4 % | | |

Befund: das ist Nadis Drop. Fünf Leute kosten 6,6 ms je Augen-Pass (1,3 ms je Person und Auge),
zwei Pässe sind 15,2 ms - 2 ms über der 75-Hz-Periode, und jeder Frame rutscht auf die nächste:
26,7 ms, 100 % der Frames, ein Drittel der Frames reprojiziert. Der "Leerlauf" von 9 ms ist dabei
das Warten auf den übernächsten Vsync, keine Luft. Unter aer läuft dieselbe Szene bei 13,4 ms mit
7 ms Luft und unter 1 % Drops; nur die leere Zelle zeigt wieder die aer-Aussetzer (14,4 %,
poses_wait-Spitzen 21 ms), wie in beiden Drei-Klingen-Läufen - sie treten auf, wenn der Frame
billig ist, nicht wenn er voll ist. Ursache offen, in der Spec als eigener Punkt.

### Nachtrag: die aer-Aussetzer in billigen Szenen (2026-10-19 abends, gelöst)

Die leere Zelle verlor unter aer in jedem Lauf Frames (12 bis 14 % headless, 50 % auf dem
Desktop, 28 % mit MonitorVsync=1), mit Leuten im Bild nicht. Der Compositor sagte es
(compositor.csv, dafür gebaut): SteamVR weckt die App 3,0 bis 3,3 ms vor einem Vsync
("Running Start"); der billige aer-Frame (1,8 ms Arbeit) wurde 1 bis 3 ms VOR dem Vsync
eingereicht und galt damit für den Vsync direkt vor ihm (der Niedriglatenz-Pfad), ein nach
dem Vsync eingereichter für den nächsten (die Ein-Frame-Pipeline). Ein Frame genau auf der
Kante kippt zwischen beiden, und jeder Kipp kostet einen Frame (`mis_presented` 1,
`dropped_frames` 1, `num_presents` 2 - 325 von 2005 Frames). Mit fünf Leuten lag der Submit
6 ms nach dem Vsync, unter Dual 0,5 ms danach: beide stabil.

Fix `[Render] SubmitPastVsync=1` (render/SubmitPacing.h, Standard an): ein vor dem Vsync
fertiger Frame wartet bis 0,5 ms danach; der Vsync ist der Moment, in dem WaitGetPoses
zurückkam, plus der Running Start, den der Compositor für den letzten fertigen Frame
meldet (`new_poses_ready_ms`). Zwei Quellen davor verworfen: das Record des laufenden
Frames (direkt nach WaitGetPoses oft noch das vorige, eine Periode zurück - kein einziger
Halt) und `GetFrameTimeRemaining` (liest im Posen-Moment etwa eine Periode; der
Compositor-Frame läuft von Running Start zu Running Start). Der Halt steht in der Capture
als `submit_hold`, die Vsync-Schätzung als `pacing_vsync`.

| leere Zelle, aer | vorher | mit SubmitPastVsync |
|---|---:|---:|
| Frame-Mittel | 15,0 bis 20,0 ms | 13,48 ms |
| über 20 ms | 12 bis 50 % | 1,1 % |
| Compositor: Frames doppelt gezeigt | 325 von 2005 | 23 von 2226 |
| Halt je Frame | - | 2,59 ms (mit Leuten 0,09 ms) |
| SteamVR, ganzer Lauf reprojiziert | 5,6 bis 13 % | 3,5 % |

Die Schätzung sitzt: Posen bis geschätzter Vsync 3,01 ms (p5 3,01, p95 3,02), der Submit
landet 0,50 ms nach dem geschätzten Vsync; der Compositor misst ihn 3,65 ms nach seinem
(p5 2,81) - die Differenz ist die Weckverzögerung zwischen Compositor und Spiel unter
Proton, sie schiebt den Submit weiter hinter den Vsync und schadet nicht.

Regression unter Dual mit der Taktung (gleicher Abend): leere Zelle 13,35 ms und 0,1 % über 20 ms
(vorher 0,4 bis 0,5 %, Halt 0,48 ms - der billige Dual-Frame lag selbst knapp an der Kante), drei
Klingen stehend 13,33 ms und 0,0 % (unverändert, kein Halt), fünf Leute 26,7 ms wie vorher (der
Frame ist zu teuer, keine Frage der Taktung); SteamVR ganzer Lauf 1,7 % reprojiziert (vorher 3,1
bis 5,6 %). In Menüs und Ladebildschirmen hält die Taktung jeden Frame (billige Frames), das
kostet dort bis zu 3,5 ms Latenz je Frame und nichts sonst.

### Nachtrag: die Aufruf-Uhr - wo die 8 ms eines Passes mit fünf Leuten liegen (2026-10-19)

`draw_calls`, `vs_constant_calls`, `state_calls` (Ereignisse je Pass, wie `bone_lock`): die Zeit
INNERHALB der gehookten D3D9-Aufrufe (InterfaceRenderHook.cpp, `CallClock`, nur während einer
Capture an; zwei Zählerstände je Aufruf, etwa 0,3 ms je Pass Messkosten). Nicht gehookt und damit im
Rest: SetTexture, SetSamplerState. `tools/perf-phases.py <Lauf> <Marken> --events draw_calls,...`.

| je Augen-Pass, Dual | Zelle leer | fünf Leute |
|---|---:|---:|
| scene_pass gesamt | 1,6 ms | 8,8 bis 8,9 ms |
| in Draw-Aufrufen (240 / 762 Draws) | 0,08 ms | 0,25 ms |
| in SetVertexShaderConstantF (1931 / 7611 Aufrufe) | 0,26 ms | 1,1 ms |
| in den übrigen gehookten Settern | 0,14 ms | 0,38 ms |
| Bone-Lock (OBVR) | 0,04 ms | 0,2 ms |
| **Rest: Engine zwischen den Aufrufen (+ SetTexture/Sampler)** | **1,1 ms** | **6,9 ms** |

Befund: DXVKs Anteil an einem Pass mit fünf Leuten ist rund 1,9 ms (ein Fünftel), ein Draw kostet in
DXVK 0,3 µs, ein Konstanten-Upload 0,15 µs. Vier Fünftel des Passes sind die Engine selbst zwischen
den Aufrufen: je Person und Pass etwa 1,2 ms Szenengraph, Skin-Paletten rechnen, Sichtbarkeit. Ein
Paletten-Cache für den zweiten Pass (Hebel 2 in der Spec, wie bisher gedacht) spart davon nur die
1,1 ms Uploads. Was den zweiten Pass wirklich entlasten würde, ist die Engine-Rechnung, die vom Auge
nicht abhängt - vor allem das Bauen der Knochenpaletten je geskinnter Shape (die Engine rechnet sie je
Pass neu, OBVR sieht nur den fertigen Upload). Nächster Schritt dafür: die Engine-Funktion finden, die
die Palette je Shape baut (Ghidra/REA), ihre Zeit je Pass messen (ein Hook, eine Uhr - klein bis
mittel), dann entscheiden, ob der zweite Pass das Ergebnis des ersten übernehmen kann (groß).

### Nachtrag: der Render-Sampler, und was die Leute wirklich kosten (2026-10-19 abends)

`[Performance] Sampler=1` (render/RenderSampler.h): ein Thread hält den Render-Thread etwa
jede Millisekunde an, liest EIP und die Rücksprungadressen in Oblivion.exe auf seinem Stack
(wie der Hang-Watchdog) und schreibt am Ende der Capture `OBVR-samples.csv` neben OBVR.log,
dazu `OBVR-samples-modules.csv` (welches Modul zu welcher 64-KiB-Seite gehört).
`tools/perf-samples.py <samples> <Lauf> <Marken> --in-pass only` zählt je Phase: wo der EIP
stand (exklusiv, nach Modul und 256-Byte-Bin), welche Engine-Frames auf dem Stack lagen
(inklusiv) und von welcher Engine-Stelle aus die EXE verlassen wurde. Szenario
`perf-dungeon-five-sampled.txt`.

Erster Lauf, fünf Leute, im Pass: 78 % des EIP außerhalb der EXE, davon 43 % in EINEM
256-Byte-Bin - OBVR.dll+F9200 = `HookedVbUnlock`, aufgerufen vom Unlock-Aufruf der Engine
(0x00776DC2). Dort las die Dual-Pass-Diagnostik (`SumLeadingBytes`, "Dual pool writes",
Vertex-Peeks) bei jedem Unlock 256 Bytes aus dem gemappten Puffer: Write-Combined-Speicher,
byteweise, 25 µs je Unlock, 135 Unlocks je Pass mit fünf Leuten (6 leer): 3,6 ms je Pass.
Seit diesem Abend opt-in: `[Debug] PoolWriteFingerprints=0`. Zweiter Lauf: 45 % in ntdll
und der vDSO, aus den gehookten D3D9-Aufrufen heraus - die Aufruf-Uhr vom Nachmittag selbst
(zwei Zählerstände je Aufruf, zwanzigtausend je Pass; ein Zählerstand ist unter Wine ein
ntdll-Aufruf). Seitdem `[Performance] CallClock=0` als Standard; die Buckets gibt es nur noch
auf Wunsch.

Ohne beides, Dual Pass, Eröffnung mit fünf Leuten (16:44): Frame 13,33 ms, 0,0 % über 20 ms,
scene_pass 2,85 ms je Auge (leer 0,72), Leerlauf 5,9 ms, SteamVR 2,1 % reprojiziert. Vorher:
26,7 ms, 100 %, 7,6 bis 8,9 ms, 31 %. Eine Person kostet die Engine also rund 0,43 ms je Pass,
nicht 1,1 bis 1,3. Das Rest-Profil ohne Instrumente: 74 % Engine (davon 22 % das memcpy der
geskinnten Vertizes in die dynamischen Puffer, 3,7 MB je Pass), 14 % DXVK, 4 % OBVR.

Lehren: gemappten Puffer-Speicher nie zurücklesen; `QueryPerformanceCounter` unter Wine ist
kein rdtsc; und ein Anteil, der "nicht spürbar" sein soll, wird gemessen - der Sampler ist
das Werkzeug dafür, die Uhren sind es nicht, wenn sie selbst zehntausendmal je Pass laufen.
Zahlen aus Captures vor diesem Abend (perf-people-hall, perf-jauffre, die Klingen-Läufe)
tragen die Fingerprint-Kosten mit; ihre Verhältnisse gelten, ihre Absolutwerte nicht.
Jauffres Haus neu gemessen (16:53, ohne Instrumente): 13,35 ms, 0,1 % über 20 ms, scene_pass
3,5 ms je Pass statt 7 bis 8 - der "leere" Innenraum bei zwei Vsync war derselbe Befund (95 geskinnte
Draws der Hausbewohner hinter dem Spieler, ebenso viele Unlocks).

### Nachtrag: das Profil nach Funktionen, und was die Engine mit den Vertizes tut (2026-10-23)

Werkzeug: `tools/perf-functions.py <samples> <Lauf> <Marken> --in-pass only` ordnet die
Sampler-Zeilen den FUNKTIONEN von Oblivion.exe zu (Funktionsanfänge aus Ghidra 12.1.4 per
REA exportiert, `tools/oblivion-1.2.0.416-functions.tsv.gz`, 21240 Funktionen; eine Funktion
reicht bis zum nächsten Anfang - Ghidra hat einige Anfänge nicht, 0x0076E430/4B0/610 liegen
in "FUN_0076e320"). Je Phase: exklusiv nach Funktion mit den drei häufigsten Aufrufern der zehn
heißesten, inklusiv nach Funktion auf dem Stack, und Bereiche (memcpy, Allocator, Critical
Sections, Stream-Packen, Module). Der Lauf vom 2026-10-19 und der erste vom 2026-10-23 (14:50)
hatten das Record-Limit der Capture (143395 Events) eine Sekunde nach people-15 erreicht -
die Sampler-Zeilen deckten eine Sekunde der Leute ab, nicht 45. Szenario
`perf-dungeon-five-functions.txt` legt die Capture ab 100 s über die Leute allein (45 s).
Seitdem kopiert der Harness `OBVR-samples*.csv` in den Lauf-Ordner.

Lauf 14:59, Dual, fünf Leute, people-15 bis people-watched, 11396 Samples im Pass:

| Bereich (exklusiv) | Anteil | Wer |
|---|---:|---|
| memcpy (CRT, 0x009812C0) | 24,0 % | 79 % aus der indizierten Stream-Kopie 0x0076E610 (Rücksprung 0x0076E6B1), 19 % aus dem Unlock 0x00776D80 (Staging in den gemappten Puffer) |
| die Kopierschleifen selbst (0x0076E610, Bins 0x0076E690..6C0 und 7A0) | 7,9 % | aus den Copier-Loops 0x0076F590 |
| 0x0076F3B0 (Copier-Teil, memset-Schleife) | 2,5 % | aus 0x0076F590 |
| DXVK (D3D9.DLL) | 13,1 % | aus 0x00718A80, dem Unlock, 0x007C9140 |
| OBVR.dll | 4,6 % | Bone-Lock, Culling-Sync, Hooks |
| 0x0047DA70 (Ebene gegen Kugel, Culling) | 3,8 % | aus 0x007D2EA0 |
| Rest Engine (Shape-Traversal, Shader-Setup, Konstanten) | ~40 % | 0x0077A1F0 (49 % inklusiv), 0x00767520, 0x007693E0 |
| ntdll + kernel32 | 4 % | Locks (0x006FF9C0), Dateizugriffe |

Das Stream-Packen (Vertizes der geskinnten Körper in die dynamischen Puffer) ist damit rund
35 % eines Passes mit fünf Leuten, und der größte Einzelposten ist eine Schleife, die je Vertex
ein memcpy mit 8 oder 12 Bytes AUFRUFT (0x0076E690: Index aus der Partition lesen, Quelle
berechnen, `call memcpy`). Allocator und Critical Sections tauchen im Pass nicht auf (unter
0,5 %) - ein Heap-Ersatz à la Stutter Remover hätte hier nichts zu holen.

Gebaut: `render/StreamCopyLogic.h` + `render/StreamCopy.cpp` (`[Render] FastStreamCopy=1`):
Detour auf 0x0076E610, dieselbe Semantik (Zero-Fill, indiziert, gerade, Rückgabe der
gezählten Bytes), Elementgröße einmal entschieden (4/8/12/16 inline, Rest memcpy).
Unit-Test `stream_copy_test` vergleicht gegen die nachgebaute Engine-Schleife. Im Spiel:
`[Debug] StreamCopyCheck=1` lässt jede Kopie erst die Engine machen, kopiert das Ergebnis
weg, macht sie dann schnell und vergleicht. Lauf 15:05 (`perf-dungeon-five-copycheck.txt`):
202000 Kopien verglichen, 0 Abweichungen, 0 zu groß. Die Zeit dieses Laufs zählt nicht
(jede Kopie doppelt, scene_pass 5,8 ms).

Frame-Uhr (Lauf 14:59, `[Debug] FrameClockTrace=1`, 75 Hz): die Engine las je Frame 13,0 ms
(458 von 732) oder 14,0 ms (230), dazu einzelne 12/15/23/26; die präzise Uhr gab 13,33 ms im
Mittel. GetTickCount ist unter Wine millisekundengenau, die Quantisierung auf ganze
Millisekunden bleibt. Was Windows ohne erhöhte Timer-Auflösung tut (16-ms-Schritte), ist
hier NICHT gemessen - der Trace zeigt es in Nadis Windows-Log.

Läufe mit der schnellen Kopie (alle Dual, fünf Leute, Sampler an, people-15 bis people-watched):

| Lauf | Bedingung | scene_pass Mittel | Unlock-memcpy (Samples) | StreamCopyFast | Leerlauf (poses_wait) |
|---|---|---:|---:|---:|---:|
| 14:59 | Engine-Kopie | 3,22 ms | 527 (0,15 ms je Pass) | - | 4,2 ms |
| 15:09 | FastStreamCopy | **3,68 ms** | **4276 (1,2 ms je Pass)** | 1376 (10 %) | 3,4 ms |
| 15:19 | FastStreamCopy + `DXVK_CONFIG=d3d9.cachedWriteOnlyBuffers = True` | **2,13 ms** | 23 | 1050 (13 %) | 6,5 ms |

Die schnelle Kopie nimmt dem Pass die 19 % Pro-Vertex-memcpy und die 8 % Schleife und kostet
selbst 10 %. Dass Lauf 15:09 trotzdem LANGSAMER war, lag an einer anderen Stelle: das memcpy
beim Unlock (0x00776D8F, Staging in den gemappten Puffer, `rep movsd` bei 0x0098131A in beiden
Läufen) brauchte achtmal so lange wie um 14:59. Lesart: DXVK legt WRITEONLY-Dynamic-Puffer in
Device-Local-Host-Visible-Speicher (VRAM über die BAR; RTX 4090, BAR1 32 GB), und das memcpy
schreibt dann mit PCIe-Rate (~3 GB/s, 3,7 MB je Pass = 1,2 ms); um 14:59 muss derselbe Puffer im
Systemspeicher gelegen haben (~25 GB/s). Welche Zuteilung ein Lauf bekommt, wechselte von Start zu
Start (5 gegen 7 Puffer-Erzeugungen) - mit DXVK-Logs NICHT belegt, nur aus den Raten gelesen.
Mit `d3d9.cachedWriteOnlyBuffers = True` (Deepwiki doitsujin/dxvk: HOST_CACHED statt DEVICE_LOCAL
für WRITEONLY-Puffer; die Proton-CachyOS-DXVK v3.1.1 kennt den Schlüssel) verschwindet die Kopie
(23 Samples), der Pass fällt von 3,22 auf 2,13 ms - eine Person kostet damit rund 0,28 ms je Pass
statt 0,43. Die Zeile steht seit 15:25 in der Live-dxvk.conf und in README/Nexus.
Lauf 6 (15:32, die Zeile in der dxvk.conf, keine Umgebungsvariable; DXVK liest sie, Lauf
`dxvk-config-check.txt` mit `DXVK_LOG_LEVEL=info`: "Found config file: dxvk.conf ...
d3d9.cachedWriteOnlyBuffers = True"): scene_pass **2,57 ms**, Unlock-memcpy 776 Samples
(0,2 ms je Pass). Die Unlock-FUNKTION (0x00776D80 inklusiv, memcpy plus DXVKs Unlock) kostet in
den Läufen 14:59 / 15:19 / 15:32 ähnlich viel (695 / 552 / 824 Samples, 0,15 bis 0,2 ms je
Pass) - nur die Aufteilung memcpy gegen DXVK-Unlock wechselt; der Ausreißer ist allein Lauf
15:09 (4421, VRAM über PCIe). Die Option schützt also vor dem 1,2-ms-Fall, mehr nicht. Fairer
Vergleich der schnellen Kopie: 2,57 gegen 3,22 ms (-20 %); die 2,13 ms von 15:19 liegen
zusätzlich tiefer (7796 gegen 9341 Samples im Pass, der ganze Render-Walk kürzer) - Lauf-zu-Lauf-
Streuung der Szene, nicht erklärt. Offen und für die Personenkosten wichtig: die Roster-Zeile
der parallelen Session (Lauf 15:32) zählt jeden der fünf Platzierten ZWEIMAL (00025200 x2,
00023F2E x2, ...) - die Szene hat zehn Platzierte, nicht fünf; "0,28 ms je Person" ist damit eher
0,14. Zu klären, ob `console-at people` doppelt feuert oder der Harness-Save die fünf schon trägt.

**Standalone-Plugin "Oblivion Frame Fixes"** (Repo /run/media/nadi/Games/Modding/OblivionFrameFixes, noch
ohne Remote; dieselben Logik-Header, Hülle nach FoliageAA-Muster) neben OBVR.dll im Linux-Spiel, Szenario
`../OblivionFrameFixes/tools/hand-scripts/frame-fixes-check.txt` (die Fünf-Leute-Zelle, Erwartung: OBVR
zieht an beiden Stellen zurück). Lauf 15:57, StreamCopyCheck=1: PASS, OBVR-Log "another plugin's jump to
79B01610 already sits there, inside OblivionFrameFixes.dll" an beiden Einträgen; Plugin-Log 522000 Kopien
verglichen, 0 Abweichungen (Zeit in diesem Modus bedeutungslos: Pass 6,3 ms, 29 % reprojiziert). Lauf
16:02 ohne Check: PASS, scene_pass **2,37 ms**, die Plugin-Kopie 11,8 % des Passes (OBVRs eigene 10 bis
13 %), SteamVR 1,2 % reprojiziert, Uhr-Trace 13/14 ms Engine gegen 13,333 gegeben. Danach wieder
deinstalliert; im Spiel liegt nur OBVR.
