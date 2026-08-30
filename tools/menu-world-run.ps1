# The measuring run for the live menu background: loads the last save from
# the main menu by keyboard, opens the Esc menu, holds it long enough for the
# menu-world probe to spend its attempts, closes the game, and prints the
# probe lines. Expects Debug.MenuWorldProbe=1 in the deployed INI. Nothing is
# ever saved: loading and closing touch no save file.

param(
	[int]$MenuWaitSec = 150,
	[int]$LoadWaitSec = 60,
	[int]$HoldSec = 15,
	[string]$OutPrefix = "$env:TEMP\obvr-menuworld"
)

$ErrorActionPreference = "Stop"
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$logPath = Join-Path $gameDir "OBVR.log"
$iniPath = Join-Path $gameDir "Data\OBSE\Plugins\OBVR.ini"

if (Get-Process Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; this harness never touches a live session."
}

# The probe is switched on for this run and off again afterwards, so the
# deployed configuration a person picks up is never left measuring. It costs
# five self-initiated renders per menu episode and can paint the world over
# the menu on the monitor - fine for a run nobody is watching, not something
# to leave behind.
function Set-Probe([string]$value) {
	if (-not (Test-Path $iniPath)) { return }
	$ini = Get-Content $iniPath
	$ini -replace '^MenuWorldProbe=.*', "MenuWorldProbe=$value" | Set-Content $iniPath -Encoding utf8
}
Set-Probe "1"
try {

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrMenuWorld {
	[DllImport("user32.dll")]
	public static extern bool SetForegroundWindow(IntPtr hWnd);
	[DllImport("user32.dll")]
	static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, IntPtr dwExtraInfo);
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
}
"@

function Focus-Oblivion {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrMenuWorld]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
}

# A corroborating shot, not the proof. On a cinema frame the back buffer IS
# the picture being shown, so a probe render that draws anything replaces the
# menu on the monitor for that frame - but the budget is spent within a
# handful of frames of the menu opening, far quicker than a screen grab, so
# catching it is luck. The draw count in the log is the measurement; a shot
# that happens to show the world where the menu belongs is a bonus, and one
# that shows the menu proves nothing either way.
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
	[ObvrMenuWorld]::Press(0x1B, 0x01)
}
if (-not $menuSeen) { Write-Host "Main menu marker never appeared - continuing blind." }
Start-Sleep -Seconds 5

# Continue: the main menu has no keyboard focus until an arrow key gives it
# one (a bare Enter did nothing, measured). Down once, Enter, Enter for the
# Yes box.
Focus-Oblivion
Write-Host "Pressing Down, Enter for Continue, Enter for the Yes box..."
[ObvrMenuWorld]::PressExtended(0x28, 0x50)
Start-Sleep -Milliseconds 800
[ObvrMenuWorld]::Press(0x0D, 0x1C)
Start-Sleep -Seconds 3
[ObvrMenuWorld]::Press(0x0D, 0x1C)

# Wait for the world itself rather than for a stopwatch. A load takes as long
# as it takes - a fixed wait that worked on a warm run pressed Esc into the
# loading screen on a cold one, and four presses then found no menu to open.
# The receipt is the world-frame viewport line, which OBVR logs once the first
# frame with a camera pass goes out; counted from the mark so an identical
# line from an earlier run cannot answer for this one.
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

# Escape with a receipt: the menu-opened trace line is the proof the press
# arrived, and a lost press is simply pressed again.
$markBefore = (Select-String -Path $logPath -Pattern "a menu just opened").Count
for ($try = 0; $try -lt 6; ++$try) {
	Focus-Oblivion
	Start-Sleep -Milliseconds 600
	Write-Host "Opening the Esc menu (try $($try+1))..."
	[ObvrMenuWorld]::Press(0x1B, 0x01)
	Start-Sleep -Seconds 3
	if ((Select-String -Path $logPath -Pattern "a menu just opened").Count -gt $markBefore) {
		Write-Host "Menu is open."
		break
	}
}

Write-Host "Holding the menu for $HoldSec seconds..."
# A burst of shots across the hold: the probe spends its attempts on the first
# few menu frames, so a single late shot would miss them.
for ($shot = 0; $shot -lt 6; ++$shot) {
	Save-Shot ("{0}-hold{1}.png" -f $OutPrefix, $shot)
	Start-Sleep -Milliseconds 400
}
Start-Sleep -Seconds $HoldSec

$p = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($p) {
	$p.CloseMainWindow() | Out-Null
	if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
}

Start-Sleep -Seconds 2
# The probe lines first and in full: they are the measurement, and pooling
# them with the trace let a run's dozen trace lines push every one of them out
# of a "last 30" that was meant to show them.
Write-Host "--- probe lines (the measurement) ---"
if (Test-Path $logPath) {
	$probe = Select-String -Path $logPath -Pattern "Menu world probe|Scene graph probe"
	if ($probe) { $probe | ForEach-Object { $_.Line } } else { Write-Host "(none - the probe never fired)" }
}
Write-Host "--- menu trace (context) ---"
if (Test-Path $logPath) {
	Select-String -Path $logPath -Pattern "a menu just (opened|closed)|Menu trace: (stereo|cinema|held)" |
		Select-Object -Last 16 | ForEach-Object { $_.Line }
}
Write-Host "Done."
} finally {
	Set-Probe "0"
	Write-Host "Probe switched back off."
}
