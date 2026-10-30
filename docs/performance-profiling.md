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
| 0x0047DA70 (NiBound::WhichSide: Kugel gegen Ebene, Culling; OBNG 2026-10-30) | 3,8 % | aus 0x007D2EA0 |
| Rest Engine (Shape-Traversal, Shader-Setup, Konstanten) | ~40 % | 0x0077A1F0 = NiD3DShader::SetupShaderPrograms (49 % inklusiv, pro Skin-Partition und Pass; OBNG 2026-10-30); darin bis zur Korrektur vom selben Tag auch PrepareGeometryForRendering 0x0077A310 (Packen + Binden der Streams), dem die Funktionstabelle den Start fehlte - perf-functions.py trennt sie seitdem (EXTRA_STARTS), 0x00767520, 0x007693E0 |
| ntdll + kernel32 | 4 % | Locks (0x006FF9C0 = NiObjectNET::GetExtraData, Critical Section 0x00B3F600; OBNG 2026-10-30), Dateizugriffe |

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
Mittel. Achtung (gesehen 16:35): im Test-Spiel ist oblivion_display_tweaks.dll mit iTickFix=1
geladen - der Tick, den die Engine in allen Läufen dieses Tages bekam, war ODTs QPC-basierter
Ersatz, ganze Millisekunden. Die 13/14 zeigen also: ODTs Tick-Fix nimmt die ms-Quantisierung
NICHT weg, unsere Uhr darunter schon; was nacktes GetTickCount unter Wine oder Windows tut, ist hier
NICHT gemessen - der Trace zeigt es in einem Spiel ohne ODT, oder in Nadis Windows-Log.

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
Streuung der Szene, nicht erklärt. Die Roster-Zeile der parallelen Session (Lauf 15:32) zählt
jeden der fünf Platzierten ZWEIMAL - aufgeklärt 16:10: schon VOR dem Platzieren (Marke prison) stehen
die fünf Basisformen je x1 im Roster, die zelleigenen Exemplare der Eröffnungs-Besetzung, mit der Zelle
geladen, der nächste Actor 818 Einheiten entfernt; nach dem Platzieren x2, der nächste 33 Einheiten.
Der Bone-Lock ersetzt in der leeren Phase 520 Paletten-Zeilen je Scene-Call, mit den fünf 2136 - im Bild
sind die fünf Platzierten, die zelleigenen stehen außer Sicht. Die Personenkosten (Leute minus leer,
durch fünf) stehen.

**Standalone-Plugin "Oblivion Frame Fixes"** (Repo /run/media/nadi/Games/Modding/OblivionFrameFixes, noch
ohne Remote; dieselben Logik-Header, Hülle nach FoliageAA-Muster) neben OBVR.dll im Linux-Spiel, Szenario
`../OblivionFrameFixes/tools/hand-scripts/frame-fixes-check.txt` (die Fünf-Leute-Zelle, Erwartung: OBVR
zieht an beiden Stellen zurück). Lauf 15:57, StreamCopyCheck=1: PASS, OBVR-Log "another plugin's jump to
79B01610 already sits there, inside OblivionFrameFixes.dll" an beiden Einträgen; Plugin-Log 522000 Kopien
verglichen, 0 Abweichungen (Zeit in diesem Modus bedeutungslos: Pass 6,3 ms, 29 % reprojiziert). Lauf
16:02 ohne Check: PASS, scene_pass **2,37 ms**, die Plugin-Kopie 11,8 % des Passes (OBVRs eigene 10 bis
13 %), SteamVR 1,2 % reprojiziert, Uhr-Trace 13/14 ms Engine gegen 13,333 gegeben. Danach wieder
deinstalliert; im Spiel liegt nur OBVR.

### Nachtrag: ein Welt-Render je Frame - der flache Gewinn (2026-10-23, 16:24 und 16:28)

