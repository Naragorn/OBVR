# Takes an actual look at the main menu: starts the game, skips the intros,
# pushes the oversized window upward so the menu's lower half lies inside the
# monitor, walks the game cursor onto a probe position, and saves a
# screenshot. The picture answers what no log line can: where the buttons and
# the visible cursor actually sit relative to each other.

param(
	[int]$MenuWaitSec = 150,
	[int]$WindowShift = -900,
	[int]$CursorX = 2047,
	[int]$CursorY = 1742,
	[switch]$Click,
	# OS-cursor counter-experiment: place the OS cursor absolutely at this
	# screen position (the game cursor stays parked elsewhere) and click
	# there. Answers whether the click follows the OS cursor's client
	# position rather than the game's own cursor.
	[int]$OsClickX = -1,
	[int]$OsClickY = -1,
	[string]$OutFile = "$env:TEMP\obvr-menu-shot.png"
)

$ErrorActionPreference = "Stop"
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$logPath = Join-Path $gameDir "OBVR.log"

if (Get-Process Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; this harness never touches a live session."
}

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrShot {
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
	[DllImport("user32.dll")]
	public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y,
	                                       int cx, int cy, uint flags);
	[DllImport("user32.dll")]
	public static extern bool SetCursorPos(int x, int y);
	public static void Move(int dx, int dy) {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dx = dx;
		one[0].mi.dy = dy;
		one[0].mi.dwFlags = 1;
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
	public static void PressEscape() {
		keybd_event(0x1B, 0x01, 0, IntPtr.Zero);
		System.Threading.Thread.Sleep(60);
		keybd_event(0x1B, 0x01, 2, IntPtr.Zero);
	}
	public static void LeftClick() {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dwFlags = 2;
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
		System.Threading.Thread.Sleep(80);
		one[0].mi.dwFlags = 4;
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
}
"@

function Focus-Oblivion {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrShot]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
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
	[ObvrShot]::PressEscape()
}
if (-not $menuSeen) { Write-Host "Menu marker never appeared." }

Start-Sleep -Seconds 5
$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p -and $p.MainWindowHandle -ne 0) {
	[ObvrShot]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, $WindowShift, 0, 0,
	                         0x0015) | Out-Null
	Start-Sleep -Milliseconds 800
	Focus-Oblivion

	# Park the game cursor at a known spot: clamp into the top-left corner,
	# then one absolute step to the probe position (mouse counts land 1:1 in
	# believed pixels, measured).
	[ObvrShot]::Move(-8000, -8000)
	Start-Sleep -Milliseconds 1500
	[ObvrShot]::Move($CursorX, $CursorY)
	Start-Sleep -Milliseconds 2500

	if ($Click) {
		Write-Host "Clicking at the parked game-cursor position..."
		[ObvrShot]::LeftClick()
		Start-Sleep -Milliseconds 4000
	}

	if ($OsClickX -ge 0) {
		Write-Host "OS cursor to screen $OsClickX,$OsClickY - then clicking..."
		[ObvrShot]::SetCursorPos($OsClickX, $OsClickY) | Out-Null
		Start-Sleep -Milliseconds 1500
		[ObvrShot]::LeftClick()
		Start-Sleep -Milliseconds 4000
	}

	$bmp = New-Object System.Drawing.Bitmap(2560, 1440)
	$gfx = [System.Drawing.Graphics]::FromImage($bmp)
	$gfx.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
	$gfx.Dispose()
	$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
	$bmp.Dispose()
	Write-Host "Screenshot saved: $OutFile (window shifted $WindowShift, cursor at $CursorX,$CursorY)"
}

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}
Write-Host "Done."
