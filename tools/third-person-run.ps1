# The measuring run for the third person camera: loads the last save from the
# main menu by keyboard, switches to third person with the Toggle POV key,
# tilts the view up and down with synthetic mouse movement while the third
# person probe writes its columns, turns it once sideways, closes the game
# and prints the probe lines. Expects the probe to be present in the deployed
# INI; it is switched on for the run and off again afterwards. Nothing is
# ever saved: loading and closing touch no save file.
#
# What the run answers: how the engine swings its third person camera as the
# player's rotX changes - the arm from the feet, and the point it turns
# about - and whether the camera node's local position is the world position
# there. Those are the numbers a head-aimed third person needs before it can
# write the pitch without carrying the viewpoint along.
#
# Requires: SteamVR running (a sleeping headset is enough), and no Oblivion
# already running - a session someone is playing is never touched.

param(
	[int]$MenuWaitSec = 150,
	[int]$LoadWaitSec = 60,
	# One phase is a burst of small relative mouse steps followed by a pause
	# long enough for the probe to write several lines at the new tilt.
	[int]$TiltSteps = 12,
	[int]$TiltStepPixels = 25,
	[int]$SettleMs = 1200
)

$ErrorActionPreference = "Stop"
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$logPath = Join-Path $gameDir "OBVR.log"
$iniPath = Join-Path $gameDir "Data\OBSE\Plugins\OBVR.ini"

if (Get-Process Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; this harness never touches a live session."
}