Frage: was bringt die schnelle Kopie einem Monitor-Spieler, der die Welt einmal je Frame zeichnet?
`[Render] Enabled=0` (OBVR als Hintergrund-App) wäre der echte flache Modus, stoppt aber HandScript
und Sampler mit dem Headset-Pfad (Lauf 16:17: "the script did not start", Hauptmenü bei 144 fps) -
nicht harness-fähig. Statt dessen `Render.Stereo=none`: die Engine zeichnet die Welt EINMAL, OBVR
reicht das Bild an beide Augen; der Sampler läuft seit diesem Nachmittag in jedem Stereo-Modus
(StartRenderSampler aus dem Dual-Block gelöst, NoteRenderThread in OnFrameEnd; in_pass bleibt 0).
Szenarien `perf-dungeon-five-mono-enginecopy.txt` / `-fastcopy.txt`, fünf Leute, 75 Hz, Samples über
den ganzen Frame (people-15 bis people-watched, je ~23970 Samples; Wartezeit in vrclient = Leerlauf):

| Mono, ein Welt-Render je Frame | Engine-Kopie | schnelle Kopie |
|---|---:|---:|
| beschäftigt (nicht in vrclient) | 40,7 % = **5,43 ms/Frame** | 37,9 % = **5,06 ms/Frame** |
| memcpy (CRT) | 6,8 % = 0,91 ms | 1,9 % = 0,26 ms (nur noch Unlock) |
| die Kopierschleifen 0x0076E610 | 2,1 % = 0,28 ms | 0 |
| OBVR.dll (mit StreamCopyFast) | 1,4 % | 4,7 % (die Kopie ~0,44 ms) |
| poses_wait (Mittel) | 8,19 ms | 8,52 ms |

Der Kopierpfad fällt von ~1,2 auf ~0,7 ms je Frame, der Frame insgesamt um 0,37 ms (7 % der
Arbeit) - mit fünf Leuten im Bild. Für einen flachen Spieler bei 60 Hz sind das 2 % des Budgets, bei
144 Hz 5 %; mit mehr Leuten entsprechend mehr. Die 0,37 ms sind der Frame-Mittelwert zweier Läufe,
die Streuung zwischen Läufen liegt in derselben Größenordnung wie die Differenz der übrigen Posten.
Nebenbefund: Mono reprojiziert 10 bis 11 % (poses_wait p99 17 ms) - das Pacing-Thema von aer
(SubmitPastVsync) auch hier, nicht untersucht. Zweiter Nebenbefund: im Test-Spiel ist
oblivion_display_tweaks.dll geladen (ein Sample im Profil) - alle Läufe dieses Tages liefen mit
Display Tweaks neben der präzisen Uhr, ohne Konflikt.

### Nachtrag: Hebel 2 im Spiel (2026-10-23 abends)

Spec §5. Läufe mit `perf-dungeon-five-reuse-check.txt` (Check-Modus: der zweite Pass packt
trotzdem, Element-Hashes aller Kopien gegen den ersten Pass) und `-reuse.txt` (scharf):

| Lauf | Fassung | skipped | Befund | scene_pass |
|---|---|---:|---|---:|
| 17:00 check | nur volatile, Span-Hash am Unlock | 49 % | 16 % "Mismatches" = Müll in den Lücken des Staging-Puffers | 5,8 (Check) |
| 17:00 scharf | nur volatile | 79 % | obere Schranke, "falsche" Records mit übersprungen | 2,38 |
| 17:26 scharf | Record-Trust mit Span-Stichprobe | 0 % | jede Verifikation "mismatch" (Lücken-Müll), nichts übersprungen | 2,75 |
| 17:43 check | Element-Hashes, alle Pakete | (40 %) | **98539 verglichen, 0 Mismatches**; 24 falsche "eye-dependent" (statische Records, zweiter Pass kopiert nichts: 6/0 Hashes) | 5,9 (Check) |
| 17:47 scharf | Element-Hashes, alle Pakete | **94 %** | **kein Gewinn: 2,82 ms**, StreamCopyFast-Samples wie ohne Reuse (1078 gegen 1050), Unlock-memcpy ebenso | 2,82 |

| 17:58 scharf | Element-Hashes, Kopie-Zähler je Pass | 92 % | Kopien im umgeleiteten Packer im zweiten Pass 20,5 GB → 0,04 GB über den Lauf; AUSSERHALB des Packers (0x00771790/0x00773330 → 0x0076FE20) 7,6 bzw. 9,2 GB je Pass unberührt; beschäftigt 7,31 ms je Frame gegen 7,23 bis 7,64 ohne Reuse | 2,36 |

