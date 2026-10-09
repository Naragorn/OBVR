# Runs a Full VR scenario in the game with nobody in the headset.
#
# The scenario is a hand script (src/test/HandScript.h): the controllers, and
# if wanted the head, played from a text file. This runner
#   1. writes OBVR-test.ini beside OBVR.dll - [Debug] HandScript plus the
#      scenario's own "ini" lines - so the player's OBVR.ini is never touched,
#   2. starts the game through xOBSE, continues the latest save,
#   3. follows OBVR.log: on every "HandScript: mark <name>" it takes pictures
#      (the game window, SteamVR's headset view window when one is open),
#   4. stops the game when the script has finished, and
#   5. checks the scenario's "expect" and "reject" lines against the run's log.
# OBVR.log and OBVR.log.prev as they were before the run are put back
# afterwards, so the player's last session stays readable; the run's own log
# is kept with its pictures under artifacts\hand-script\<scenario>\<time>.
#
# Needs: SteamVR running with the headset connected (it may sleep - the
# controllers may be off), the desktop unlocked, Oblivion not running.

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$Script,
	[string]$GameDir = "C:\Steam\steamapps\common\Oblivion",
	[string]$ArtifactRoot,
	[int]$StartTimeoutSec = 240,
	[int]$RunTimeoutSec = 300,
	[int]$ShotWidth = 1600,
	# The composited dump is seen from where the real headset lies; on the
	# desk on 2026-09-27 it lay rolled a quarter turn, so the picture is
	# turned back by this many degrees (0, 90, 180, 270).
	[ValidateSet(0, 90, 180, 270)][int]$DumpRotate = 270,
	[switch]$FullScreenShots,
	# The save the run starts from, by name, loaded from the main menu's
	# console. The player's own last save changes whenever they play
	# (2026-09-27: every holster run failed on an over-encumbered autosave
	# with a tutorial box up), so scenarios start from their own save,
	# written by make-harness-save.txt. Empty or "continue": continue the last save.
	[string]$Save = "OBVRHarness",
	[switch]$KeepGameOpen
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$scriptPath = (Resolve-Path -LiteralPath $Script).Path
$scenario = [IO.Path]::GetFileNameWithoutExtension($scriptPath)
if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
	$ArtifactRoot = Join-Path $root "artifacts\hand-script"
}
$runDir = Join-Path (Join-Path $ArtifactRoot $scenario) (Get-Date -Format "yyyyMMdd-HHmmss")
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

$pluginDir = Join-Path $GameDir "Data\OBSE\Plugins"
$testIni = Join-Path $pluginDir "OBVR-test.ini"
$scriptTarget = Join-Path $pluginDir "OBVR-HandScript.txt"
$logPath = Join-Path $GameDir "OBVR.log"
$prevPath = Join-Path $GameDir "OBVR.log.prev"

if (Get-Process -Name Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; the runner starts its own session."
}
if (-not (Get-Process -Name vrserver -ErrorAction SilentlyContinue)) {
	Write-Host "Starting SteamVR..."
	Start-Process (Join-Path (Split-Path $GameDir -Parent) "SteamVR\bin\win64\vrstartup.exe")
	Start-Sleep -Seconds 25
	if (-not (Get-Process -Name vrserver -ErrorAction SilentlyContinue)) {
		throw "SteamVR did not start."
	}
}

