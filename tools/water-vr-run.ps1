# Automated water-reflection run for the second save in the Oblivion save list.
#
# The run starts through obse_loader.exe, selects the second save entry, captures
# the game view while the headset is swept from right to left, and evaluates
# the resulting BMP sequence plus OBVR.log with water_visual_harness.py.
#
# The DLL supplies a deterministic synthetic HMD yaw sweep after the save has
# loaded. The runner controls the second-save selection, enables the isolated
# test mode, waits for the in-game matrix/capture verdict, and evaluates the
# resulting left/right eye BMPs. Use -Attach when xOBSE is already at the menu.

[CmdletBinding()]
param(
	[string]$GameDir = "D:\SteamLibrary\steamapps\common\Oblivion",
	[string]$ArtifactDir,
	[int]$SaveIndex = 1,
	[int]$CaptureCount = 8,
	[int]$CaptureIntervalMs = 350,
	[int]$LoadWaitSec = 60,
	[ValidateRange(-80,80)][float]$PitchDegrees = -35,
	[switch]$KeepGameOpen,
	[switch]$Attach,
	[switch]$AlreadyInWorld,
	[switch]$ArmBeforeLoad,
	[switch]$WaitForManualLaunch,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ArtifactDir)) {
	$ArtifactDir = Join-Path $root "artifacts\water-vr"
}
$saveDir = Join-Path $env:USERPROFILE "Documents\My Games\Oblivion\Saves"
$logPath = Join-Path $GameDir "OBVR.log"
$loaderPath = Join-Path $GameDir "obse_loader.exe"

$saves = @(Get-ChildItem -LiteralPath $saveDir -Filter "*.ess" -File |
	Sort-Object LastWriteTime -Descending)
if ($saves.Count -le $SaveIndex) {
	throw "Save index $SaveIndex is unavailable; found $($saves.Count) .ess saves in $saveDir"
}
$targetSave = $saves[$SaveIndex]
Write-Host "Save entry $($SaveIndex + 1) from top: $($targetSave.Name)"
Write-Host "Save timestamp: $($targetSave.LastWriteTime)"

if ($DryRun) {
	Write-Host "Dry run: no game was started and no files were changed."
	exit 0
}
if (-not $Attach -and -not (Test-Path -LiteralPath $loaderPath)) {
	throw "OBSE loader not found: $loaderPath"
}
if ($Attach -and -not (Get-Process -Name Oblivion -ErrorAction SilentlyContinue)) {
	throw "-Attach was requested, but Oblivion.exe is not running."
}
if (-not $Attach -and (Get-Process -Name Oblivion -ErrorAction SilentlyContinue)) {
	throw "Oblivion is already running; use -Attach to reuse it."
}
if ($Attach -and $WaitForManualLaunch) {
	throw "-Attach and -WaitForManualLaunch cannot be combined."
}
if ($AlreadyInWorld -and -not $Attach) {
	throw "-AlreadyInWorld requires -Attach."
}
$oblivionIni = Join-Path $env:USERPROFILE "Documents\My Games\Oblivion\Oblivion.ini"
if (-not (Test-Path -LiteralPath $oblivionIni)) {
	throw "Oblivion.ini not found: $oblivionIni"
}
$oblivionIniText = [IO.File]::ReadAllText($oblivionIni)
$disabledReflectionSettings = @()
foreach ($key in @(
	"bUseWaterReflections",
	"bUseWaterReflectionsTrees",
	"bUseWaterReflectionsStatics",
	"bUseWaterReflectionsActors",
	"bUseWaterReflectionsMisc"
)) {
	if (-not [regex]::IsMatch($oblivionIniText,
		"(?m)^" + [regex]::Escape($key) + "\s*=\s*1\s*$")) {
		$disabledReflectionSettings += $key
	}
}
if ($disabledReflectionSettings.Count -ne 0) {
	throw "Oblivion water reflections are disabled: $($disabledReflectionSettings -join ', ')"
}
if (-not (Test-Path -LiteralPath $ArtifactDir)) {
	New-Item -ItemType Directory -Path $ArtifactDir -Force | Out-Null
}
Get-ChildItem -LiteralPath $ArtifactDir -Filter "*.bmp" -File -ErrorAction SilentlyContinue |
	Remove-Item -Force