Fazit: Hebel 2 tut, was er soll (0 Abweichungen, 92 % Skips, die Packer-Kopien des zweiten Passes
weg), und der Frame wird davon nicht messbar schneller - nach der schnellen Kopie kosten diese
Kopien nur noch ~0,14 ms je Frame, und ein zweiter Eingang in den Packer trägt weitere 0,5 MB je
Pass am Detour vorbei. Das Urteil vom 20.10. steht; Standard aus, Code bleibt als Opt-in.

Lehren: (1) Der Staging-Puffer der Engine behält zwischen den Elementen eines verschränkten
Vertex, was frühere Pakete hinterließen - ein Hash über den geschriebenen Bereich vergleicht
Müll; nur die kopierten Elemente hashen. (2) Je Pass werden nur ~23 Pakete notiert (die Puffer
sperren), fast alle Typ 0x8000 (volatile); die 3,7 MB Kopien je Pass schrumpften trotz 94 %
Skips nicht - welche Pakete die Bytes tragen, zeigt der nächste Lauf mit den Kopie-Zählern je
Pass. (3) 4-MB-Ringe wrappen 0,66 und 0,37 mal je Frame (zwei Ringe), auch mit skippendem
zweitem Pass. Standard bleibt aus.

### Nachtrag: Jauffres Haus mit dem Stand des Tages (2026-10-23, 20:01)

Nadi konnte nicht ins Headset ("kannst du das mit harness machen"); die zweite Szene als
Gegenprobe: `perf-jauffre.txt`, sieben Leute im Haus, Dual, 75 Hz, Build 388bdb55 (3581189 + Grip-Trace
der OBVR-Session), dxvk.conf mit cachedWriteOnlyBuffers, Live-INI mit Sampler=1 und FrameClockTrace=1.

| Jauffres Haus, sieben Leute | 19.10. 16:53 (ohne Instrumente) | 23.10. 20:01 (heutiger Stand) |
|---|---:|---:|
| scene_pass Mittel | 3,52 ms | **3,15 ms** (-10 %) |
| poses_wait (Leerlauf) | 4,79 ms | 5,63 ms |
| lange Frames im Fenster | 0,1 % | 0,1 % |
| Engine-Uhr | - | 13/14 ms, gegeben 13,333 |

Der Gewinn ist kleiner als in der Zelle (-20 bis -26 %): das Haus hängt weniger an den
geskinnten Kopien, mehr an Lichtern und Draws. Richtung und Größenordnung passen.

### Nachtrag: Halle und Spaziergang als A/B der schnellen Kopie, und ein Pacing-Befund (2026-10-23, 20:10 bis 20:25)

Nadi: "testen wir in anderen harness auch" - dann "dauert zu lange, testen wir später"; die vier Läufe
liefen schon. `perf-people-hall(-enginecopy).txt` (sieben Leute in der Halle des Harness-Saves) und
`perf-walk(-enginecopy).txt` (der Spaziergang; beide walk-Läufe FAIL, "Performance: export" fehlt -
das Szenario vom 14.10. passt nicht mehr zum Save, nicht untersucht). Die Halle:

| Halle, sieben Leute | Engine-Kopie | schnelle Kopie |
|---|---:|---:|
| scene_pass Mittel | 1,58 ms | **1,29 ms** (-18 %) |
| Frames > 20 ms im Fenster | **9,1 %** | **25,8 %** |
| SteamVR reprojiziert (ganzer Lauf) | 11,8 % | 24,6 % |
| Compositor: mis-presented / dropped | 498 / 450 | 1270 / 1267 |
| App-GPU-Zeit p50 / p95 | 4,6 / 5,1 ms | 2,4 / 4,3 ms |
| submit_hold (SubmitPastVsync) | 71 Frames, 4,3 ms | 2150 Frames, 0,36 ms |

