# The smoke run for the aim set at the source: loads the last save from the
# main menu by keyboard, draws the weapon with Ready Item, attacks three
# times with the attack control held long enough for a bow to draw and
# loose, casts twice with the cast key, closes the
# game and prints the lines the wrapped calls wrote. Nothing is ever saved:
# loading and closing touch no save file.
#
# What the run answers: whether the key handler hook went in (the vtable
# slot held what this build expects), and whether a bow release, a cast and
# a swing pass through the wrapped call without incident. With SteamVR up
# but the headset asleep the head reads level, so the swap writes the same
# heading it read: this checks the wrapping, not the aim; the aim is the
# wearer.s to check.
#
# Requires: SteamVR running (a sleeping headset is enough), and no Oblivion
# already running - a session someone is playing is never touched.

param(
	[int]$MenuWaitSec = 150,
	[int]$LoadWaitSec = 60
)

$ErrorActionPreference = "Stop"
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$logPath = Join-Path $gameDir "OBVR.log"

if (Get-Process Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; this harness never touches a live session."
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrAimSource {
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
	public static void Click(int holdMs) {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dwFlags = 2; // MOUSEEVENTF_LEFTDOWN
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
		System.Threading.Thread.Sleep(holdMs);
		one[0].mi.dwFlags = 4; // MOUSEEVENTF_LEFTUP
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
}
"@

function Focus-Oblivion {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrAimSource]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
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
	[ObvrAimSource]::Press(0x1B, 0x01)
}
if (-not $menuSeen) { Write-Host "Main menu marker never appeared - continuing blind." }
Start-Sleep -Seconds 5

Focus-Oblivion
Write-Host "Pressing Down, Enter for Continue, Enter for the Yes box..."
[ObvrAimSource]::PressExtended(0x28, 0x50)
Start-Sleep -Milliseconds 800
[ObvrAimSource]::Press(0x0D, 0x1C)
Start-Sleep -Seconds 3
[ObvrAimSource]::Press(0x0D, 0x1C)

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

# Ready Item is F in the user's Oblivion.ini (scancode 0x21), the attack
# control the left mouse button, Cast is C (scancode 0x2E).
Focus-Oblivion
Write-Host "Drawing the weapon (F)..."
[ObvrAimSource]::Press(0x46, 0x21)
Start-Sleep -Seconds 6
for ($i = 1; $i -le 3; ++$i) {
	Write-Host "Attack $i (held, so a bow draws and looses)..."
	Focus-Oblivion
	[ObvrAimSource]::Click(700)
	Start-Sleep -Seconds 3
}
Write-Host "Casting (C)..."
Focus-Oblivion
[ObvrAimSource]::Press(0x43, 0x2E)
Start-Sleep -Seconds 3
Write-Host "Casting again (C)..."
Focus-Oblivion
[ObvrAimSource]::Press(0x43, 0x2E)
Start-Sleep -Seconds 3

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}

Start-Sleep -Seconds 2
Write-Host "--- the wrapped calls ---"
if (Test-Path $logPath) {
	$lines = Select-String -Path $logPath -Pattern "Aim: .*(wrapped|does not hold|stub|at the source)|Config: .*AimAtSource"
	if ($lines) { $lines | ForEach-Object { $_.Line } } else { Write-Host "(none)" }
}
Write-Host "--- last lines ---"
if (Test-Path $logPath) { Get-Content $logPath -Tail 8 }
Write-Host "Done."
