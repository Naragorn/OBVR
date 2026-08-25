# OBVR — Übergabedokument

Stand: 2026-08-25. Dieses Dokument fasst alles zusammen, was bisher erarbeitet, verifiziert
und entschieden wurde. Es ist so geschrieben, dass die Arbeit auf einem anderen Rechner
(insbesondere Windows) ohne Rückfragen fortgesetzt werden kann.

---

## 1. Was OBVR ist

VR-Mod für das **originale The Elder Scrolls IV: Oblivion von 2006** — ausdrücklich **nicht**
für das Remaster.

Leitgedanke: *Oblivion bleibt Oblivion.* Kein Gameplay-Umbau, sondern echtes natives VR auf
der bestehenden Kamera- und Renderpipeline.

```
Vanilla Oblivion Camera
        ×
relative HMD rotation
        =
final VR camera
```

### Erster Funktionsumfang (bewusst eng)

- Umschaltung First/Third Person bleibt erhalten
- Steuerung weiter per Gamepad oder Tastatur/Maus, **keine** VR-Controller, **keine** Hände
- **kein** Snap Turn; horizontales Drehen weiter über rechten Stick
- vertikales Schauen per Gamepad soll möglichst deaktiviert werden
- HMD bestimmt die Kopfrotation (Yaw, Pitch, optional Roll)
- zunächst **keine** Positionsbewegung des Kopfes, kein Roomscale → 3DoF
- Recenter-Funktion
- Gameplay und Animationen unverändert

### Qualitätsziel Stereo-Rendering (in dieser Reihenfolge)

1. echtes Dual-Pass-Stereo ← Ziel
2. Single-Pass/Multiview ← spätere Optimierung
3. AER (Alternate Eye Rendering) ← Plan B
4. Depth-Reprojection ← Notlösung

Beide Augen gehören zum selben Game-Frame:

```
UpdateGame()        // einmal
SetCamera(leftEye)  → RenderWorld()
SetCamera(rightEye) → RenderWorld()
SubmitOpenVR/OpenXR()
```

Der große offene Forschungsbereich bleibt: *Kann Gamebryo innerhalb desselben Game-Ticks
zweimal die Welt rendern, ohne Simulation, Physik, Partikel und Animationen zweimal
weiterzuschalten?* Das ist nicht die GPU-Frage — Vanilla Oblivion ist für moderne GPUs
trivial — sondern eine Frage der Renderpipeline.

### Ausdrücklich nicht im ersten Scope

Hände, Motion Controller, Roomscale, 6DoF, IK, physische Interaktion, Waffenrichtung
per Controller.

---

## 2. Aktueller Stand

| Version | Inhalt | Stand |
| --- | --- | --- |
| 0.0.1 | Plugin lädt, Logging, Versionsprüfung, Kamera-Hook, feste Testrotation | **im Spiel verifiziert** |
| 0.0.2 | Quaternion-Schicht, Recenter, austauschbare Kopfquelle, Konfigurations-Hot-Reload | **im Spiel verifiziert**, VR-Backend fehlt |
| 0.0.3 | OpenVR anbinden, echte HMD-Rotation auf die Kamera | offen ← **hier weitermachen** |
| 0.0.4 | Frameloop, linke/rechte Swapchain, Testbilder im Headset | offen |
| 0.1.0 | Oblivions Welt als echtes Dual-Pass-Stereo | offen |

Beides wurde gegen Oblivion GOTY (Steam, AppID 22330) unter Proton mit xOBSE 22.13 getestet.
Belege (Screenshots + Logs) liegen unter `docs/verification/`.

---

## 3. Der Kamera-Hook — das technische Herzstück

### Hook-Punkt: `0x0066BE6E` in Oblivion.exe 1.2.0.416

Oblivion berechnet die Spielerkamera und schreibt das Ergebnis in den `CameraNode` des
Szenengraphen. Der Hook sitzt unmittelbar danach. Der disassemblierte Befund:

