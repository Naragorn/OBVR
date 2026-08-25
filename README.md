# OBVR

VR für das originale **The Elder Scrolls IV: Oblivion (2006)** — ausdrücklich nicht für das Remaster.

Leitgedanke: *Oblivion bleibt Oblivion.* OBVR ersetzt kein Gameplay, sondern setzt auf die
bestehende Kamera- und Renderpipeline echtes natives VR auf.

```
Vanilla Oblivion Camera
        ×
relative HMD rotation
        =
final VR camera
```

## Stand: 0.0.2 — Kopfrotation als austauschbare Quelle

Noch kein Stereo, noch kein Headset. 0.0.1 hat die Kernfrage beantwortet:

> Lässt sich nach Oblivions Vanilla-Kameraberechnung eine zusätzliche Rotation aufsetzen,
> ohne First Person, Third Person oder Animationen zu beschädigen?

Ja — im laufenden Spiel bestätigt, siehe unten.

0.0.2 ersetzt den festen Testwinkel durch eine austauschbare Quelle (`vr::HeadTracker`).
Die Rotation kommt als Quaternion herein, wird gegen eine Recenter-Referenz verrechnet und
von OpenXR- in Oblivion-Konvention gedreht. Welche Quelle das Quaternion liefert, ist
Konfiguration: fester Winkel, simulierte Kopfbewegung, oder später ein echtes Headset.

Der simulierte Kopf ist kein Spielzeug, sondern die einzige Möglichkeit, die vollständige
Kette bis zur Kameramatrix ohne HMD im Spiel zu prüfen — und auf einem Linux-System ohne
funktionierende VR-Kette der einzige Weg überhaupt.

## Wie der Hook funktioniert

Oblivion.exe 1.2.0.416 berechnet die Spielerkamera und schreibt das Ergebnis in den
`CameraNode` des Szenengraphen. Der Hook sitzt unmittelbar danach, bei `0x0066BE6E`.

Der relevante Code in der Binary:

```
0066BE33  mov  eax, [edx]              ; eax = CameraNode (NiAVObject*)
0066BE3D  mov  [eax+0x54], ecx         ; localTransform.pos.x
0066BE44  mov  [eax+0x58], edx         ; localTransform.pos.y
0066BE47  mov  [eax+0x5C], ecx         ; localTransform.pos.z
...
0066BE60  lea  edi, [eax+0x30]         ; localTransform.rot
0066BE63  mov  ecx, 9
0066BE68  lea  esi, [esp+0x60]
0066BE6C  rep  movsd                   ; 9 DWORDs = NiMatrix33
0066BE6E  <-- OBVR hängt sich hier ein
```

Ab dieser Stelle stehen Position und Rotation fest und `eax` hält noch den `CameraNode`.
Die Schreibziele `[eax+0x30]` und `[eax+0x54]` belegen zugleich das `NiAVObject`-Layout
(`localTransform` bei `0x30`, darin `pos` bei `+0x24`).

Kurz darauf ruft das Spiel auf demselben Knoten `NiAVObject::UpdateSelectedDownwardPass`
auf (`0x00707370`, vtable-Slot `0x64` = Index 25). Deshalb ändert OBVR `localTransform`
und nicht `worldTransform`: die Welttransformation wird ohnehin neu aus `parent * local`
berechnet.

Alle Adressen in `src/game/GameAddresses.h` sind gegen die tatsächliche Binary
disassembliert und dort mit dem Befund belegt.

### Warum kein Inline-Assembly

Vergleichbare Mods lösen solche Mid-Function-Hooks mit `__declspec(naked)` und einem
`__asm`-Block. Das bindet das Projekt an MSVC. OBVR erzeugt die Trampolin-Bytes stattdessen
zur Laufzeit über einen winzigen `CodeWriter`. Das baut mit MSVC, clang-cl und clang-cross
gleichermaßen — und lässt sich ohne laufendes Oblivion testen.

Das Trampolin sichert alle Register, ruft OBVR auf und führt danach die überschriebene
Instruktion samt ihrem originalen Kontrollfluss aus:

```
pushad
pushfd
push dword ptr [esp+0x20]       ; das von pushad gesicherte EAX = CameraNode
call OBVR_OnCameraUpdated
add  esp, 4
popfd
popad
cmp  word ptr [ebx+0xB6], 0     ; Original
ja   0x0066BE7C                 ; Original
xor  ecx, ecx                   ; Original
jmp  0x0066BE84                 ; Original
```