function Set-Probe([string]$value) {
	if (-not (Test-Path $iniPath)) { return }
	$ini = Get-Content $iniPath
	$ini -replace '^ThirdPersonProbe=.*', "ThirdPersonProbe=$value" | Set-Content $iniPath -Encoding utf8
}
Set-Probe "1"
try {

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrThirdPerson {
	[StructLayout(LayoutKind.Sequential)]
	struct INPUT { public uint type; public MOUSEINPUT mi; }
	[StructLayout(LayoutKind.Sequential)]
	struct MOUSEINPUT {
		public int dx; public int dy; public uint mouseData;
		public uint dwFlags; public uint time; public IntPtr dwExtraInfo;
	}
	[DllImport("user32.dll", SetLastError = true)]
	static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
	[DllImport("user32.dll")]
	public static extern bool SetForegroundWindow(IntPtr hWnd);
	[DllImport("user32.dll")]
	static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, IntPtr dwExtraInfo);
	public static void Press(byte vk, byte scan) {
		keybd_event(vk, scan, 0, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(vk, scan, 2, IntPtr.Zero);
	}
	public static void PressExtended(byte vk, byte scan) {
		keybd_event(vk, scan, 1, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(vk, scan, 3, IntPtr.Zero);
	}
	public static void Move(int dx, int dy) {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dx = dx;
		one[0].mi.dy = dy;
		one[0].mi.dwFlags = 1; // MOUSEEVENTF_MOVE, relative
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
}
"@

function Focus-Oblivion {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrThirdPerson]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
}

# A tilt phase: the steps are small and spaced so DirectInput sees a sweep
# rather than one jump, and the pause after them is where the probe lines
# with the new rotX land.
function Tilt([int]$dx, [int]$dy, [string]$label) {
	Write-Host "Mouse: $label"
	for ($i = 0; $i -lt $TiltSteps; ++$i) {
		[ObvrThirdPerson]::Move($dx, $dy)
		Start-Sleep -Milliseconds 40
	}
	Start-Sleep -Milliseconds $SettleMs
}

$logLengthBefore = 0
if (Test-Path $logPath) { $logLengthBefore = (Get-Item $logPath).Length }

Write-Host "Starting Oblivion..."
Start-Process -FilePath (Join-Path $gameDir "obse_loader.exe") -WorkingDirectory $gameDir

$deadline = (Get-Date).AddSeconds($MenuWaitSec)
$menuSeen = $false
$logAlive = $false
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	if (-not (Test-Path $logPath)) { continue }
	if ((Get-Item $logPath).Length -eq $logLengthBefore -and -not $logAlive) { continue }
	$logAlive = $true
	if (Select-String -Path $logPath -Pattern "hooked at table entries" -Quiet) {
		$menuSeen = $true
		break
	}
	Focus-Oblivion
	[ObvrThirdPerson]::Press(0x1B, 0x01)
}
if (-not $menuSeen) { Write-Host "Main menu marker never appeared - continuing blind." }
Start-Sleep -Seconds 5

Focus-Oblivion
Write-Host "Pressing Down, Enter for Continue, Enter for the Yes box..."
[ObvrThirdPerson]::PressExtended(0x28, 0x50)
Start-Sleep -Milliseconds 800
[ObvrThirdPerson]::Press(0x0D, 0x1C)
Start-Sleep -Seconds 3
[ObvrThirdPerson]::Press(0x0D, 0x1C)

$worldMark = (Select-String -Path $logPath -Pattern "on a world frame Oblivion draws").Count
$loadDeadline = (Get-Date).AddSeconds($LoadWaitSec)
$worldSeen = $false
while ((Get-Date) -lt $loadDeadline) {
	Start-Sleep -Seconds 2
	if ((Select-String -Path $logPath -Pattern "on a world frame Oblivion draws").Count -gt $worldMark) {
		$worldSeen = $true
		break
	}
}
if ($worldSeen) {
	Write-Host "The world is being drawn - the save is loaded."
	Start-Sleep -Seconds 3
} else {
	Write-Host "No world-frame line within $LoadWaitSec s - continuing blind."
}

# Third person, with a receipt: the camera hook logs the switch, and a press
# that left no line is pressed again. R is Toggle POV in the user's
# Oblivion.ini (Toggle POV=001302FF: scancode 0x13).
$povMark = (Select-String -Path $logPath -Pattern "switched to third person").Count
for ($try = 0; $try -lt 4; ++$try) {
	Focus-Oblivion
	Start-Sleep -Milliseconds 600
	Write-Host "Toggling to third person (try $($try+1))..."
	[ObvrThirdPerson]::Press(0x52, 0x13)
	Start-Sleep -Seconds 2
	if ((Select-String -Path $logPath -Pattern "switched to third person").Count -gt $povMark) {
		Write-Host "Third person."
		break
	}
}

# The sweep. Up, further up, back through level and down, back to level:
# each pause is several probe lines at one tilt, and the columns across the
# pauses are the camera's answer to rotX. Then once sideways, so the arm's
# dependence on rotZ is in the same log.
Start-Sleep -Seconds 2
Tilt 0 (-$TiltStepPixels) "tilt up"
Tilt 0 (-$TiltStepPixels) "tilt further up"
Tilt 0 $TiltStepPixels "back towards level"
Tilt 0 $TiltStepPixels "level"
Tilt 0 $TiltStepPixels "tilt down"
Tilt 0 $TiltStepPixels "tilt further down"
Tilt 0 (-$TiltStepPixels) "back towards level"
Tilt 0 (-$TiltStepPixels) "level"
Tilt (2 * $TiltStepPixels) 0 "turn right"
Tilt (2 * $TiltStepPixels) 0 "turn further right"
Tilt (-2 * $TiltStepPixels) 0 "turn back"
Tilt (-2 * $TiltStepPixels) 0 "turn back to start"

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}

Start-Sleep -Seconds 2
Write-Host "--- probe lines (the measurement) ---"
if (Test-Path $logPath) {
	$probe = Select-String -Path $logPath -Pattern "Third person probe"
	if ($probe) { $probe | ForEach-Object { $_.Line } } else { Write-Host "(none - the probe never fired)" }
}
Write-Host "--- camera context ---"
if (Test-Path $logPath) {
	Select-String -Path $logPath -Pattern "switched to (third|first) person|Aim:" |
		Select-Object -Last 12 | ForEach-Object { $_.Line }
}
Write-Host "Done."
} finally {
	Set-Probe "0"
	Write-Host "Probe switched back off."
}