```
0066BE1B  mov  ebx, [esp+0x14]              ; Kameraobjekt
0066BE1F  cmp  word ptr [ebx+0xB6], 0       ; Knotenliste leer?
0066BE27  ja   0066BE2D
0066BE29  xor  eax, eax
0066BE2B  jmp  0066BE35
0066BE2D  mov  edx, [ebx+0xB0]              ; Knotenliste
0066BE33  mov  eax, [edx]                   ; eax = CameraNode (NiAVObject*)
0066BE35  mov  ecx, [esp+0x38]
0066BE39  mov  edx, [esp+0x3C]
0066BE3D  mov  [eax+0x54], ecx              ; localTransform.pos.x
0066BE40  mov  ecx, [esp+0x40]
0066BE44  mov  [eax+0x58], edx              ; localTransform.pos.y
0066BE47  mov  [eax+0x5C], ecx              ; localTransform.pos.z
0066BE4A  cmp  word ptr [ebx+0xB6], 0
0066BE52  ja   0066BE58
0066BE54  xor  eax, eax
0066BE56  jmp  0066BE60
0066BE58  mov  edx, [ebx+0xB0]
0066BE5E  mov  eax, [edx]                   ; eax = CameraNode
0066BE60  lea  edi, [eax+0x30]              ; localTransform.rot
0066BE63  mov  ecx, 9
0066BE68  lea  esi, [esp+0x60]
0066BE6C  rep  movsd                        ; 9 DWORDs = NiMatrix33
0066BE6E  <-- OBVR hängt sich hier ein
```

Ab dieser Stelle stehen Position und Rotation fest und `eax` hält noch den `CameraNode`.

**Wichtig — `localTransform`, nicht `worldTransform`:** Kurz nach dem Hook ruft das Spiel
auf demselben Knoten `NiAVObject::UpdateSelectedDownwardPass` auf (`0x00707370`, dispatcht
über vtable-Slot `0x64` = Index 25). Die Welttransformation wird also ohnehin neu aus
`parent * local` berechnet. Eine Änderung an `worldTransform` würde im selben Frame
überschrieben.

### Verifikation der Adressen

Die Steam-`Oblivion.exe` hat eine `.bind`-Section und ihren Entry-Point darin (SteamStub-DRM),
**aber die `.text` ist nicht verschlüsselt**. `objdump -d --start-address=... -M intel` liefert
direkt korrekten Code. Jede Adresse in `src/game/GameAddresses.h` ist so gegen die Binary
geprüft, nicht aus fremdem Code übernommen.

md5 der getesteten EXE: `cdd2f0c5eff198d4f26b7b5b54ce4930`

### NiAVObject-Layout (Oblivion, 32 Bit)

```
0x00  vtable
0x04  refCount
0x08  name
0x0C  controller
0x10  extraDataList
0x14  extraDataListLen (UInt16)
0x16  extraDataListCapacity (UInt16)
0x18  flags (UInt16)
0x1C  parent
0x20  worldBound (NiBound, 0x10)
0x30  localTransform (NiTransform, 0x34)   ← rot bei 0x30, pos bei 0x54
0x64  worldTransform (NiTransform, 0x34)
```

Belegt durch die Schreibziele `[eax+0x30]` und `[eax+0x54]` im Code oben.

### Weitere verifizierte Adressen

- `0x00B333C4` — Zeiger auf den PlayerCharacter
- `+0x588` — `isThirdPerson` im PlayerCharacter (belegt durch `0x0066C580` ToggleCamera)
- `0x010201A0` — erwartete Oblivion-Version 1.2.0.416

### Warum kein Inline-Assembly

Vergleichbare Mods (TES Reloaded) lösen solche Mid-Function-Hooks mit `__declspec(naked)`
und `__asm`. Das bindet an MSVC. OBVR erzeugt die Trampolin-Bytes stattdessen zur Laufzeit
über einen winzigen `CodeWriter`. Vorteile: baut mit MSVC, clang-cl und clang-cross
gleichermaßen — und die Byte-Erzeugung ist **ohne laufendes Oblivion testbar**.

Das erzeugte Trampolin (37 Bytes):

```
pushad
pushfd
push dword ptr [esp+0x20]       ; das von pushad gesicherte EAX = CameraNode
call OBVR_OnCameraUpdated
add  esp, 4
popfd
popad
cmp  word ptr [ebx+0xB6], 0     ; Original, originalgetreu reproduziert
ja   0x0066BE7C                 ; Original
xor  ecx, ecx                   ; Original
jmp  0x0066BE84                 ; Original
```

`pushad` legt EAX zuoberst ab, `pushfd` schiebt um vier Bytes — daher `[esp+0x20]`.

TES Reloaded nimmt an dieser Stelle eine Abkürzung und erzwingt einen Zweig. OBVR baut den
Original-Kontrollfluss vollständig nach.