Die Kopie spart in der Halle 18 % des Passes, und die Halle verliert trotzdem jeden vierten Frame -
mehr, je billiger der Frame. Das ist das Muster des aer-Befunds vom 19.10. (mis-presented und
dropped im Paar, Flags 4 = nur "async an", GPU-Zeit weit unter der Periode): der Compositor kippt
zwischen seinen zwei Pipelines. Der SubmitPastVsync-Halt deckt nur Frames ab, die VOR dem Vsync fertig
sind (die leere Zelle, 0,1 %); die Halle landet mit 3 bis 5 ms Arbeit nach dem Vsync offenbar in einem
zweiten schlechten Fenster, die Zelle mit 7 bis 8 ms nicht. Die Halle hatte das am 18.10. auch (18 %
reprojiziert, damals mit Fingerprints). **Offen, nächster Schritt:** aus compositor.csv je Frame die
Submit-Lage relativ zum Vsync (`wait_get_poses_called_ms`, `new_frame_ready_ms`) gegen
`mis_presented` legen und das zweite Fenster bestimmen; dann den Halt darauf ausdehnen. Das ist
vermutlich der größte Performance-Posten, der heute noch offen ist - er trifft genau die billigen
Szenen, die nach den Kopie-Gewinnen häufiger werden.

**Nachtrag 20:45, compositor.csv der Halle gelesen:** die verlorenen Frames kommen in Strecken von 228,
231 und 784 Frames hintereinander, in denen jeder Frame zweimal gezeigt wird (num_presents 2,
frame_index in Zweierschritten, Posen bei +0,1 ms statt -3,0, Compositor-Update bei +13,4 statt +10,2,
vsyncs_to_first_view 2) - SteamVRs **Interleaved Reprojection**, der Halbrate-Rückfall (Valve-Doku:
"too close to using up the available frame time"; "throttled after 4 dropped frames out of the past
10"). Jede Strecke beginnt direkt nach einem GPU-Spike von 14 bis 47 ms (total_render_gpu_ms; 13 Spikes
in 75 s, die ersten fünf beim Erscheinen der sieben Leute) und hält 3 bis 10 s, obwohl die Frames
3,5 ms Arbeit haben. Der Dual Pass in der Zelle (7-8 ms Arbeit, keine Spikes) gerät nie hinein. Kein
Submit-Fenster also, sondern GPU-Hitches plus SteamVRs Hysterese. Offen: Ursache der Spikes (DXVK-
Pipeline-Kompilierung oder Textur-Upload beim ersten Draw einer Person - `DXVK_HUD=compiler`), die
Austrittsbedingung der Halbrate und ob OBVRs Takt sie verlängert. `allowInterleavedReprojection` gibt es
in SteamVRs default.vrsettings nicht mehr; `enableLinuxVulkanAsync` steht dort auf false, die Flags der
Läufe zeigen trotzdem 0x04 (async an). Nadis zweite Session heute Abend (11,9 % reprojiziert, kurz, mit
Laden) passt zum selben Muster.

### Nachtrag: die Halle im eingeschwungenen Zustand (2026-10-23, 21:11)

Gegenprobe zum Halbrate-Befund: `perf-people-hall-steady.txt` - dieselbe Halle mit sieben Leuten,
aber die Capture erst ab 100 s (StartAfterSeconds=100, 60 s lang); die Leute stehen dann seit über
einer Minute. PASS, Build 388bdb55 wie die Läufe davor.

| Halle, sieben Leute | 20:10-20:25 (Capture über das Erscheinen) | 21:11 (eingeschwungen) |
|---|---:|---:|
| SteamVR reprojiziert (ganzer Lauf) | 11,8 bis 24,6 % | **0,8 %** (127 von 15185) |
| mis-presented im Fenster | 498 bis 1270, in Strecken von 228-784 Frames | 19 von 4447 (0,4 %), Strecken von höchstens 2 |
| GPU-Spikes > 12 ms (total_render_gpu_ms) | 13 in 75 s, die ersten fünf beim Erscheinen | 1 in 60 s (21,0 ms) |
| total_render_gpu_ms p50 / p95 / p99 | - | 6,11 / 7,31 / 8,15 ms |
| Frames > 20 ms im Fenster | 9,1 bis 25,8 % | 0,6 % |
| scene_pass Mittel | 1,29 bis 1,58 ms | 1,65 ms |