Vor dem Patchen prüft OBVR, dass an der Zieladresse tatsächlich die erwarteten acht Bytes
stehen. Stimmen sie nicht — andere Spielversion, oder ein anderer Mod war zuerst da —
unterbleibt der Patch und Oblivion startet unverändert.

## Bauen

### Regulär: MSVC unter Windows

Oblivion.exe ist 32 Bit, die DLL muss es also auch sein.

```
cmake -B build -A Win32
cmake --build build --config Release
```

### Verifikation unter Linux, ohne Windows SDK

Prüft, dass alles sauber zu einer 32-Bit-Windows-DLL übersetzt und linkt, ohne
Windows-Maschine und ohne mehrere Gigabyte SDK. Möglich, weil OBVR außer `kernel32` und
`msvcrt` nichts braucht und die Importbibliotheken via `llvm-dlltool` selbst erzeugt werden.

Benötigt `clang`, `lld`, `llvm` und `cmake`.

```
cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake -G Ninja
cmake --build build
```

Dieser Weg baut ohne C++-Standardbibliothek und ohne Ausnahmen. Er ist eine Prüfumgebung,
keine Komfortumgebung — für ein Release bleibt MSVC der Weg.

### Tests

Zwei Testbinaries, beide ohne laufendes Oblivion:

- **`trampoline_test`** prüft die erzeugten Hook-Bytes gegen von Hand nachgerechnete
  Sollwerte. Ein Fehler dort lässt Oblivion zuverlässig abstürzen.
- **`quaternion_test`** prüft die Quaternion-Mathematik und den Basiswechsel von OpenXR
  nach Oblivion. Entscheidend ist dort die Gegenprobe gegen `EulerToMatrix`: diese Funktion
  ist im laufenden Spiel verifiziert, die Quaternion-Route wird also an eine belegte
  Referenz gebunden statt nur gegen sich selbst geprüft. Beide Implementierungen sind
  bewusst unabhängig — elementare Achsenmatrizen gegen Quaternion-Formel.

```
cmake -B build-tests tests -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Installieren

1. [xOBSE](https://github.com/llde/xOBSE/releases/latest) entpacken und
   `obse_1_2_416.dll`, `obse_editor_1_2.dll`, `obse_steam_loader.dll`, `obse_loader.exe`
   sowie den `Data`-Ordner ins Oblivion-Verzeichnis kopieren (getestet mit 22.13).
2. `OBVR.dll` nach `Data/OBSE/Plugins/` kopieren.
3. `OBVR.ini` neben `Oblivion.exe` legen.
4. Oblivion über den OBSE-Loader starten.

`OBVR.log` entsteht neben `Oblivion.exe`.

### Steam Proton unter Linux

Für Proton verlangt xOBSE, dass der Loader den Launcher ersetzt:

```
cd ~/.local/share/Steam/steamapps/common/Oblivion
cp OblivionLauncher.exe OblivionLauncher.exe.vanilla
cp obse_loader.exe OblivionLauncher.exe
```

Danach normal über Steam starten. Proton-CachyOS bringt einen eigenen protonfix mit, der
`OblivionLauncher.exe` ohnehin auf `obse_loader.exe` umbiegt — beide Wege führen zum Ziel.

Zum Starten ohne Steam-Oberfläche muss der Weg über die Steam Linux Runtime gehen; ein
blosses `proton run` bricht still ab:

```
STEAM_COMPAT_DATA_PATH=~/.local/share/Steam/steamapps/compatdata/22330 \
STEAM_COMPAT_CLIENT_INSTALL_PATH=~/.local/share/Steam \
~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point \
  --verb=waitforexitandrun -- \
  /usr/share/steam/compatibilitytools.d/proton-cachyos-slr/proton waitforexitandrun \
  ~/.local/share/Steam/steamapps/common/Oblivion/OblivionLauncher.exe