@("OBVR.log", "water-vr-result.json") | ForEach-Object {
	$path = Join-Path $ArtifactDir $_
	if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public static class ObvrWaterRun {
	[StructLayout(LayoutKind.Sequential)]
	struct INPUT { public uint type; public MOUSEINPUT mi; }
	[StructLayout(LayoutKind.Sequential)]
	struct MOUSEINPUT {
		public int dx; public int dy; public uint mouseData;
		public uint dwFlags; public uint time; public IntPtr dwExtraInfo;
	}
	[DllImport("user32.dll")]
	static extern void keybd_event(byte key, byte scan, uint flags, IntPtr extra);
	[DllImport("user32.dll")]
	public static extern bool SetForegroundWindow(IntPtr handle);
	[StructLayout(LayoutKind.Sequential)]
	struct RECT { public int left; public int top; public int right; public int bottom; }
	[DllImport("user32.dll")]
	static extern bool GetWindowRect(IntPtr handle, out RECT rect);
	[DllImport("user32.dll")]
	static extern bool GetClientRect(IntPtr handle, out RECT rect);
	public delegate bool EnumWindowProc(IntPtr handle, IntPtr parameter);
	[DllImport("user32.dll")]
	static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);
	[DllImport("user32.dll")]
	static extern IntPtr SendMessage(IntPtr handle, uint message, IntPtr wParam, IntPtr lParam);
	[DllImport("user32.dll")]
	static extern bool SetCursorPos(int x, int y);
	[DllImport("user32.dll")]
	static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
	public static void ClickWindowOffset(IntPtr handle, int x, int y) {
		int packed = (y << 16) | (x & 0xffff);
		IntPtr point = new IntPtr(packed);
		SendMessage(handle, 0x0201, new IntPtr(1), point);
		System.Threading.Thread.Sleep(120);
		SendMessage(handle, 0x0202, IntPtr.Zero, point);
	}
	public static void ClickLauncherPlay(IntPtr handle) {
		IntPtr target = IntPtr.Zero;
		RECT best = new RECT();
		EnumChildWindows(handle, delegate(IntPtr child, IntPtr parameter) {
			RECT rect;
			if (!GetWindowRect(child, out rect)) return true;
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			if (width >= 200 && width <= 300 && height >= 25 && height <= 60 &&
			    (target == IntPtr.Zero || rect.top < best.top)) {
				target = child;
				best = rect;
			}
			return true;
		}, IntPtr.Zero);
		if (target == IntPtr.Zero) throw new InvalidOperationException("Play bitmap not found");
		int x = (best.left + best.right) / 2;
		int y = (best.top + best.bottom) / 2;
		SetForegroundWindow(handle);
		System.Threading.Thread.Sleep(250);
		SetCursorPos(x, y);
		System.Threading.Thread.Sleep(150);
		mouse_event(0x0002, 0, 0, 0, UIntPtr.Zero);
		System.Threading.Thread.Sleep(120);
		mouse_event(0x0004, 0, 0, 0, UIntPtr.Zero);
	}
	public static void Press(byte key, byte scan) {
		keybd_event(key, scan, 0, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(key, scan, 2, IntPtr.Zero);
	}
	public static void PressExtended(byte key, byte scan) {
		keybd_event(key, scan, 1, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(key, scan, 3, IntPtr.Zero);
	}
}
"@

function Get-GameProcess {
	Get-Process -Name Oblivion -ErrorAction SilentlyContinue | Select-Object -First 1
}

function Get-LauncherProcess {
	Get-Process -Name OblivionLauncher -ErrorAction SilentlyContinue | Select-Object -First 1
}

function Focus-ProcessWindow($process) {
	if ($process -and $process.MainWindowHandle -ne 0) {
		[ObvrWaterRun]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
		Start-Sleep -Milliseconds 150
		return $true
	}
	return $false
}

function Focus-Game {
	$p = Get-GameProcess
	return Focus-ProcessWindow $p
}

function Press-Down {
	[ObvrWaterRun]::PressExtended(0x28, 0x50)
	Start-Sleep -Milliseconds 250
}

function Press-Enter {
	[ObvrWaterRun]::Press(0x0D, 0x1C)
	Start-Sleep -Milliseconds 500
}

function Press-Right {
	[ObvrWaterRun]::PressExtended(0x27, 0x4D)
	Start-Sleep -Milliseconds 250
}

function Click-LauncherPlay($launcher) {
	if (-not (Focus-ProcessWindow $launcher)) { return $false }
	# The image-only Play control has no button notification. Locate its measured
	# child rectangle and send a real foreground mouse click to its center.
	[ObvrWaterRun]::ClickLauncherPlay($launcher.MainWindowHandle)
	Start-Sleep -Seconds 2
	return $true
}

function Save-ScreenBmp([string]$path) {
	$bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
	$bmp = New-Object System.Drawing.Bitmap(
		$bounds.Width, $bounds.Height,
		[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$gfx = [System.Drawing.Graphics]::FromImage($bmp)
	try {
		$gfx.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bmp.Size)
		$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
	} finally {
		$gfx.Dispose()
		$bmp.Dispose()
	}
	Write-Host "Captured $([IO.Path]::GetFileName($path))"
}

$pluginIni = Join-Path $GameDir "Data\OBSE\Plugins\OBVR.ini"
if (-not (Test-Path -LiteralPath $pluginIni)) {
	throw "OBVR.ini not found: $pluginIni"
}
$originalPluginIni = [IO.File]::ReadAllBytes($pluginIni)
# Attach must only accept markers emitted after this invocation. LastWriteTime
# alone cannot distinguish an old successful sweep from a new unfinished one.
$logStartLength = 0
if ($Attach -and -not $ArmBeforeLoad -and (Test-Path -LiteralPath $logPath)) {
	$logStartLength = (Get-Content -LiteralPath $logPath -Raw).Length
}
function Read-CurrentRunLog {
	if (-not (Test-Path -LiteralPath $logPath)) { return "" }
	$text = [string](Get-Content -LiteralPath $logPath -Raw)
	if ($text.Length -lt $logStartLength) {
		throw "OBVR.log was truncated during attach; current run evidence is ambiguous."
	}
	return $text.Substring($logStartLength)
}
try {
	$testIni = [IO.File]::ReadAllText($pluginIni)
	foreach ($setting in @(
		@("VRTestSuite", "1"),
		@("VRTestWaterOnly", "1"),
		@("VRTestWaterCoverageDiagnostic", "0"),
		@("VRTestWaterPitch", $PitchDegrees.ToString([Globalization.CultureInfo]::InvariantCulture)),
		@("StableWaterReflections", "1"),
		@("WaterReflectionMode", "2")
	)) {
		$key, $value = $setting
		$pattern = "(?m)^" + [regex]::Escape($key) + "\s*=.*$"
		if ([regex]::IsMatch($testIni, $pattern)) {
			$testIni = [regex]::Replace($testIni, $pattern, "$key=$value")
		} elseif ($key -like "VRTest*") {
			$debugPattern = "(?m)^\[Debug\]\s*$"
			if (-not [regex]::IsMatch($testIni, $debugPattern)) {
				throw "OBVR.ini has no [Debug] section for $key"
			}
			$testIni = [regex]::Replace($testIni, $debugPattern, "[Debug]`r`n$key=$value", 1)
		} else {
			throw "Required OBVR.ini setting is missing: $key"
		}
	}
	Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-VRTest-water-view-*.bmp" -File -ErrorAction SilentlyContinue |
		Remove-Item -Force
	Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-VRTest-first-world-*.bmp" -File -ErrorAction SilentlyContinue |
		Remove-Item -Force
	# Keep replay disabled while loading/settling. The armed text is written
	# only after LoadWaitSec; otherwise the sweep can finish during that wait.
	$settlingIni = [regex]::Replace($testIni, '(?m)^VRTestSuite\s*=.*$', 'VRTestSuite=0')
	$initialIni = if ($ArmBeforeLoad) { $testIni } else { $settlingIni }
	[IO.File]::WriteAllText($pluginIni, $initialIni, [Text.UTF8Encoding]::new($false))

$startedAt = Get-Date
if (-not $Attach) {
	Write-Host "Starting $loaderPath"
	Start-Process -FilePath $loaderPath -WorkingDirectory $GameDir | Out-Null
} else {
	Write-Host "Attaching to the running Oblivion.exe session."
}

# Wait for the current run's plugin startup, not an old marker in a retained log.
$deadline = (Get-Date).AddSeconds(150)
$startedMarker = $false
$testArmed = $false
$launcherPlaySent = $false
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 2
	if ($Attach) {
		$readyProcess = Get-GameProcess
		if ($readyProcess -and $readyProcess.MainWindowHandle -ne 0) {
			# Config reload and VR-test installation run from world frames. At the
			# main menu there are only HUD invocations, so waiting for the armed
			# marker here deadlocks before the runner can load the test save.
			$startedMarker = $true
			break
		}
		continue
	}
	if (Test-Path -LiteralPath $logPath) {
		$logItem = Get-Item -LiteralPath $logPath
		if ($logItem.LastWriteTime -ge $startedAt) {
			# Startup can emit more than 120 lines before the first poll.
			$tail = Get-Content -LiteralPath $logPath -ErrorAction SilentlyContinue
			if ($tail -match "^OBVR ready$") {
				$readyProcess = Get-GameProcess
				if ($readyProcess -and $readyProcess.MainWindowHandle -ne 0) {
					$startedMarker = $true
					break
				}
			}
		}
	}
	$p = Get-GameProcess
	if (-not $p) {
		if ($Attach) { continue }
		$launcher = Get-LauncherProcess
		if ($launcher) {
			throw "OBSE redirected to OblivionLauncher; refusing launcher fallback. Start obse_loader.exe from its game-folder Explorer window and use -Attach."
		}
		continue
	}
	Focus-Game | Out-Null
}
if (-not $startedMarker) {
	$launcher = Get-LauncherProcess
	if ($launcher) {
		throw "The loader left OblivionLauncher open but the game process never appeared."
	}
	throw "The current run never produced a fresh OBVR startup marker."
}
Start-Sleep -Seconds 3
if (-not $AlreadyInWorld -and -not (Focus-Game)) { throw "Oblivion has no focusable window." }

if (-not $AlreadyInWorld) {
	# Observed main menu is horizontal: Down establishes Continue focus, then
	# two Right presses select Load. Further Down presses do not move selection.
	# The save list starts at its top
	# entry, so SaveIndex=1 selects the second entry from the top.
	Write-Host "Selecting Load, then save entry $($SaveIndex + 1) from the top..."
	Press-Down
	Press-Right
	Press-Right
	Press-Enter
	Start-Sleep -Seconds 2
	for ($i = 0; $i -lt $SaveIndex; ++$i) { Press-Down }
	Press-Enter
	# Main-menu load confirmation defaults to Yes (the in-world dialog does not).
	Start-Sleep -Seconds 1
	Press-Enter
} else {
	Write-Host "Using the already-loaded world; no menu input will be sent."
}
Write-Host "Waiting $LoadWaitSec seconds for the world and water to settle..."
Start-Sleep -Seconds $LoadWaitSec
if (-not (Get-GameProcess)) { throw "Oblivion stopped before capture." }
if (-not $AlreadyInWorld) { Focus-Game | Out-Null }

# Discard pre-arm evidence, including any sweep retained by an attached game.
# This applies equally to a fresh launch, main-menu attach and in-world attach.
if (-not $ArmBeforeLoad) {
	$logStartLength = if (Test-Path -LiteralPath $logPath) {
		([string](Get-Content -LiteralPath $logPath -Raw)).Length
	} else { 0 }
	[IO.File]::WriteAllText($pluginIni, $testIni, [Text.UTF8Encoding]::new($false))
}

if (-not $testArmed) {
	Write-Host "Waiting for the in-world config reload to arm the water runner..."
	$armDeadline = (Get-Date).AddSeconds(60)
	while ((Get-Date) -lt $armDeadline) {
		if (-not (Get-GameProcess)) { throw "Oblivion stopped before the water runner armed." }
		if (Test-Path -LiteralPath $logPath) {
			$logItem = Get-Item -LiteralPath $logPath
			if ($logItem.LastWriteTime -ge $startedAt) {
				$tail = Read-CurrentRunLog
				if ($tail -match "VRTEST water runner armed schema=21") {
					$testArmed = $true
					break
				}
			}
		}
		Start-Sleep -Seconds 2
	}
	if (-not $testArmed) {
		throw "The in-world config reload never armed the water runner."
	}
}

Write-Host "Waiting for the automatic -60 to +60 degree HMD water sweep..."
$sweepDeadline = (Get-Date).AddSeconds(180)
$sweepStatus = $null
while ((Get-Date) -lt $sweepDeadline) {
	Start-Sleep -Seconds 2
	if (-not (Get-GameProcess)) { throw "Oblivion stopped during the water sweep." }
	if (-not (Test-Path -LiteralPath $logPath)) { continue }
	$tail = Read-CurrentRunLog
	$statusMatch = [regex]::Match(($tail -join "`n"),
		"VRTEST water-sweep status=(pass|fail)")
	if ($statusMatch.Success) {
		$sweepStatus = $statusMatch.Groups[1].Value
		break
	}
}
$captured = @(Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-VRTest-water-view-*.bmp" -File |
	Sort-Object Name)
$shots = @()
foreach ($shot in $captured) {
	Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $ArtifactDir $shot.Name) -Force
	$shots += $shot.Name
}
$firstWorld = @(Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-VRTest-first-world-*.bmp" -File |
	Where-Object { $_.LastWriteTime -ge $startedAt } | Sort-Object Name)
foreach ($shot in $firstWorld) {
	Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $ArtifactDir $shot.Name) -Force
}

# Supplementary native-target evidence, separate from the 28 eye-image gate.
# Match this run's timestamps so an older DLL cannot contribute stale textures.
Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-VRTest-reflection-view-*.bmp" -File |
	Where-Object { $_.LastWriteTime -ge $startedAt } |
	Copy-Item -Destination $ArtifactDir -Force

if (Test-Path -LiteralPath $logPath) {
	Copy-Item -LiteralPath $logPath -Destination (Join-Path $ArtifactDir "OBVR.log") -Force
}
$manifest = [ordered]@{
	schema = 2
	attach = [bool]$Attach
	alreadyInWorld = [bool]$AlreadyInWorld
	armBeforeLoad = [bool]$ArmBeforeLoad
	waitForManualLaunch = [bool]$WaitForManualLaunch
	saveIndex = $SaveIndex
	saveEntry = $targetSave.Name
	saveTimestamp = $targetSave.LastWriteTime.ToString("o")
	captureCount = $shots.Count
	sweep = "synthetic-hmd-yaw--60-to-60"
	pitchDegrees = $PitchDegrees
	files = $shots
	firstWorldFiles = @($firstWorld | ForEach-Object Name)
	startedAt = $startedAt.ToString("o")
	completedAt = (Get-Date).ToString("o")
}
$manifest | ConvertTo-Json -Depth 4 |
	Set-Content -LiteralPath (Join-Path $ArtifactDir "water-vr-result.json") -Encoding UTF8

# Preserve failed/partial evidence before reporting the same failure gates.
if (-not $sweepStatus) { throw "The automatic water sweep did not finish before timeout." }
if ($sweepStatus -ne "pass") { throw "The in-game water matrix/capture sweep failed." }
if ($captured.Count -ne 28) {
	throw "Expected 28 synthetic-yaw eye screenshots, found $($captured.Count)."
}

$python = "C:\Users\Nadi\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
$harness = Join-Path $root "tools\water_visual_harness.py"
$json = Join-Path $ArtifactDir "water-vr-analysis.json"
if (Test-Path -LiteralPath $python) {
	& $python $harness --source-root $root --artifact-dir $ArtifactDir --json $json
	$analysisExit = $LASTEXITCODE
	Write-Host "Harness exit code: $analysisExit"
} else {
	Write-Warning "Bundled Python was not found; evidence was captured but not analyzed."
	$analysisExit = 2
}

if (-not $KeepGameOpen) {
	$p = Get-GameProcess
	if ($p) {
		$p.CloseMainWindow() | Out-Null
		if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
	}
}
if ($analysisExit -ne 0) {
	throw "Water visual harness did not pass; inspect $ArtifactDir"
}
Write-Host "Water VR harness passed: $ArtifactDir"
} finally {
	[IO.File]::WriteAllBytes($pluginIni, $originalPluginIni)
	Write-Host "Restored OBVR.ini after water run."
}