Der Patch an der Zieladresse: `E9 <rel32>` plus drei `nop`, weil die überschriebene
Originalinstruktion acht Bytes lang ist und kein Rest stehen bleiben darf.

**Sicherung:** Vor dem Patchen prüft `mem::Verify`, dass dort die erwarteten acht Bytes
`66 83 BB B6 00 00 00 00` stehen. Andernfalls unterbleibt der Patch und Oblivion startet
unverändert — eine falsche Spielversion soll kein zerschossenes Codesegment ergeben.

---

## 4. Achsen und Koordinatensysteme

### Oblivion / Gamebryo

X = rechts, Y = vorne (Blickrichtung), Z = oben.

Für `EulerToMatrix(x, y, z)` (Reihenfolge Z·Y·X):

| Achse | INI-Schlüssel | Wirkung | Status |
| --- | --- | --- | --- |
| X | `FixedPitch` | Pitch — hoch/runter schauen | **im Spiel bestätigt** |
| Y | `FixedRoll` | Roll — Kopf seitlich neigen | **im Spiel bestätigt** |
| Z | `FixedYaw` | Yaw — links/rechts schauen | folgt zwingend, nicht einzeln geprüft |

Nachweis: Bei Roll steht der Horizont schräg, bei Pitch bleibt er waagerecht und die
Blickrichtung kippt. Das HUD bleibt in beiden Fällen unberührt — der Eingriff trifft
wirklich nur die Kamera.

### OpenXR → Oblivion

OpenXR: X = rechts, Y = oben, −Z = vorne.

Basiswechsel:

```
x_obl =  x_xr
y_obl = -z_xr
z_obl =  y_xr
```

Determinante +1, die Händigkeit bleibt erhalten (eine Spiegelung wäre im Headset sofort
sichtbar). Für Quaternionen heißt das: `(x, y, z, w) → (x, -z, y, w)`.

Gegenprobe: OpenXR-Yaw (um Y) wird zu Oblivion-Yaw (um Z) ✓, OpenXR-Pitch (um X) bleibt
Pitch ✓, OpenXR-Roll (um Z) wird zu Roll um −Y ✓.

**OpenVR liefert Posen als 3x4-Matrix in derselben Konvention wie OpenXR** (Y oben,
−Z vorne). Das OpenVR-Backend muss also nur Matrix→Quaternion umrechnen und dann durch
dieselbe `FromOpenXR`-Funktion schicken.

---

## 5. Projektstruktur

```
OBVR/
├── CMakeLists.txt                      DLL-Build
├── OBVR.ini                            Vorlage, gehört neben Oblivion.exe
├── README.md
├── HANDOFF.md                          dieses Dokument
├── cmake/
│   ├── toolchain-linux-nosdk.cmake     Cross-Build ohne Windows SDK
│   └── imports/                        .def-Dateien + Import-Lib-Erzeugung
├── docs/verification/                  Screenshots und Logs der Spieltests
├── src/
│   ├── Plugin.cpp                      OBSEPlugin_Query / _Load
│   ├── camera/
│   │   ├── CameraHook.{h,cpp}          Callback + Hook-Installation
│   │   └── CameraTrampoline.{h,cpp}    Byte-Erzeugung, plattformfrei, getestet
│   ├── core/
│   │   ├── CodeWriter.{h,cpp}          Mini-Assembler
│   │   ├── Config.{h,cpp}              INI + Hot-Reload
│   │   ├── Log.{h,cpp}                 Logging ohne CRT-Abhängigkeit
│   │   ├── MathFns.h                   sin/cos/sqrt, freestanding-tauglich
│   │   ├── Memory.{h,cpp}              SafeWrite / Verify / AllocExecutable
│   │   ├── Rotation.{h,cpp}            EulerToMatrix (im Spiel verifiziert)
│   │   └── Types.h
│   ├── game/
│   │   ├── GameAddresses.h             alle Adressen mit disassembliertem Beleg
│   │   ├── GameTypes.h                 NiAVObject (32-Bit-Layout)
│   │   └── NiMath.h                    NiPoint3/NiMatrix33/NiTransform
│   ├── obse/PluginInterface.h          binärkompatible xOBSE-Nachbildung
│   ├── platform/
│   │   ├── Freestanding.cpp            nur im SDK-freien Build
│   │   └── Win32Min.h                  schmale Win32-Schicht
│   └── vr/
│       ├── HeadTracker.{h,cpp}         austauschbare Kopfquelle ← hier ansetzen
│       └── Quaternion.{h,cpp}          Quaternion + Basiswechsel
└── tests/
    ├── TrampolineTest.cpp
    └── QuaternionTest.cpp
```

