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
```

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

## Grenzen

Ein großer `poses_wait_wall_ms`-Wert kann normales Compositor-Pacing sein. Ein großer
`scene_pass_wall_ms`-Wert kann Engine-Arbeit oder Treiberwartezeit enthalten. GPU-Marker
zeigen keine Shaderauslastung und keine exklusive GPU-Zeit. Reprojection oder verfehlte
Headset-Frames dürfen nur mit separater SteamVR-Evidenz behauptet werden.

Das Profiling selbst parallelisiert keine Augen. Die vorhandene Dual-Pass-Logik bleibt
unverändert; eine spätere Render-Parallelisierung wäre ein eigenes Architekturprojekt.