```

## Verifikation im Spiel

0.0.1 wurde gegen Oblivion GOTY (Steam, AppID 22330) unter Proton mit xOBSE 22.13
getestet. Ergebnis:

| Prüfung | Ergebnis |
| --- | --- |
| xOBSE lädt die DLL | `plugin OBVR.dll (00000003 OBVR 00000001) loaded correctly` |
| Spielversion | OBSE meldet `010201A0` = 1.2.0.416 |
| Byte-Vergleich an `0x0066BE6E` | bestanden, Hook gesetzt |
| Trampolin | 37 Bytes, wie vom Test vorhergesagt |
| Callback feuert | ja, pro Frame |
| Third Person | Rotation wirkt sichtbar |
| First Person | Rotation wirkt sichtbar |
| POV-Wechsel | in beide Richtungen erkannt und protokolliert |
| Stabilität | kein Absturz |

Damit ist die Kernfrage von 0.0.1 beantwortet: Oblivions Vanilla-Kameraberechnung lässt
sich um eine Zusatzrotation ergänzen, in beiden Kameramodi, ohne Gameplay oder
Animationen anzufassen.

Auszug aus `OBVR.log`:

```
OBSE-Version 22, Oblivion-Version 010201A0
Config: HookEnabled=1 Source=fixed Fixed=(P 0.0, R 20.0, Y 0.0)
Kamera: Hook auf 0066BE6E gesetzt, Trampolin bei 024C0000 (37 Bytes)
OBVR bereit
Kamera: erster Hook-Durchlauf, CameraNode=1812BC40, Third Person
Kamera: Wechsel nach First Person (Frame 1621)
Kamera: Wechsel nach Third Person (Frame 1745)
```

### 0.0.2 im Spiel

Der simulierte Kopf und der Hot-Reload wurden im selben Aufbau geprüft.

| Prüfung | Ergebnis |
| --- | --- |
| Simulierte Kopfbewegung | Kamera schwenkt sichtbar, Charakter dreht sich nicht mit |
| Quaternion-Werte im Log | stimmen auf drei Nachkommastellen mit einer unabhängigen Nachrechnung überein |
| Kameraposition | bleibt konstant — reine Rotation, kein Positionsversatz |
| Kompass | zeigt unverändert dieselbe Richtung, die Spielerausrichtung bleibt also unberührt |
| Hot-Reload | Quellenwechsel `simulated` → `fixed` im laufenden Spiel, ohne Neustart |

Die geloggten Quaternionen lassen sich direkt nachrechnen. Bei `SimulatedPeriodFrames=600`,
`SimulatedYawDegrees=25`, `SimulatedPitchDegrees=12`:

```
Frame  540  Log (-0.099, -0.127, -0.013, 0.987)   Rechnung (-0.099, -0.127, -0.013, 0.987)
Frame  720  Log ( 0.060,  0.206, -0.013, 0.977)   Rechnung ( 0.060,  0.206, -0.013, 0.977)
Frame  900  Log ( 0.000, -0.000,  0.000, 1.000)   Rechnung (-0.000,  0.000,  0.000, 1.000)
```

Frame 900 liegt bei Phase π, wo beide Sinusterme null sind — die Identität ist dort also
das erwartete Ergebnis, kein Aussetzer.

Nach dem Umschalten auf `Source=fixed` mit `FixedRoll=35` steht das Quaternion konstant bei
`(0.000, 0.000, -0.301, 0.954)`. Das ist exakt eine Drehung um 35°: sin(−17,5°) = −0,3007,
cos(17,5°) = 0,9537.

Belege liegen unter `docs/verification/`.

Zwei Beobachtungen zur Testumgebung: Oblivion pausiert, sobald sein Fenster den Fokus
verliert — die Frame-Zähler stehen dann still. Und synthetische Klicks kommen im
Hauptmenü nicht an, im geladenen Spiel dagegen schon.

### Achsenzuordnung

Der lokale Kameraraum folgt der Gamebryo-Konvention: X nach rechts, Y in Blickrichtung,
Z nach oben. Für `EulerToMatrix(x, y, z)` heißt das:

| Achse | INI-Schlüssel | Wirkung | Status |
| --- | --- | --- | --- |
| X | `FixedPitch` | Pitch — hoch und runter schauen | im Spiel bestätigt |
| Y | `FixedRoll` | Roll — Kopf zur Seite neigen | im Spiel bestätigt |
| Z | `FixedYaw` | Yaw — nach links und rechts schauen | folgt zwingend aus den beiden anderen, nicht einzeln geprüft |

Beim Roll-Test steht der Horizont schräg, beim Pitch-Test bleibt er waagerecht und die
Blickrichtung kippt nach unten. Das HUD bleibt in beiden Fällen unberührt, was bestätigt,
dass der Eingriff wirklich nur die Kamera betrifft.

Noch offen: ob Vanity- und Dialogkamera stören. Beide laufen über eigene Zweige.

## VR-Backend: warum OpenVR zuerst kommt

Der Fahrplan sah ursprünglich OpenXR als einzige Anbindung vor. Die Recherche für 0.0.2 hat
das korrigiert, und der Grund ist die Bitness.

Oblivion.exe ist 32 Bit, OBVR.dll damit auch. Bei OpenXR ist 32-Bit-Unterstützung bis heute
lückenhaft:

| Runtime | 32-Bit-OpenXR |
| --- | --- |
| SteamVR | erst ab Beta 2.17.2 (Juni 2026); Stable steht bei 2.16 |
| Meta / Oculus | ja |
| VDXR (Virtual Desktop) | ja |
| Pimax | ja |
| Varjo | nein |
| WMR | ja, aber von Microsoft eingestellt |

Der offizielle Win32-Loader existiert (im NuGet-Paket `OpenXR.Loader`, nicht im ZIP), aber
er nützt nichts, wenn die Runtime keine 32-Bit-DLL registriert hat.

**OpenVR hat dieses Problem nicht.** `openvr_api.dll` gibt es seit jeher für x86, und Valve
bestätigt das explizit: *„OpenVR supports 32-bit applications. On the other hand, the OpenXR
implementation in SteamVR does not support them."* Architektonisch ist OpenVR ohnehin
out-of-process (`vrclient` ↔ `vrserver.exe`) — die 64/32-Bit-Brücke hat Valve dort schon
gebaut. Über SteamVR erreicht ein einziges OpenVR-Backend mehr Headsets als alle
32-Bit-OpenXR-Runtimes zusammen.

Den engsten Präzedenzfall liefert [openRBRVR](https://github.com/Detegr/openRBRVR):
Richard Burns Rally von 2004, ebenfalls 32 Bit und D3D9. Es läuft vollständig in-process
und unterstützt OpenVR *und* OpenXR über eine Backend-Umschaltung.

Daraus folgt für OBVR: `TrackerSource` ist bewusst eine Aufzählung mit austauschbaren
Quellen. OpenVR kommt zuerst, OpenXR als zweites Backend. Ein 64-Bit-Hilfsprozess mit
Shared Memory (wie ihn [fear-vr](https://github.com/DR-89/fear-vr) für F.E.A.R. fährt) wäre
für das Lesen eines Quaternions deutlich überdimensioniert und bleibt der Notnagel, falls
sich später doch eine Runtime nur so erreichen lässt.

**Unter Proton** fällt OpenXR ohnehin aus: `wineopenxr` wird laut Protons `Makefile.in`
nicht für i386 gebaut. Dort führt nur OpenVR zum Ziel.

## Fahrplan

| Version | Inhalt | Stand |
| --- | --- | --- |
| 0.0.1 | Plugin lädt, Logging, Versionsprüfung, Kamera-Hook, feste Testrotation | im Spiel verifiziert |
| 0.0.2 | Quaternion-Schicht, Recenter, austauschbare Kopfquelle, Konfigurations-Hot-Reload | Gerüst steht, Backend fehlt |
| 0.0.3 | OpenVR anbinden, echte HMD-Rotation auf die Kamera — noch Monitorbild | offen |
| 0.0.4 | Frameloop, linke und rechte Swapchain, Testbilder im Headset | offen |
| 0.1.0 | Oblivions Welt als echtes Dual-Pass-Stereo, beide Augen im selben Game-Frame | offen |

Qualitätsziel für das Stereo-Rendering, in dieser Reihenfolge:

1. Echtes Dual-Pass-Stereo ← Ziel
2. Single-Pass/Multiview ← spätere Optimierung
3. AER ← Plan B
4. Depth-Reprojection ← Notlösung

Der eigentliche Knackpunkt ist nicht GPU-Leistung, sondern die Frage, ob Gamebryo innerhalb
desselben Game-Ticks zweimal die Welt rendern kann, ohne Simulation, Physik, Partikel und
Animationen zweimal weiterzuschalten.

Nicht im ersten Scope: Hände, Motion Controller, Roomscale, 6DoF, IK, physische Interaktion.

## Referenzen

- [llde/xOBSE](https://github.com/llde/xOBSE) — Plugin-Loader; die Plugin-Schnittstelle in
  `src/obse/PluginInterface.h` ist eine minimale, binärkompatible Nachbildung von
  `obse/obse/PluginAPI.h`.
- [mcstfuerson/TES-Reloaded](https://github.com/mcstfuerson/TES-Reloaded) — Reverse-Engineering-Referenz.
  Der Hook-Punkt stammt von dort, wurde aber gegen die Binary nachgeprüft, bevor er
  übernommen wurde.
