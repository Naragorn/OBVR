# Automated water-reflection run for the second save in the Oblivion save list.
#
# The run starts through obse_loader.exe, selects the second save entry, captures
# the game view while the headset is swept from right to left, and evaluates
# the resulting BMP sequence plus OBVR.log with water_visual_harness.py.
#
# The headset sweep is deliberately physical: the harness controls save loading
# and evidence capture, while SteamVR supplies the pose under test.

[CmdletBinding()]
param(
	[string]$GameDir = "D:\SteamLibrary\steamapps\common\Oblivion",
	[string]$ArtifactDir,
	[int]$SaveIndex = 1,
	[int]$CaptureCount = 8,
	[int]$CaptureIntervalMs = 350,
	[int]$LoadWaitSec = 60,
	[switch]$KeepGameOpen,
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
if (-not (Test-Path -LiteralPath $loaderPath)) {
	throw "OBSE loader not found: $loaderPath"
}
if (Get-Process -Name Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; stop it before this harness."
}
if (-not (Test-Path -LiteralPath $ArtifactDir)) {
	New-Item -ItemType Directory -Path $ArtifactDir -Force | Out-Null
}
Get-ChildItem -LiteralPath $ArtifactDir -Filter "ScreenShot*.bmp" -File -ErrorAction SilentlyContinue |
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
	static extern IntPtr SendMessage(IntPtr handle, uint message, IntPtr wParam, IntPtr lParam);
	public static void ClickWindowOffset(IntPtr handle, int x, int y) {
		int packed = (y << 16) | (x & 0xffff);
		IntPtr point = new IntPtr(packed);
		SendMessage(handle, 0x0201, new IntPtr(1), point);
		System.Threading.Thread.Sleep(120);
		SendMessage(handle, 0x0202, IntPtr.Zero, point);
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

function Click-LauncherPlay($launcher) {
	if (-not (Focus-ProcessWindow $launcher)) { return $false }
	# OblivionLauncher has an image-only Play button; send the click to the
	# parent because the child is a non-notifying Static bitmap.
	[ObvrWaterRun]::ClickWindowOffset($launcher.MainWindowHandle, 325, 122)
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

$startedAt = Get-Date
Write-Host "Starting $loaderPath"
Start-Process -FilePath $loaderPath -WorkingDirectory $GameDir | Out-Null

# Wait for the current run's plugin startup, not an old marker in a retained log.
$deadline = (Get-Date).AddSeconds(150)
$startedMarker = $false
$launcherPlaySent = $false
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 2
	$p = Get-GameProcess
	if (-not $p) {
		$launcher = Get-LauncherProcess
		if ($launcher -and -not $launcherPlaySent) {
			Write-Host "OblivionLauncher is open; clicking its Play button."
			if (Click-LauncherPlay $launcher) {
				$launcherPlaySent = $true
			}
		}
		continue
	}
	if (-not (Test-Path -LiteralPath $logPath)) { continue }
	$logItem = Get-Item -LiteralPath $logPath
	if ($logItem.LastWriteTime -lt $startedAt) { continue }
	$tail = Get-Content -LiteralPath $logPath -Tail 120 -ErrorAction SilentlyContinue
	if ($tail -match "hooked at table entries|capture/projection coupling hook installed") {
		$startedMarker = $true
		break
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
if (-not (Focus-Game)) { throw "Oblivion has no focusable window." }

# Main menu: the first Down gives the menu focus on Continue; three Down
# presses land on Load (Continue, New, Load). The save list starts at its top
# entry, so SaveIndex=1 selects the second entry from the top.
Write-Host "Selecting Load, then save entry $($SaveIndex + 1) from the top..."
Press-Down
Press-Down
Press-Down
Press-Enter
Start-Sleep -Seconds 2
for ($i = 0; $i -lt $SaveIndex; ++$i) { Press-Down }
Press-Enter
Write-Host "Waiting $LoadWaitSec seconds for the world and water to settle..."
Start-Sleep -Seconds $LoadWaitSec
if (-not (Focus-Game)) { throw "Oblivion stopped before capture." }

Write-Host "Headset sweep starts in 3 seconds: sweep your view from RIGHT to LEFT across the pond."
Start-Sleep -Seconds 1
Write-Host "3"
Start-Sleep -Seconds 1
Write-Host "2"
Start-Sleep -Seconds 1
Write-Host "1"
Focus-Game | Out-Null
$shots = @()
for ($i = 0; $i -lt $CaptureCount; ++$i) {
	$name = "ScreenShot{0:D3}.bmp" -f ($i + 1)
	$path = Join-Path $ArtifactDir $name
	Save-ScreenBmp $path
	$shots += $name
	if ($i + 1 -lt $CaptureCount) { Start-Sleep -Milliseconds $CaptureIntervalMs }
}

if (Test-Path -LiteralPath $logPath) {
	Copy-Item -LiteralPath $logPath -Destination (Join-Path $ArtifactDir "OBVR.log") -Force
}
$manifest = [ordered]@{
	schema = 1
	saveIndex = $SaveIndex
	saveEntry = $targetSave.Name
	saveTimestamp = $targetSave.LastWriteTime.ToString("o")
	captureCount = $shots.Count
	captureIntervalMs = $CaptureIntervalMs
	files = $shots
	startedAt = $startedAt.ToString("o")
	completedAt = (Get-Date).ToString("o")
}
$manifest | ConvertTo-Json -Depth 4 |
	Set-Content -LiteralPath (Join-Path $ArtifactDir "water-vr-result.json") -Encoding UTF8

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