### Bewusste Design-Entscheidungen

- **Keine xOBSE-Header.** Die ziehen den kompletten Spielobjektbaum nach sich und binden an
  MSVC. `src/obse/PluginInterface.h` bildet nur die zwei nötigen Strukturen binärkompatibel
  nach. Oblivion Reloaded macht es genauso.
- **Kein `windows.h` im freestanding-Pfad.** `Win32Min.h` deklariert die ~10 benötigten
  Importe selbst. Die DLL braucht nur `kernel32` und `msvcrt`.
- **Mathematik von Speicherlayout getrennt.** `NiMath.h` enthält nur float-Strukturen und
  ist auf jeder Architektur gültig; `GameTypes.h` mit den zeigerbehafteten Strukturen prüft
  seine Offsets nur unter `OBVR_TARGET_32BIT`.

---

## 6. Bauen

### Windows / MSVC — der reguläre Weg

Oblivion.exe ist 32 Bit, die DLL muss es auch sein.

```
cmake -B build -A Win32
cmake --build build --config Release
```

Ergebnis: `build/Release/OBVR.dll`.

Die statische Runtime ist voreingestellt (`MultiThreaded`), damit keine Redistributable
neben Oblivion liegen muss.

### Linux ohne Windows SDK — Verifikationsbau

Prüft, dass alles sauber zu einer 32-Bit-Windows-DLL übersetzt und linkt, ohne
Windows-Maschine und ohne SDK-Download. Möglich, weil nur `kernel32` und `msvcrt` gebraucht
werden und die Import-Libs via `llvm-dlltool` aus `.def`-Dateien erzeugt werden.

Benötigt `clang`, `lld`, `llvm`, `cmake`, `ninja`.

```
cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake -G Ninja
cmake --build build
```

Baut ohne C++-Standardbibliothek und ohne Ausnahmen. Prüfumgebung, keine Komfortumgebung.

### Tests (nativ, auf jeder Plattform)

```
cmake -B build-tests tests -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

- `trampoline_test` — die erzeugten Hook-Bytes gegen von Hand nachgerechnete Sollwerte.
- `quaternion_test` — Quaternion-Mathematik und Basiswechsel. **Der wichtige Kniff:** die
  Gegenprobe läuft gegen `EulerToMatrix`, das im Spiel visuell verifiziert ist. Beide
  Implementierungen sind bewusst unabhängig (elementare Achsenmatrizen gegen
  Quaternion-Formel), sonst wäre der Vergleich wertlos.

---

## 7. Installieren

1. [xOBSE](https://github.com/llde/xOBSE/releases/latest) (getestet: 22.13) entpacken,
   `obse_1_2_416.dll`, `obse_editor_1_2.dll`, `obse_steam_loader.dll`, `obse_loader.exe`
   und den `Data`-Ordner ins Oblivion-Verzeichnis kopieren.
2. `OBVR.dll` nach `Data/OBSE/Plugins/`.
3. `OBVR.ini` neben `Oblivion.exe`.
4. Starten über den OBSE-Loader.

`OBVR.log` entsteht neben `Oblivion.exe`.

### Steam Proton unter Linux

xOBSE verlangt, dass der Loader den Launcher ersetzt:

```
cd ~/.local/share/Steam/steamapps/common/Oblivion
cp OblivionLauncher.exe OblivionLauncher.exe.vanilla
cp obse_loader.exe OblivionLauncher.exe
```

Proton-CachyOS bringt zusätzlich einen protonfix mit, der `OblivionLauncher.exe` ohnehin auf
`obse_loader.exe` umbiegt.

Start ohne Steam-Oberfläche — ein blosses `proton run` bricht still ab, es muss über die
Steam Linux Runtime gehen:

```
STEAM_COMPAT_DATA_PATH=~/.local/share/Steam/steamapps/compatdata/22330 \
STEAM_COMPAT_CLIENT_INSTALL_PATH=~/.local/share/Steam \
~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point \
  --verb=waitforexitandrun -- \
  /usr/share/steam/compatibilitytools.d/proton-cachyos-slr/proton waitforexitandrun \
  ~/.local/share/Steam/steamapps/common/Oblivion/OblivionLauncher.exe
