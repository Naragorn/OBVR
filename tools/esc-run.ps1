# The in-game half of the measuring loop: loads the last save from the main
# menu by keyboard (Continue is the entry the keyboard reaches with Enter),
# opens the Esc menu, parks the game cursor on the item column, photographs
# the window (highlight and sprite visible side by side), clicks, and
# photographs again - the user's own repro, run without a person. Nothing is
# ever saved: loading and closing touch no save file.

param(
	[int]$MenuWaitSec = 150,
	[int]$LoadWaitSec = 60,
	[int]$WindowShift = -900,
	[int]$ParkX = 2014,
	[int]$ParkY = 1350,
	[string]$OutPrefix = "$env:TEMP\obvr-esc"
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
public static class ObvrEsc {
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
	public static void Move(int dx, int dy) {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dx = dx;
		one[0].mi.dy = dy;
		one[0].mi.dwFlags = 1;
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
	public static void Press(byte vk, byte scan) {
		keybd_event(vk, scan, 0, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(vk, scan, 2, IntPtr.Zero);
	}
	// Arrow keys are extended keys; without the flag they arrive as the
	// numpad and the menu ignores them.
	public static void PressExtended(byte vk, byte scan) {
		keybd_event(vk, scan, 1, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(vk, scan, 3, IntPtr.Zero);
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
		[ObvrEsc]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
}

function Save-Shot([string]$path) {
	$bmp = New-Object System.Drawing.Bitmap(2560, 1440)
	$gfx = [System.Drawing.Graphics]::FromImage($bmp)
	$gfx.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
	$gfx.Dispose()
	$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
	$bmp.Dispose()
	Write-Host "Shot: $path"
}

$logLengthBefore = 0
if (Test-Path $logPath) { $logLengthBefore = (Get-Item $logPath).Length }

Write-Host "Starting Oblivion..."
Start-Process -FilePath (Join-Path $gameDir "obse_loader.exe") -WorkingDirectory $gameDir

# Wait for the main menu (the device hooks' log line), skipping intros.
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
	[ObvrEsc]::Press(0x1B, 0x01)
}
if (-not $menuSeen) { Write-Host "Main menu marker never appeared - continuing blind." }
Start-Sleep -Seconds 5

# Continue: the main menu has no keyboard focus until an arrow key gives it
# one (a bare Enter did nothing, measured). Down once, then Enter.
Focus-Oblivion
Write-Host "Pressing Down, then Enter for Continue..."
[ObvrEsc]::PressExtended(0x28, 0x50)
Start-Sleep -Milliseconds 800
Save-Shot "$OutPrefix-keyfocus.png"
[ObvrEsc]::Press(0x0D, 0x1C)
Start-Sleep -Seconds $LoadWaitSec

# Push the window up so the item column lies inside the monitor either way.
$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p -and $p.MainWindowHandle -ne 0) {
	[ObvrEsc]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, $WindowShift, 0, 0,
	                        0x0015) | Out-Null
	Start-Sleep -Milliseconds 800
	Focus-Oblivion

	Save-Shot "$OutPrefix-loaded.png"

	Write-Host "Opening the Esc menu..."
	[ObvrEsc]::Press(0x1B, 0x01)
	Start-Sleep -Seconds 3

	# Park the game cursor on the item column: clamp into the top-left, then
	# one absolute step (mouse counts land 1:1 in believed pixels, measured).
	[ObvrEsc]::Move(-8000, -8000)
	Start-Sleep -Milliseconds 1500
	[ObvrEsc]::Move($ParkX, $ParkY)
	Start-Sleep -Seconds 4

	Save-Shot "$OutPrefix-hover.png"

	Write-Host "Clicking..."
	[ObvrEsc]::LeftClick()
	Start-Sleep -Seconds 4

	Save-Shot "$OutPrefix-clicked.png"
}

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}

Start-Sleep -Seconds 2
Write-Host "--- probe lines ---"
if (Test-Path $logPath) {
	Select-String -Path $logPath -Pattern "Cursor probe|Cursor draw probe|a menu just opened" |
		Select-Object -Last 20 | ForEach-Object { $_.Line }
}
Write-Host "Done."