Die Spikes und die Halbrate-Strecken gehören zum ersten Zeichnen der Leute, nicht zur Halle. Ob das
Erstzeichnen-Kosten sind (Texturen hochladen, Pipelines einmal bauen), die ein zweites Erscheinen
nicht wiederholt, oder ob jedes Erscheinen sie kostet, soll `perf-jauffre-reappear.txt` sagen:
sieben Leute erscheinen bei 64 s, verschwinden bei 100 s und erscheinen bei 118 s wieder, eine
Capture von 60 bis 150 s; Spikes und die neuen Compositor-Logzeilen bei +4 s gegen +58 s lesen.
Geschrieben, nicht gelaufen - Nadi: "wir stoppen hier für heute abend".

Gebaut dazu `render/HalfRateWatch.h` (rein, half_rate_watch-Test, 153 Tests grün): OBVR liest die
Frame-Timing-Zeile des Compositors ohnehin für SubmitPastVsync; nach zehn doppelt gezeigten Frames
in Folge steht "Compositor: SteamVR shows every frame twice since its frame N - interleaved
reprojection, the half-rate fallback ..." im Log, nach zehn einfach gezeigten "Compositor: back to
full rate after N frames at half rate". Per Unit-Test geprüft, im Spiel noch nicht gesehen: der
Deploy traf ein laufendes Spiel einer anderen Session (nichts deployt), dann war der Abend zu Ende.

Achtung für alle Vergleiche ab hier: um 21:20 hat die Session obvr-6f auf Nadis Wunsch die
Linux-Installation geändert (ihr Bericht: Plugins.txt mit Ragdolls.esp samt 47 ersetzten Skeletten,
Maskar's Oblivion Overhaul, Enhanced Grabbing, Ascension, My Voice Extender; Sicherung
`Plugins.txt.bak-2026-10-23-before-ragdolls` daneben). Frame-Zeiten davor und danach sind nicht
vergleichbar; der Reappear-Lauf vergleicht nur innerhalb seiner eigenen Capture und bleibt gültig.

### Nadis Windows-Sessions (2026-10-23 abends und 2026-10-24 morgens; gelesen 2026-10-24)

Zwei Headset-Sessions auf Windows mit der Live-INI (Performance.Enabled=1 abends, FrameClockTrace=1,
Sampler=1): abends 21:5x bis 22:02 mit der ersten echten Capture (OBVR-performance-1, 87 s, 4821
Compositor-Frames, 2844 davon world_dual, 1838 live_menu), morgens 06:09 bis 06:46 nur mit der
Frame-Uhr-Spur (Performance.Enabled stand ab 06:15 auf 0). Rechner: RTX 4090 (DXVK wählt sie, nicht
die Radeon 610M), Beyond mit 90 Hz, SteamVR-Ziel 4028x3380 je Auge bei resolutionScale 100,
dxvk-gplasync 3.1.1, Oblivion.ini iMultiSample=8, FoliageAA Supersampling mit 8 Pässen je Blatt-Draw,
SteamVR motionSmoothingOverride=1 für die App. Kein Absturz (CobbCrashLogger leer), keine Fehlerzeile.

| Nadis Windows-Capture, 87 s | Welt (world_dual) | Menü mit Welt (live_menu) |
|---|---:|---:|
| App-GPU je Frame (pre_submit_gpu_ms) p50 / p95 | 8,26 / 13,79 ms | 6,43 / 11,94 ms |
| Frames über der 11,1-ms-Periode | 32,3 % | 22,9 % |
| zweimal gezeigt (num_presents >= 2) | 64,3 % | 56,5 % |
| scene_pass CPU je Pass p50 | 1,89 ms | - |
| poses_wait / submit_hold p50 | 6,8 / 6,9 ms | - |
| Frame-Abstand (frames.csv) p50, Anteil > 20 ms | 20,1 ms, 50 % | - |