```

---

## 8. Konfiguration (OBVR.ini)

```ini
[Camera]
HookEnabled=1          ; nur beim Start gelesen

[Head]
Source=fixed           ; none | fixed | simulated | openvr | openxr
FixedPitch=0.0
FixedRoll=20.0
FixedYaw=0.0
SimulatedYawDegrees=25.0
SimulatedPitchDegrees=12.0
SimulatedPeriodFrames=600

[Debug]
LogEveryFrames=0       ; 0 = aus
ReloadEveryFrames=120  ; Hot-Reload der [Head]/[Debug]-Werte, 0 = aus
```

**Der Hot-Reload ist beim Entwickeln entscheidend.** Ohne ihn kostet jede Winkeländerung
einen kompletten Neustart samt Laden eines Spielstands. Mit ihm lässt sich die Kamera im
laufenden Spiel abstimmen — Quellenwechsel eingeschlossen.

`Source=simulated` fährt eine langsame Kopfbewegung und ist die einzige Möglichkeit, die
vollständige Kette bis zur Kameramatrix **ohne Headset** zu prüfen.

---

## 9. VR-Backend: warum OpenVR und nicht OpenXR

Der ursprüngliche Fahrplan sah OpenXR vor. Das ist für ein 32-Bit-Spiel die falsche erste
Wahl.

### OpenXR und 32 Bit

Der offizielle Win32-Loader existiert (im NuGet-Paket `OpenXR.Loader`, **nicht** im
ZIP-Release). Ein 32-Bit-Prozess landet per WOW64 in
`HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ActiveRuntime`. Der Knackpunkt ist nicht der
Loader, sondern ob die Runtime dort eine 32-Bit-DLL registriert hat:

| Runtime | 32-Bit-OpenXR |
| --- | --- |
| SteamVR | erst ab **Beta** 2.17.2 (Juni 2026); Stable stand bei 2.16 |
| Meta / Oculus | ja |
| VDXR (Virtual Desktop) | ja |
| Pimax | ja |
| Varjo | nein |
| WMR | ja, aber von Microsoft eingestellt |

**Für ein Bigscreen Beyond heißt das: OpenXR läuft über SteamVR — und dort ist
32-Bit-Support noch Beta.**

### OpenVR

`openvr_api.dll` gibt es seit jeher für x86 (`bin/win32/` im Valve-Repo). Valve bestätigt es
explizit (ValveSoftware/openvr Issue #1687):

> *"OpenVR supports 32-bit applications. On the other hand, the OpenXR implementation in
> SteamVR does not support them, as opposed to most other vendors OpenXR runtimes."*

Architektonisch ist OpenVR ohnehin out-of-process (`vrclient` ↔ `vrserver.exe`) — die
64/32-Bit-Brücke hat Valve bereits gebaut.

### Präzedenzfall

[openRBRVR](https://github.com/Detegr/openRBRVR) — Richard Burns Rally von 2004, ebenfalls
32 Bit und D3D9, praktisch dieselbe Ausgangslage. Läuft **vollständig in-process** und
unterstützt OpenVR *und* OpenXR über eine Backend-Umschaltung. Für das Rendering nutzt es
einen DXVK-Fork mit VR-Support (D3D9 → Vulkan), was das Textur-Übergabeproblem gleich
mitlöst.

Ein 64-Bit-Hilfsprozess mit Shared Memory (wie ihn
[fear-vr](https://github.com/DR-89/fear-vr) für F.E.A.R. fährt) wäre fürs Lesen eines
Quaternions deutlich überdimensioniert und bleibt der Notnagel.

### Unter Proton

`wineopenxr` wird laut Protons `Makefile.in` **nicht für i386 gebaut** (Begründung dort:
von SteamVR nicht unterstützt). Unter Proton fällt OpenXR damit komplett aus.

---

## 10. VR-Hardwarelage auf dem Linux-Testsystem

Getestet am 2026-08-25 auf CachyOS (Kernel 7.2.0), GNOME/Wayland, NVIDIA.

### Was funktioniert

Das **Bigscreen Beyond wird vollständig erkannt**:

```
USB:  35bd:0101 Bigscreen Beyond
      35bd:0105 Bigscreen Beyond Audio Strap
      28de:2102 Valve VR Radio  (2x)
      28de:2300 Valve Tundra Tracker