# The scenario's lines for the runner.
$expects = @()
$rejects = @()
$iniLines = @{}
$consoleLines = @()
# Console lines typed when a mark is reached ("console-at <mark> <line>"),
# {near} replaced by the form ID of the item nearest the eyes from that
# mark's "HandScript: items" line.
$consoleAt = @{}
$counts = @()
# Marks at which a window of another process is put in front of the game and
# kept there ("background-at <mark>"): from then on the runner no longer
# brings the game to the front.
$backgroundAt = @()
# How far the player got between two marks, from the marks' "HandScript:
# player at" lines: "moved <a> <b> <units>" (at least), "still <a> <b>
# <units>" (at most), "turned <a> <b> <degrees>" (at least).
$walkChecks = @()
foreach ($raw in [IO.File]::ReadAllLines($scriptPath)) {
	$line = ($raw -replace "#.*$", "").Trim()
	if ($line -match "^expect\s+(.+)$") { $expects += $Matches[1].Trim() }
	elseif ($line -match "^reject\s+(.+)$") { $rejects += $Matches[1].Trim() }
	elseif ($line -match "^background-at\s+(\S+)$") { $backgroundAt += $Matches[1] }
	elseif ($line -match "^(moved|still|turned)\s+(\S+)\s+(\S+)\s+([0-9]+(?:\.[0-9]+)?)$") {
		$walkChecks += ,@($Matches[1], $Matches[2], $Matches[3], [double]$Matches[4])
	}
	elseif ($line -match "^console-at\s+(\S+)\s+(.+)$") {
		if (-not $consoleAt.ContainsKey($Matches[1])) { $consoleAt[$Matches[1]] = @() }
		$consoleAt[$Matches[1]] += $Matches[2].Trim()
	}
	elseif ($line -match "^console\s+(.+)$") { $consoleLines += $Matches[1].Trim() }
	elseif ($line -match "^count\s+(\d+)\s+(.+)$") { $counts += ,@([int]$Matches[1], $Matches[2].Trim()) }
	elseif ($line -match "^ini\s+(\S+)\s+(\S+=\S*)$") {
		if (-not $iniLines.ContainsKey($Matches[1])) { $iniLines[$Matches[1]] = @() }
		$iniLines[$Matches[1]] += $Matches[2]
	}
}

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public static class ObvrHandRun {
	[DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, IntPtr extra);
	[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
	[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
	[DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
	[DllImport("user32.dll")] static extern bool AttachThreadInput(uint from, uint to, bool attach);
	[DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr handle);
	// Puts a window in front from a process that does not own the front:
	// joined to the front thread's input for the call, as Windows allows.
	public static bool BringToFront(IntPtr target) {
		uint ignored;
		uint frontThread = GetWindowThreadProcessId(GetForegroundWindow(), out ignored);
		uint current = GetCurrentThreadId();
		bool attached = frontThread != 0 && frontThread != current && AttachThreadInput(current, frontThread, true);
		try {
			BringWindowToTop(target);
			SetForegroundWindow(target);
		} finally {
			if (attached) AttachThreadInput(current, frontThread, false);
		}
		return GetForegroundWindow() == target;
	}
	[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr handle);
	[DllImport("user32.dll")] public static extern bool IsIconic(IntPtr handle);
	[DllImport("user32.dll")] static extern int GetWindowText(IntPtr handle, StringBuilder text, int max);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr handle, out uint pid);
	[DllImport("user32.dll")] static extern bool PrintWindow(IntPtr handle, IntPtr hdc, uint flags);
	public delegate bool EnumProc(IntPtr handle, IntPtr parameter);
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
	[StructLayout(LayoutKind.Sequential)]
	public struct RECT { public int left; public int top; public int right; public int bottom; }
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr handle, out RECT rect);
	[DllImport("user32.dll")] static extern void mouse_event(uint flags, int dx, int dy, uint data, IntPtr extra);
	[DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint mapType);
	// Left and right button up, and every key the hands use (HandKeyMap's
	// defaults and the hotkeys 1-8) up: an up for a key that is not down
	// does nothing.
	[DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
	public static void ReleaseAll() {
		// Only a button that is down: a right-button up on its own opens the
		// desktop's context menu (seen 2026-09-27).
		if ((GetAsyncKeyState(0x01) & 0x8000) != 0) mouse_event(0x0004, 0, 0, 0, IntPtr.Zero);
		if ((GetAsyncKeyState(0x02) & 0x8000) != 0) mouse_event(0x0010, 0, 0, 0, IntPtr.Zero);
		// Virtual key and the US scan code OBVR sends it by (KeyScanCodes.h):
		// up by scan code, as it went down, and by the layout's own code.
		byte[,] keys = { {0x20, 0x39}, {0x5A, 0x2C}, {0x45, 0x12}, {0x11, 0x1D}, {0x10, 0x2A}, {0x46, 0x21},
		                 {0x09, 0x0F}, {0x1B, 0x01}, {0x70, 0x3B}, {0x52, 0x13}, {0x57, 0x11}, {0x41, 0x1E},
		                 {0x53, 0x1F}, {0x44, 0x20}, {0x43, 0x2E}, {0x31, 0x02}, {0x32, 0x03}, {0x33, 0x04},
		                 {0x34, 0x05}, {0x35, 0x06}, {0x36, 0x07}, {0x37, 0x08}, {0x38, 0x09}, {0x54, 0x14} };
		for (int i = 0; i < keys.GetLength(0); ++i) {
			keybd_event(keys[i, 0], keys[i, 1], 8u | 2u, IntPtr.Zero);
			keybd_event(keys[i, 0], (byte)MapVirtualKey(keys[i, 0], 0), 2u, IntPtr.Zero);
		}
	}
	public static void Press(byte key, byte scan, bool extended) {
		uint ext = extended ? 1u : 0u;
		keybd_event(key, scan, ext, IntPtr.Zero);
		System.Threading.Thread.Sleep(70);
		keybd_event(key, scan, ext | 2u, IntPtr.Zero);
	}
	public static string Title(IntPtr handle) {
		StringBuilder text = new StringBuilder(256);
		GetWindowText(handle, text, 256);
		return text.ToString();
	}
	public static uint Pid(IntPtr handle) { uint pid; GetWindowThreadProcessId(handle, out pid); return pid; }
	public static List<IntPtr> Windows() {
		List<IntPtr> all = new List<IntPtr>();
		EnumWindows(delegate(IntPtr h, IntPtr p) { if (IsWindowVisible(h)) all.Add(h); return true; }, IntPtr.Zero);
		return all;
	}
	// The picture without the black a window has where it reaches past the
	// monitors: the right and bottom edges pulled in to the last lit column
	// and row.
	public static System.Drawing.Rectangle LitArea(System.Drawing.Bitmap bmp) {
		int right = 0, bottom = 0;
		for (int y = 0; y < bmp.Height; y += 2) {
			for (int x = 0; x < bmp.Width; x += 2) {
				System.Drawing.Color c = bmp.GetPixel(x, y);
				if (c.R + c.G + c.B > 24) {
					if (x > right) right = x;
					if (y > bottom) bottom = y;
				}
			}
		}
		return new System.Drawing.Rectangle(0, 0, Math.Max(64, right + 2), Math.Max(64, bottom + 2));
	}
	// Text as the game's console reads it: by the key's scan code, named for
	// a US keyboard whatever the layout (2026-09-27: typed by this layout's
	// keys, "player" came out "plazer" on a German one).
	const string UsRows = "1234567890-=qwertyuiop[]asdfghjkl;'zxcvbnm,./";
	static readonly byte[] UsScans = {
		0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,
		0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,
		0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
		0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35 };
	public static void Type(string text) {
		foreach (char raw in text) {
			bool shift = char.IsUpper(raw) || raw == '_';
			char ch = raw == '_' ? '-' : char.ToLowerInvariant(raw);
			byte scan;
			if (ch == ' ') scan = 0x39;
			else {
				int at = UsRows.IndexOf(ch);
				if (at < 0) continue;
				scan = UsScans[at];
			}
			if (shift) keybd_event(0, 0x2A, 8, IntPtr.Zero);
			keybd_event(0, scan, 8, IntPtr.Zero);
			System.Threading.Thread.Sleep(35);
			keybd_event(0, scan, 8 | 2, IntPtr.Zero);
			if (shift) keybd_event(0, 0x2A, 8 | 2, IntPtr.Zero);
			System.Threading.Thread.Sleep(35);
		}
	}
	public static bool Print(IntPtr handle, IntPtr hdc) { return PrintWindow(handle, hdc, 2); }
	// SteamVR's dump of an idle headset's frame is dim (its brightest pixel
	// near 26 of 255 on 2026-09-27): stretched so the brightest is white.
	public static void Stretch(System.Drawing.Bitmap bmp) {
		System.Drawing.Rectangle all = new System.Drawing.Rectangle(0, 0, bmp.Width, bmp.Height);
		System.Drawing.Imaging.BitmapData data = bmp.LockBits(all,
			System.Drawing.Imaging.ImageLockMode.ReadWrite,
			System.Drawing.Imaging.PixelFormat.Format24bppRgb);
		int bytes = Math.Abs(data.Stride) * bmp.Height;
		byte[] px = new byte[bytes];
		Marshal.Copy(data.Scan0, px, 0, bytes);
		int max = 1;
		for (int i = 0; i < bytes; i++) if (px[i] > max) max = px[i];
		for (int i = 0; i < bytes; i++) px[i] = (byte)Math.Min(255, px[i] * 255 / max);
		Marshal.Copy(px, 0, data.Scan0, bytes);
		bmp.UnlockBits(data);
	}
}
"@

function Get-Game { Get-Process -Name Oblivion -ErrorAction SilentlyContinue | Select-Object -First 1 }

$background = $false
$thief = $null
function Focus-Game {
	# In a scenario's background part the game stays behind the other window.
	if ($script:background) { return $false }
	$p = Get-Game
	if ($p -and $p.MainWindowHandle -ne 0) {
		if ([ObvrHandRun]::GetForegroundWindow() -ne $p.MainWindowHandle) {
			[ObvrHandRun]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
			Start-Sleep -Milliseconds 150
		}
		return $true
	}
	return $false
}

function Save-Scaled([System.Drawing.Bitmap]$bmp, [string]$path) {
	$w = $bmp.Width
	$h = $bmp.Height
	if ($ShotWidth -gt 0 -and $w -gt $ShotWidth) {
		$h = [int]($h * $ShotWidth / $w)
		$w = $ShotWidth
	}
	$out = New-Object System.Drawing.Bitmap($w, $h)
	$g = [System.Drawing.Graphics]::FromImage($out)
	try {
		$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
		$g.DrawImage($bmp, 0, 0, $w, $h)
		$out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
	} finally {
		$g.Dispose()
		$out.Dispose()
	}
}

# A window by its handle: from the screen where it is on top, else through
# PrintWindow (which draws a covered window too).
function Save-Window([IntPtr]$handle, [string]$path, [bool]$fromScreen) {
	$rect = New-Object ObvrHandRun+RECT
	if (-not [ObvrHandRun]::GetWindowRect($handle, [ref]$rect)) { return $false }
	if ($fromScreen) {
		# Only what is on the screen: the game's window reaches past its edges.
		Add-Type -AssemblyName System.Windows.Forms
		$screen = [System.Windows.Forms.SystemInformation]::VirtualScreen
		$rect.left = [Math]::Max($rect.left, $screen.Left)
		$rect.top = [Math]::Max($rect.top, $screen.Top)
		$rect.right = [Math]::Min($rect.right, $screen.Right)
		$rect.bottom = [Math]::Min($rect.bottom, $screen.Bottom)
	}
	$w = $rect.right - $rect.left
	$h = $rect.bottom - $rect.top
	if ($w -lt 64 -or $h -lt 64) { return $false }
	$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	try {
		if ($fromScreen) {
			$g.CopyFromScreen($rect.left, $rect.top, 0, 0, $bmp.Size)
		} else {
			$hdc = $g.GetHdc()
			try { [ObvrHandRun]::Print($handle, $hdc) | Out-Null } finally { $g.ReleaseHdc($hdc) }
		}
		$lit = $bmp.Clone([ObvrHandRun]::LitArea($bmp), $bmp.PixelFormat)
		try { Save-Scaled $lit $path } finally { $lit.Dispose() }
	} finally {
		$g.Dispose()
		$bmp.Dispose()
	}
	return $true
}

function Save-FullScreen([string]$path) {
	Add-Type -AssemblyName System.Windows.Forms
	$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
	$bmp = New-Object System.Drawing.Bitmap($b.Width, $b.Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	try {
		$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
		Save-Scaled $bmp $path
	} finally {
		$g.Dispose()
		$bmp.Dispose()
	}
}

# SteamVR's headset view: a visible window of a SteamVR process whose title
# names it. Listed once into the run folder, so a title that is missed shows.
$script:windowsListed = $false
function Find-HeadsetView {
	$vrPids = @(Get-Process | Where-Object { $_.Name -match "^vr" } | ForEach-Object { [uint32]$_.Id })
	$found = [IntPtr]::Zero
	$listing = @()
	foreach ($h in [ObvrHandRun]::Windows()) {
		$pid_ = [ObvrHandRun]::Pid($h)
		if ($vrPids -contains $pid_) {
			$title = [ObvrHandRun]::Title($h)
			$listing += "$pid_ `"$title`""
			if ($title -match "VR View|Headset|Mirror|Spiegel|Ansicht") { $found = $h }
		}
	}
	$listing | Set-Content -LiteralPath (Join-Path $runDir "steamvr-windows.txt") -Encoding UTF8
	return $found
}

$shotIndex = 0
function Take-Shots([string]$name) {
	$script:shotIndex++
	$stem = "{0:D2}-{1}" -f $script:shotIndex, $name
	$p = Get-Game
	if ($p -and $p.MainWindowHandle -ne 0) {
		Focus-Game | Out-Null
		if (Save-Window $p.MainWindowHandle (Join-Path $runDir "$stem-game.png") $true) {
			Write-Host "  shot $stem-game.png"
		}
	}
	$view = Find-HeadsetView
	if ($view -ne [IntPtr]::Zero) {
		if (Save-Window $view (Join-Path $runDir "$stem-headset.png") $false) {
			Write-Host "  shot $stem-headset.png"
		}
	}
	if ($FullScreenShots) {
		Save-FullScreen (Join-Path $runDir "$stem-screen.png")
	}
}

# A plain window of a process of its own, put in front of the game: what a
# browser or a chat in front looks like to the game. Small and in the top
# left corner, so the pictures still show most of the game.
function Start-FocusThief {
	$form = "Add-Type -AssemblyName System.Windows.Forms; `$f = New-Object Windows.Forms.Form; " +
		"`$f.Text = 'OBVR harness - the game is behind this window'; `$f.TopMost = `$true; " +
		"`$f.StartPosition = 'Manual'; `$f.Left = 0; `$f.Top = 0; `$f.Width = 420; `$f.Height = 120; " +
		"[Windows.Forms.Application]::Run(`$f)"
	$process = Start-Process powershell.exe -ArgumentList "-NoProfile", "-Command", "`"$form`"" -PassThru
	$deadline = (Get-Date).AddSeconds(15)
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Milliseconds 200
		$process.Refresh()
		if ($process.MainWindowHandle -ne 0) { break }
	}
	return $process
}

function Read-Log {
	if (-not (Test-Path -LiteralPath $logPath)) { return @() }
	try {
		$fs = [IO.File]::Open($logPath, "Open", "Read", "ReadWrite")
		$sr = New-Object IO.StreamReader($fs)
		$text = $sr.ReadToEnd()
		$sr.Close()
		return $text -split "`r?`n"
	} catch { return @() }
}

# The player's logs, kept aside and put back at the end.
$savedLog = Join-Path $runDir "player-OBVR.log"
$savedPrev = Join-Path $runDir "player-OBVR.log.prev"
if (Test-Path -LiteralPath $logPath) { Copy-Item -LiteralPath $logPath -Destination $savedLog }
if (Test-Path -LiteralPath $prevPath) { Copy-Item -LiteralPath $prevPath -Destination $savedPrev }

$verdict = "FAIL"
$problems = @()
$marks = @()
$measured = @()
try {
	# The overlay: [Debug] HandScript and the scenario's own lines.
	$sections = @{ "Debug" = @("HandScript=OBVR-HandScript.txt") }
	foreach ($k in $iniLines.Keys) {
		if (-not $sections.ContainsKey($k)) { $sections[$k] = @() }
		$sections[$k] += $iniLines[$k]
	}
	$ini = @("; Written by tools/hand-script-run.ps1 for one run and deleted after it.")
	foreach ($k in $sections.Keys) {
		$ini += "[$k]"
		$ini += $sections[$k]
	}
	[IO.File]::WriteAllLines($testIni, $ini)
	Copy-Item -LiteralPath $testIni -Destination (Join-Path $runDir "OBVR-test.ini")
	Copy-Item -LiteralPath $scriptPath -Destination $scriptTarget -Force
	Copy-Item -LiteralPath $scriptPath -Destination (Join-Path $runDir "script.txt")

	# The old log out of the way (it is saved above and put back after), so
	# every line read below is this run's.
	Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue
	$runStarted = Get-Date

	# Launch through the game folder's Explorer window, as the other runners.
	$shell = New-Object -ComObject Shell.Application
	$explorerOpen = @($shell.Windows() | Where-Object {
		try {
			[IO.Path]::GetFileName([string]$_.FullName) -ieq "explorer.exe" -and
				([Uri][string]$_.LocationURL).AbsoluteUri.TrimEnd('/') -ieq ([Uri]$GameDir).AbsoluteUri.TrimEnd('/')
		} catch { $false }
	})
	if ($explorerOpen.Count -eq 0) {
		Start-Process -FilePath "explorer.exe" -ArgumentList $GameDir
		Start-Sleep -Seconds 3
	}
	# The scenarios' own save: the main menu's Continue takes the newest
	# save file, and the console does not open there - so for this run the
	# harness save is made the newest, and afterwards the oldest again, so
	# the player's own Continue still finds their own last save.
	$harnessSave = @()
	if ($Save -ne "" -and $Save -ne "continue") {
		$saves = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\Oblivion\Saves"
		foreach ($ext in "ess", "obse") {
			$f = Join-Path $saves "$Save.$ext"
			if (Test-Path -LiteralPath $f) {
				$harnessSave += [pscustomobject]@{ Path = $f; Time = (Get-Item -LiteralPath $f).LastWriteTime }
				(Get-Item -LiteralPath $f).LastWriteTime = Get-Date
			} elseif ($ext -eq "ess") {
				throw "No save $Save.ess in $saves - run make-harness-save.txt with -Save continue once."
			}
		}
	}
	& (Join-Path $PSScriptRoot "start-obse-from-explorer.ps1") -GameDir $GameDir

	# Main menu: OBVR's "ready" line, then Continue (Down selects it, Enter).
	$deadline = (Get-Date).AddSeconds($StartTimeoutSec)
	$ready = $false
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Seconds 2
		if (-not (Get-Game)) { continue }
		if ((Read-Log) -match "^OBVR ready$") { $ready = $true; break }
	}
	if (-not $ready) { throw "No 'OBVR ready' line within $StartTimeoutSec s." }
	Start-Sleep -Seconds 4
	Focus-Game | Out-Null
	Take-Shots "main-menu"
	Write-Host "Continuing the latest save$(if ($harnessSave) { " ($Save, made the newest for this run)" })..."
	[ObvrHandRun]::Press(0x28, 0x50, $true)
	Start-Sleep -Milliseconds 400
	[ObvrHandRun]::Press(0x0D, 0x1C, $false)
	# "Continue from your last saved game?" - Yes is selected.
	Start-Sleep -Milliseconds 1500
	Focus-Game | Out-Null
	[ObvrHandRun]::Press(0x0D, 0x1C, $false)

	$deadline = (Get-Date).AddSeconds($StartTimeoutSec)
	$started = $false
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Seconds 1
		Focus-Game | Out-Null
		$log = Read-Log
		if ($log -match "HandScript: WARNING") { throw "The script was refused: $(($log -match 'HandScript: WARNING') -join ' / ')" }
		if ($log -match "HandScript: started") { $started = $true; break }
		if (-not (Get-Game)) { throw "The game ended before the script started." }
	}
	if (-not $started) { throw "The script did not start within $StartTimeoutSec s (no world?)." }
	Write-Host "Script started."

	# The scenario's console lines, typed into the game's console (the key
	# left of 1 opens and closes it), {weapon} replaced by the equipped
	# weapon's form ID from the "started" line. The script's first wait has
	# to leave time for this.
	if ($consoleLines.Count -gt 0) {
		$startLine = @(Read-Log | Where-Object { $_ -match "HandScript: started" }) | Select-Object -First 1
		$weapon = if ($startLine -match "weapon form ([0-9A-F]{8})") { $Matches[1] } else { "" }
		Focus-Game | Out-Null
		[ObvrHandRun]::Press(0xDC, 0x29, $false)
		Start-Sleep -Milliseconds 600
		foreach ($c in $consoleLines) {
			$command = $c.Replace("{weapon}", $weapon)
			Write-Host "Console: $command"
			[ObvrHandRun]::Type($command)
			Start-Sleep -Milliseconds 200
			Take-Shots "console"
			Start-Sleep -Milliseconds 150
			[ObvrHandRun]::Press(0x0D, 0x1C, $false)
			Start-Sleep -Milliseconds 400
		}
		# The console has to have taken the last line before the key that
		# closes it: after a dozen lines it was still busy and the close was
		# lost - the whole run happened with the console open (2026-09-28).
		# And the close checked in the log - a closed menu leaves "a menu just
		# closed" behind - and pressed again when it was lost.
		for ($try = 0; $try -lt 4; $try++) {
			Start-Sleep -Milliseconds 1000
			Focus-Game | Out-Null
			[ObvrHandRun]::Press(0xDC, 0x29, $false)
			Start-Sleep -Milliseconds 1200
			$last = @(Read-Log | Where-Object { $_ -match "Menu trace: a menu just (opened|closed)" }) |
				Select-Object -Last 1
			Write-Host "Console close $($try + 1): $last"
			if ($last -match "just closed") { break }
		}
	}

	$seen = 0
	$finished = $false
	$deadline = (Get-Date).AddSeconds($RunTimeoutSec)
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Milliseconds 200
		Focus-Game | Out-Null
		$lines = @(Read-Log | Where-Object { $_ -match "HandScript: (mark|finished)" })
		for ($i = $seen; $i -lt $lines.Count; $i++) {
			if ($lines[$i] -match "HandScript: mark (\S+)") {
				$marks += $Matches[1]
				Write-Host "Mark $($Matches[1])"
				$markName = $Matches[1]
				Take-Shots $markName
				if ($background) {
					# The game must still be behind at every later mark, else what
					# the marks measured was not the background.
					$p = Get-Game
					if ($p -and [ObvrHandRun]::GetForegroundWindow() -eq $p.MainWindowHandle) {
						$problems += "the game was in front again at mark $markName - the background part is inconclusive"
					}
				}
				if ($backgroundAt -contains $markName -and -not $background) {
					$thief = Start-FocusThief
					$inFront = $thief.MainWindowHandle -ne 0 -and [ObvrHandRun]::BringToFront($thief.MainWindowHandle)
					$background = $true
					Write-Host "Background from mark ${markName}: another window in front $inFront"
					if (-not $inFront) { $problems += "could not put another window in front of the game at mark $markName" }
				}
				if ($consoleAt.ContainsKey($markName)) {
					Start-Sleep -Milliseconds 300
					$itemLine = @(Read-Log | Where-Object { $_ -match "HandScript: items" }) | Select-Object -Last 1
					$near = if ($itemLine -match "nearest to the eyes [0-9A-F]{8} form ([0-9A-F]{8})") { $Matches[1] } else { "" }
					Focus-Game | Out-Null
					[ObvrHandRun]::Press(0xDC, 0x29, $false)
					Start-Sleep -Milliseconds 600
					foreach ($c in $consoleAt[$markName]) {
						$command = $c.Replace("{near}", $near)
						Write-Host "Console at ${markName}: $command"
						[ObvrHandRun]::Type($command)
						Start-Sleep -Milliseconds 200
						Take-Shots "console"
						Start-Sleep -Milliseconds 150
						[ObvrHandRun]::Press(0x0D, 0x1C, $false)
						Start-Sleep -Milliseconds 400
					}
					[ObvrHandRun]::Press(0xDC, 0x29, $false)
					Start-Sleep -Milliseconds 400
				}
			} elseif ($lines[$i] -match "HandScript: finished") {
				$finished = $true
			}
		}
		$seen = $lines.Count
		if ($finished) { break }
		if (-not (Get-Game)) { $problems += "the game ended during the script"; break }
	}
	if (-not $finished -and $problems.Count -eq 0) { $problems += "the script did not finish within $RunTimeoutSec s" }
	Take-Shots "end"

	# SteamVR's dumps ("dump" in the script): the composited frame, overlays
	# included - what the headset would have shown, from where the real
	# headset lies. Brought over small and brightened; the originals (tens of
	# megabytes each) are removed. The "-raw" twin is the eyes as submitted.
	$dumpDir = Join-Path (Split-Path $GameDir -Parent) "SteamVR\screenshots"
	if (Test-Path -LiteralPath $dumpDir) {
		$dumps = @(Get-ChildItem -LiteralPath $dumpDir -Filter "*.png" | Where-Object { $_.LastWriteTime -ge $runStarted })
		foreach ($d in $dumps) {
			$kind = if ($d.BaseName -like "*-raw") { "eyes" } else { "headset" }
			$img = [System.Drawing.Bitmap]::FromFile($d.FullName)
			try {
				$w = [Math]::Min($ShotWidth, $img.Width)
				$small = New-Object System.Drawing.Bitmap($img, $w, [int]($img.Height * $w / $img.Width))
				$small24 = $small.Clone((New-Object System.Drawing.Rectangle(0, 0, $small.Width, $small.Height)), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
				if ($kind -eq "headset") {
					# The right eye alone: the two panels' pictures are turned
					# opposite ways in the dump (2026-09-27), and one eye says
					# what the other does.
					$half = $small24.Clone((New-Object System.Drawing.Rectangle([int]($small24.Width / 2), 0, [int]($small24.Width / 2), $small24.Height)), $small24.PixelFormat)
					$small24.Dispose()
					$small24 = $half
					[ObvrHandRun]::Stretch($small24)
					if ($DumpRotate -ne 0) { $small24.RotateFlip([System.Drawing.RotateFlipType]("Rotate{0}FlipNone" -f $DumpRotate)) }
				}
				$small24.Save((Join-Path $runDir ("dump-{0}-{1}.png" -f $d.LastWriteTime.ToString("HHmmss"), $kind)), [System.Drawing.Imaging.ImageFormat]::Png)
				$small.Dispose()
				$small24.Dispose()
			} finally { $img.Dispose() }
			Remove-Item -LiteralPath $d.FullName -Force
		}
	}

	# OBVR's own pictures at the marks (OBVR-<what>-<mark>.bmp beside
	# Oblivion.exe, e.g. the quick menu's ring as painted): brought over as
	# PNG, the originals removed.
	foreach ($b in @(Get-ChildItem -LiteralPath $GameDir -Filter "OBVR-*.bmp" | Where-Object { $_.LastWriteTime -ge $runStarted })) {
		$img = [System.Drawing.Bitmap]::FromFile($b.FullName)
		try {
			$img.Save((Join-Path $runDir ($b.BaseName + ".png")), [System.Drawing.Imaging.ImageFormat]::Png)
		} finally { $img.Dispose() }
		Remove-Item -LiteralPath $b.FullName -Force
		Write-Host "  picture $($b.BaseName).png"
	}

	$runLog = Read-Log
	foreach ($e in $expects) {
		if (-not ($runLog | Where-Object { $_.Contains($e) })) { $problems += "expected in the log, missing: $e" }
	}
	foreach ($c in $counts) {
		$n = @($runLog | Where-Object { $_.Contains($c[1]) }).Count
		if ($n -ne $c[0]) { $problems += "expected $($c[0]) times in the log, found $($n): $($c[1])" }
	}
	foreach ($r in $rejects) {
		$hits = @($runLog | Where-Object { $_.Contains($r) })
		if ($hits.Count -gt 0) { $problems += "rejected, but in the log: $($hits[0])" }
	}
	# Where the player stood at each mark: the first "HandScript: player at
	# x y z, heading h degrees" after the mark's own line (both are written
	# on the mark's frame).
	$standing = @{}
	$current = $null
	foreach ($l in $runLog) {
		if ($l -match "HandScript: mark (\S+)") { $current = $Matches[1]; continue }
		if ($current -and $l -match "HandScript: player at (-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+), heading (-?[0-9.]+) degrees") {
			if (-not $standing.ContainsKey($current)) {
				$standing[$current] = @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3], [double]$Matches[4])
			}
			$current = $null
		}
	}
	foreach ($c in $walkChecks) {
		$kind = $c[0]; $a = $c[1]; $b = $c[2]; $limit = $c[3]
		if (-not $standing.ContainsKey($a) -or -not $standing.ContainsKey($b)) {
			$problems += "$kind ${a} ${b}: no player line at mark $(if (-not $standing.ContainsKey($a)) { $a } else { $b })"
			continue
		}
		$pa = $standing[$a]; $pb = $standing[$b]
		if ($kind -eq "turned") {
			$turn = [Math]::Abs(((($pb[3] - $pa[3]) % 360) + 540) % 360 - 180)
			$measured += ("turned from {0} to {1}: {2:N1} degrees (at least {3})" -f $a, $b, $turn, $limit)
			if ($turn -lt $limit) { $problems += ("turned {0} {1}: {2:N1} degrees, wanted at least {3}" -f $a, $b, $turn, $limit) }
		} else {
			$d = [Math]::Sqrt(($pb[0] - $pa[0]) * ($pb[0] - $pa[0]) + ($pb[1] - $pa[1]) * ($pb[1] - $pa[1]))
			$measured += ("walked from {0} to {1}: {2:N1} units across the ground ({3} {4})" -f $a, $b, $d, $(if ($kind -eq "moved") { "at least" } else { "at most" }), $limit)
			if ($kind -eq "moved" -and $d -lt $limit) { $problems += ("moved {0} {1}: {2:N1} units, wanted at least {3}" -f $a, $b, $d, $limit) }
			if ($kind -eq "still" -and $d -gt $limit) { $problems += ("still {0} {1}: {2:N1} units, wanted at most {3}" -f $a, $b, $d, $limit) }
		}
	}
	if ($problems.Count -eq 0) { $verdict = "PASS" }
} catch {
	$problems += "runner: $($_.Exception.Message)"
} finally {
	if ($thief -and -not $thief.HasExited) { Stop-Process -Id $thief.Id -Force -ErrorAction SilentlyContinue }
	if (-not $KeepGameOpen) {
		$p = Get-Game
		if ($p) {
			$p.CloseMainWindow() | Out-Null
			if (-not $p.WaitForExit(10000)) { Stop-Process -Id $p.Id -Force }
			Start-Sleep -Seconds 2
		}
		# Whatever the hands held when the game went stays down in Windows (a
		# killed game cannot let go): the right button held by a block stuck
		# for every later run, 2026-09-27. Both buttons and the hands' keys up.
		[ObvrHandRun]::ReleaseAll()
	}
	# The harness save is always left the oldest save there is (and just
	# written by make-harness-save.txt, too), so the player's own Continue
	# never lands in it.
	$savesDir = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\Oblivion\Saves"
	foreach ($ext in "ess", "obse") {
		$f = Join-Path $savesDir "OBVRHarness.$ext"
		if (Test-Path -LiteralPath $f) { (Get-Item -LiteralPath $f).LastWriteTime = Get-Date "2000-01-01" }
	}
	Remove-Item -LiteralPath $testIni -Force -ErrorAction SilentlyContinue
	Remove-Item -LiteralPath $scriptTarget -Force -ErrorAction SilentlyContinue
	if (-not $KeepGameOpen) {
		if (Test-Path -LiteralPath $logPath) { Copy-Item -LiteralPath $logPath -Destination (Join-Path $runDir "OBVR.log") -Force }
		if (Test-Path -LiteralPath $savedLog) { Copy-Item -LiteralPath $savedLog -Destination $logPath -Force }
		if (Test-Path -LiteralPath $savedPrev) { Copy-Item -LiteralPath $savedPrev -Destination $prevPath -Force }
	}
}

$summary = @("# $scenario - $verdict", "", "Run: $runDir", "", "Marks: $($marks -join ', ')", "")
if ($measured.Count -gt 0) {
	$summary += "Measured:"
	$summary += ($measured | ForEach-Object { "- $_" })
	$summary += ""
}
if ($problems.Count -gt 0) {
	$summary += "Problems:"
	$summary += ($problems | ForEach-Object { "- $_" })
} else {
	$summary += "Every expect line was found, no reject line was."
}
$summary | Set-Content -LiteralPath (Join-Path $runDir "summary.md") -Encoding UTF8
$summary | ForEach-Object { Write-Host $_ }
if ($verdict -ne "PASS") { exit 1 }