Die Reprojection-Flags lesen sich mit dem openvr.h von ValveSoftware/openvr (Compositor_FrameTiming:
0x04 async, 0x08 Motion Smoothing in diesem Frame ausgelöst, 0xF0 Vorhersage-Frames, 0xF00
gedrosselte Frames): 0x11C auf 2530 Frames = ein Frame gedrosselt + ein Frame voraus + Motion
Smoothing + async, 0x1C auf 1951 = nicht gedrosselt; 0x22C auf 21 = zwei Frames gedrosselt. 54 % der
Frames gedrosselt, 91 Strecken mit mindestens zehn doppelt gezeigten Frames (längste 181). Das ist der
Halbrate-Rückfall aus der Halle, hier mit synthetisierten Frames (Motion Smoothing erzwungen) und mit
echter Ursache: die GPU liegt über dem Budget, die CPU hat 7 ms Luft.

Die Frame-Uhr-Spur (eine Zeile je 10 s) über beide Sessions: 209 von 217 Gameplay-Fenstern zwischen
45 und 60 fps, Mittel 58 fps, eines bei 90; die Engine-Millisekunden zu 37 % 22, zu 23 % 13, zu 11 %
23, zu 11 % 14 - die Engine sieht 1-ms-Schritte (Display Tweaks ist auf Windows geladen), keine
16-ms-Schritte; nacktes GetTickCount bleibt ungemessen. Die erste Zeile jeder Session zählt 2,7 bis
8,9 Millionen Aufrufe in 10 s: der Ladebildschirm ruft die Zeit-Aktualisierung in einer engen
Schleife, harmlos.

Hebel, nach Kosten: iMultiSample 8 -> 4 und FoliageAA Passes 8 -> 4 (billig, zwei INI-Zeilen; die
beiden Multiplikatoren der Kosten je Pixel, Anteil ungemessen - eine Capture mit beiden gesenkt
klärt es), resolutionScale unter 100 für die App (billig), 75 Hz statt 90 auf Windows wie auf Linux
(billig, 13,3 ms Budget). Nebenbefund: ~27000 Spurzeilen in 37 Minuten (Menu trace 7924, Hud aim
7606, Hud pass trace 3810, Contact 3710 mit ContactTrace=4000 in der Live-INI, Hud between renders
3701) - Instrumente anderer Sessions, deren Kosten ungemessen sind.

### Nachtrag: Jauffres Haus, die Leute zweimal (2026-10-24, 08:45)

`perf-jauffre-reappear.txt`, headless, PASS. Die Capture ging verloren: der Export scheiterte an
einem liegengebliebenen `OBVR-performance-1` im Linux-Spielordner (Nadis Live-Session 21:22 vom
Vorabend mit Performance.Enabled=1 in der Live-INI; der Runner räumt nur eigene Läufe weg;
jetzt unter `artifacts/hand-script/nadi-linux-live/`), und sie war ohnehin bei 119 s voll
(truncated_capacity: 143395 Records aus 32 MiB sind bei dieser Ereignisrate etwa 60 s, die 90 s
passten nie, das zweite Erscheinen bei 134 s lag außerhalb). Gelesen wurde stattdessen die
Frame-Clock-Spur (eine Zeile je 10 s mit Histogramm der Engine-Millisekunden) und die neuen
HalfRateWatch-Zeilen.

| 10-s-Fenster | Frames | Mittel ms | Max ms | Frames >= 16 ms |
|---|---:|---:|---:|---:|
| 80-90 s (Leute erscheinen bei 80,02, stehen bei 88) | 751 | 13,333 | 14,4 | 0 |
| 90-100 s | 750 | 13,334 | 19,5 | 2 (20, 18) |
| 100-110 s | 750 | 13,334 | 14,6 | 0 |
| 110-120 s (Leute weg bei 116) | 750 | 13,351 | 20,0 | 3 |
| 130-140 s (Leute wieder da bei 134) | 750 | 13,333 | 17,2 | 1 |
| 140-150 s (stehen bei 142) | 750 | 13,335 | 17,9 | 1 |
| 150-170 s | 751/751 | 13,333 | 16,0 / 15,4 | 1 / 0 |