HID:  hidraw11 = Bigscreen Beyond   (beschreibbar, Gruppe wheel)
DRM:  card1-DP-2  status=connected  enabled=disabled
EDID: Display Product Name 'Beyond'
      Primary Use Case: Head-mounted Virtual Reality (VR) display
      Modi: 5088x2544, 3840x1920
```

Und **SteamVR erkennt das HMD**:

```
lighthouse: HMD Model: Bigscreen Beyond
Active HMD set to lighthouse.LHR-58B456BE
Using existing HMD lighthouse.LHR-58B456BE
```

Beim Lauf vom 12.08. stand dort noch `VRInitError_Init_HmdNotFound` — das Lighthouse-Tracking
greift also inzwischen.

### Was nicht funktioniert

- `lighthouse: Unable to query MC Image size` — die Mura-Korrekturdaten des Beyond werden
  nicht gelesen (unter Windows liefert die der Bigscreen-Treiber). Kosmetisch, aber ein
  Zeichen für unvollständige Treiberunterstützung.
- `lighthouse: Enumerating displays... SDL says there are 2 video displays` — SteamVR sieht
  nur die beiden Monitore, nicht das Beyond-Panel.
- **Der Start von SteamVR hat den GNOME-Desktop zum Absturz gebracht.** Neuanmeldung
  erforderlich. GNOME/Wayland + NVIDIA + SteamVR-Direct-Mode ist eine bekannt fragile
  Kombination.
- Steam meldet dauerhaft: `Refusing to init SteamVR build 23791826 because it crashed.`

### Bewertung

Für **0.0.3 (nur Kopfrotation lesen)** bräuchte man streng genommen nur das Tracking, nicht
die Display-Ausgabe — und das Tracking funktioniert. Aber solange der bloße Start von
SteamVR den Desktop mitreißt, ist das keine tragfähige Entwicklungsumgebung.

**Empfehlung: Windows-Dual-Boot für alles ab 0.0.3.**

Gründe:
1. SteamVR Linux crasht hier den Desktop.
2. Das Beyond braucht unter Windows ohnehin seinen offiziellen Treiber (Display-Aktivierung,
   Mura-Korrektur) — unter Linux gibt es dafür nur Community-Bastellösungen.
3. OpenVR 32-Bit ist unter Windows bewährt und erprobt.
4. Debugging mit MSVC/Visual Studio ist für ein Hook-Plugin deutlich angenehmer.

Linux bleibt als Umgebung für alles nützlich, was **kein** Headset braucht: Code schreiben,
Unit-Tests, den Cross-Build zur Verifikation, und Kameratests mit `Source=simulated`.

---

## 11. Fallstricke der Testumgebung (Linux/Proton)

Diese Punkte haben je einen fehlgeschlagenen Versuch gekostet:

- **Oblivion pausiert bei Fokusverlust.** Der Frame-Zähler steht still, sobald das Fenster
  nicht aktiv ist. Das sieht aus, als würde ein Hot-Reload nicht greifen.
- **Synthetische Eingaben:** `xdotool` (XTEST) wird vom Spiel komplett ignoriert. `ydotool`
  (uinput) funktioniert — aber Tastendrücke kommen nur an, wenn das Oblivion-Fenster
  **wirklich** fokussiert ist (`xdotool windowactivate --sync`, danach mit
  `getactivewindow` gegenprüfen). Mausbewegung wirkt auch ohne Fokus, was leicht zu dem
  Fehlschluss verleitet, die Eingabe käme an.
- **Im Hauptmenü kommen synthetische Klicks gar nicht an**, im geladenen Spiel dagegen
  problemlos. Der Sprung ins Spiel muss von Hand gemacht werden.
- **`proton run` allein bricht still ab** — der Weg muss über
  `SteamLinuxRuntime_4/_v2-entry-point` gehen (siehe Abschnitt 7).
- **Steams Launch-Pipeline** hängt gelegentlich bei `ProcessingShaderCache` und wartet auf
  eine GUI-Antwort.

---

## 12. Nächster Schritt: 0.0.3 — OpenVR anbinden

Der gesamte Unterbau steht. Was fehlt, ist ausschließlich die Quelle.

### Ansatzpunkt

`src/vr/HeadTracker.cpp`, Funktion `ReadSource`, Fall `TrackerSource::OpenVR`. Aktuell:

```cpp
case TrackerSource::OpenVR:
case TrackerSource::OpenXR:
    // Noch nicht angebunden. Bis dahin bleibt die Kamera unveraendert,
    // statt eine erfundene Orientierung zu liefern.
    return Quaternion::Identity();
