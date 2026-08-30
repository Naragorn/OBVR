# The modal-dialog click experiment. Continue without the mod manager pops a
# "content is missing" message box - modal, reproducible, its buttons centred
# in the believed space. Perfect target: this script walks the game cursor
# down a ladder over the box and clicks at each rung (phase A), then walks
# the OS cursor over the box's VISIBLE position and clicks again (phase B),
# photographing after every click. Whichever rung makes the box vanish names
# the coordinate space the click really lives in. Nothing here can save.

param(
	[int]$MenuWaitSec = 150,
	[string]$OutPrefix = "$env:TEMP\obvr-dlg"
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
public static class ObvrDlg {
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
	public static extern bool SetCursorPos(int x, int y);
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
		[ObvrDlg]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
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
	[ObvrDlg]::Press(0x1B, 0x01)
}
Start-Sleep -Seconds 5

# Raise the message box: Down gives Continue the keyboard focus, Enter
# activates it, and without the mod manager the missing-content box appears.
Focus-Oblivion
[ObvrDlg]::PressExtended(0x28, 0x50)
Start-Sleep -Milliseconds 800
[ObvrDlg]::Press(0x0D, 0x1C)
Start-Sleep -Seconds 6
Save-Shot "$OutPrefix-box.png"

# Phase A: game-cursor ladder over the centred box, one click per rung.
$rung = 0
foreach ($y in 1100, 1200, 1300, 1400) {
	[ObvrDlg]::Move(-8000, -8000)
	Start-Sleep -Milliseconds 1200
	[ObvrDlg]::Move(2014, $y)
	Start-Sleep -Milliseconds 2000
	[ObvrDlg]::LeftClick()
	Start-Sleep -Milliseconds 2000
	++$rung
	Save-Shot "$OutPrefix-a$rung-y$y.png"
}

# Phase B: OS-cursor ladder over the box's VISIBLE position (backbuffer y
# times 2266/3380 - the window squash - plus the caption's 26 pixels).
$rung = 0
foreach ($y in 763, 830, 897, 964) {
	[ObvrDlg]::SetCursorPos(2014, $y) | Out-Null
	Start-Sleep -Milliseconds 1200
	[ObvrDlg]::LeftClick()
	Start-Sleep -Milliseconds 2000
	++$rung
	Save-Shot "$OutPrefix-b$rung-y$y.png"
}

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}

Start-Sleep -Seconds 2
Write-Host "--- probe lines ---"
if (Test-Path $logPath) {
	Select-String -Path $logPath -Pattern "Cursor probe" |
		Select-Object -Last 12 | ForEach-Object { $_.Line }
}
Write-Host "Done."