Halbrate-Episoden: drei (22, 287, 166 Frames), alle beim Laden und Platzieren vor 50 s; in den 150 s
danach keine. Gegen die Halle von 21:11 ist der ganze Lauf sauberer: dort hielt jedes 10-s-Fenster
5 bis 10 Frames von 23-27 ms (ein verlorener Frame samt Aufholer), 28 Frames über 20 ms in den 60 s
der Capture, Abstände 102 bis 987 Frames (nicht die 120 des INI-Reloads), auch im leeren Haus;
hier in 120 s Leute und Wiedererscheinen kein Frame ab 23 ms.

Was NICHT der Unterschied ist: das Antialiasing. Beide Läufe melden "texture path, 0 sample(s)" -
das iMultiSample=4 des Morgens in der Linux-Oblivion.ini wirkt nicht, solange bDoHighDynamicRange=1
steht (der effektive Sample-Zähler der Engine las 0; `game/SceneFrame.h` nennt HDR als Bedingung).
Die Windows-INI hat HDR aus, dort zählt das iMultiSample=4. Unterschiede, die bleiben und nicht
getrennt gemessen sind: der Raum (Haus gegen Halle), die Installation seit 21:20, FoliageAAs Pässe,
ein frisches System. Nadis Live-Capture von 21:21-21:22 (60 s, world_dual): Intervall p50 13,3,
p95 13,7, p99 15,1, max 56,6 ms, 8 Frames über 20 ms - ebenfalls ruhig, Szene unbekannt.

### Nachtrag: die Halle noch einmal, mit dem neuen Budget (2026-10-24, 10:52)

Profiler-Fix 67d4a47 (Tabellen je Frame bemessen, freier Ordnername): die Capture startete mit
262144 Events, 32768 Frames, 1024 GPU-Samples und exportierte vollständig (116744 Events in 60 s,
Budget reicht jetzt für etwa 135 s). `perf-people-hall-steady.txt` PASS.

| Halle, Leute stehen (Capture 100-160 s) | 2026-10-23 21:11 | 2026-10-24 10:52 |
|---|---:|---:|
| Frames > 20 ms in 60 s | 28 | 57 |
| zweimal gezeigt (num_presents >= 2) | 19 | 41 |
| total_render_gpu_ms p50 / p95 | 6,11 / 7,31 | 6,24 / 7,51 |
| GPU-Frames > 12 ms | 1 | 15 |
| Frame-Intervall p50 / p99 | - | 13,33 / 23,21 ms |

Die langen Frames (26,7 ms, oft mit einem 23-ms-Nachzügler) sind keine GPU-Spikes: 0 von 57 liegen
innerhalb von zwei Frames an einem GPU-Frame über 12 ms, die GPU liegt rundherum bei 6-7,6 ms,
scene_pass 3,35 gegen 3,29 ms. Die ganzen zusätzlichen 12 ms stecken in poses_wait (20,7 gegen
8,4 ms): WaitGetPoses hält den Frame eine VSync länger. Jauffres Haus (08:45) hatte das nicht, also
gehört es zur Halle, nicht zum Tag. Warum der Compositor das in der Halle tut, ist offen; dass das
schlafende Headset des Harness mitspielt, ist eine Vermutung. Neu gegenüber 21:11: fünf
Halbrate-Episoden (39 bis 203 Frames) beim Laden und in der leeren Halle vor den Leuten - zwischen
den beiden Läufen liegt die Install-Änderung von 21:20.

Nachtrag dazu: seit 2026-10-24 07:31 loggt jeder Harness-Lauf "OpenVR: CreateOverlay('obvr.hud')
failed (17)" (KeyInUse; SteamVR hält den Schlüssel, Fund der OBVR-Session). Seit 07:49 fehlten auch das
Crosshair und alle 11 Hand-HUD-Overlays. Ursache: vrcompositor brach um 07:29:45, 07:49:31 und 08:07:18
ab (ASSERT "Vulkan transfer ring buffer overflow"), die Schlüssel der toten Spiele blieben bis zum
SteamVR-Neustart um 19:24 registriert. Betroffen: Haus 08:45 und Halle 10:52, nicht die Halle 21:11. Haus gegen Halle von heute bleibt vergleichbar, die Halle heute
gegen gestern nicht ganz.