```

### Zu tun

1. `openvr_api.dll` (**x86**, aus `bin/win32/` des Valve-Repos) und `headers/openvr.h`
   einbinden. Möglichst per `LoadLibrary`/`GetProcAddress` statt statischem Link, damit
   OBVR ohne installiertes SteamVR lädt.
2. `VR_Init(&err, VRApplication_Background)` — **`Background`, nicht `Scene`**: OBVR will
   nur Posen lesen und darf SteamVR nicht die Szene wegnehmen.
3. `IVRSystem::GetDeviceToAbsoluteTrackingPose(TrackingUniverseSeated, 0, poses, count)`,
   Index `k_unTrackedDeviceIndex_Hmd` (= 0).
4. Aus `HmdMatrix34_t` das Quaternion extrahieren (Standard-Matrix→Quaternion mit
   Spurfallunterscheidung). OpenVR nutzt dieselbe Achsenkonvention wie OpenXR, also
   anschließend durch die bestehende `FromOpenXR`-Funktion.
5. **Fehlerfall sauber behandeln:** Schlägt `VR_Init` fehl (kein SteamVR, kein HMD), muss
   OBVR auf die Vanilla-Kamera zurückfallen und das im Log vermerken — **nicht** abstürzen.
   Das ist wichtig, weil Nutzer das Plugin auch ohne laufendes SteamVR installiert haben
   werden.
6. Recenter auf eine Taste legen (`HeadTracker::Recenter()` existiert bereits).

### Tests

Matrix→Quaternion lässt sich wie die übrige Mathematik nativ testen: eine bekannte
`HmdMatrix34_t` konstruieren, umrechnen, gegen `EulerToMatrix` gegenprüfen. Damit bleibt der
neue Code an dieselbe im Spiel verifizierte Referenz gebunden.

### Danach (0.0.4)

Stereo-Rendering. Die Architekturentscheidung fällt dort über die *Rendering*-Strategie,
nicht über die VR-API:

- **Wurzel-Fix (Vorbild openRBRVR):** D3D9 → Vulkan über einen DXVK-Fork mit VR-Support.
  Bleibt in-process 32 Bit, liefert Vulkan-Texturen, die OpenVR direkt annimmt, und löst
  nebenbei Oblivions Single-Thread-D3D9-Probleme.
- **Pragmatisch:** natives D3D9 behalten, per D3D9Ex-Hilfsdevice + Shared Handle nach D3D11
  brücken. Ebenfalls in-process möglich; Preis ist ein CPU-Readback, weil klassisches D3D9
  keine Shared Surfaces kennt.

Hinweis zu OpenVR: `IVRCompositor::Submit` akzeptiert nur `TextureType_DirectX` (ID3D11),
OpenGL, Vulkan oder DirectX12. `TextureType_DXGISharedHandle` ist ausdrücklich nur für
Overlays. Aus D3D9 muss also so oder so gebrückt werden.

---

## 13. Referenzen

- [llde/xOBSE](https://github.com/llde/xOBSE) — Plugin-Loader. `src/obse/PluginInterface.h`
  ist eine minimale binärkompatible Nachbildung von `obse/obse/PluginAPI.h`.
- [mcstfuerson/TES-Reloaded](https://github.com/mcstfuerson/TES-Reloaded) —
  Reverse-Engineering-Referenz. Der Hook-Punkt stammt von dort, wurde aber vor der
  Übernahme gegen die Binary nachgeprüft. Besonders relevant: `TESReloaded/Core/CameraMode.cpp`
  und `TESReloaded/Framework/GameNi.h`.
- [openRBRVR](https://github.com/Detegr/openRBRVR) — nächster Verwandter: 32-Bit-D3D9-Spiel
  mit VR-Mod, in-process, OpenVR + OpenXR.
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr) — `bin/win32/openvr_api.dll`,
  `headers/openvr.h`.
- [DR-89/fear-vr](https://github.com/DR-89/fear-vr) — Zwei-Prozess-Architektur mit Shared
  Memory, falls je gebraucht.
