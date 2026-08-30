# Automated measuring run for the cursor offset.
#
# Starts Oblivion through the xOBSE loader, waits until OBVR's cursor probe
# reports from the main menu, feeds slow synthetic mouse movement so the
# probe lines carry different positions, then closes the game and prints
# every probe line the run produced. No clicks are ever sent: the main menu
# is only looked at and pointed across, so the run can touch no save.
#
# Requires: SteamVR running, the headset present, Debug.CursorProbe=1 in the
# deployed OBVR.ini, and no Oblivion already running - a session someone is
# playing is never touched.

param(
	[int]$MenuWaitSec = 120,
	[int]$MovePhases = 10,
	[int]$SettleMs = 3000,
	[switch]$NoInput,
	# The click experiment: put the game's own cursor on the Continue button
	# while the OS cursor stays clamped above it, click, and let the log say
	# whether a load started - that answers which of the two cursors the
	# click follows. If nothing loads, the window is pushed upward so the OS
	# cursor can reach the same button, and the click is tried again.
	[switch]$ClickExperiment
)

$ErrorActionPreference = "Stop"
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$logPath = Join-Path $gameDir "OBVR.log"

if (Get-Process Oblivion -ErrorAction SilentlyContinue) {
	throw "Oblivion is already running; this harness never touches a live session."
}

# Relative mouse movement through SendInput. DirectInput on modern Windows is
# fed from the same injected stream, but that is a claim the run itself
# verifies: if the probe's position never changes, the injection did not
# arrive, and the log says so by standing still.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrMouse {
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
	[StructLayout(LayoutKind.Sequential)]
	public struct RECT { public int Left, Top, Right, Bottom; }
	[DllImport("user32.dll")]
	public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
	[DllImport("user32.dll")]
	public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y,
	                                       int cx, int cy, uint flags);
	public static void LeftClick() {
		INPUT[] one = new INPUT[1];
		one[0].type = 0;
		one[0].mi.dwFlags = 2; // MOUSEEVENTF_LEFTDOWN
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
		System.Threading.Thread.Sleep(80);
		one[0].mi.dwFlags = 4; // MOUSEEVENTF_LEFTUP
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
	public static void Move(int dx, int dy) {
		INPUT[] one = new INPUT[1];
		one[0].type = 0; // INPUT_MOUSE
		one[0].mi.dx = dx;
		one[0].mi.dy = dy;
		one[0].mi.dwFlags = 1; // MOUSEEVENTF_MOVE, relative
		SendInput(1, one, Marshal.SizeOf(typeof(INPUT)));
	}
	public static void PressEscape() {
		keybd_event(0x1B, 0x01, 0, IntPtr.Zero);       // ESC down, scancode 1
		System.Threading.Thread.Sleep(60);
		keybd_event(0x1B, 0x01, 2, IntPtr.Zero);       // KEYEVENTF_KEYUP
	}
}
"@

function Focus-Oblivion {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrMouse]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
}

$logLengthBefore = 0
if (Test-Path $logPath) {
	$logLengthBefore = (Get-Item $logPath).Length
}

Write-Host "Starting Oblivion through obse_loader..."
Start-Process -FilePath (Join-Path $gameDir "obse_loader.exe") -WorkingDirectory $gameDir

# The cold start spends its first half minute in the intro movies, during
# which the 2D pass never runs. The pass's first run installs the device
# hooks and says so in the log - that line is the "main menu reached"
# marker. Escape is pressed on the way to skip the movies, which also
# proves whether injected keyboard input reaches the game at all.
$deadline = (Get-Date).AddSeconds($MenuWaitSec)
$menuSeen = $false
$logAlive = $false
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	if (-not (Test-Path $logPath)) { continue }
	$item = Get-Item $logPath
	if ($item.Length -eq $logLengthBefore -and -not $logAlive) { continue }
	$logAlive = $true
	if (Select-String -Path $logPath -Pattern "hooked at table entries" -Quiet) {
		$menuSeen = $true
		break
	}
	if (-not $NoInput) {
		Focus-Oblivion
		[ObvrMouse]::PressEscape()
	}
}

$oblivion = Get-Process Oblivion -ErrorAction SilentlyContinue
if (-not $menuSeen) {
	Write-Host "No cursor probe line within $MenuWaitSec s - collecting what there is."
} elseif (-not $NoInput) {
	# Let the menu settle, then walk the cursor: first up-left into a known
	# corner, then downward in slow steps so consecutive probe lines carry a
	# descending position. Each phase rests longer than the probe cadence.
	# The game window is brought to the foreground first - a window started
	# from a background job spawns without focus, and unfocused input goes
	# to whatever has it instead.
	Start-Sleep -Seconds 5
	Focus-Oblivion
	Start-Sleep -Milliseconds 500

	if ($ClickExperiment) {
		function Test-GameChanged {
			(Select-String -Path $logPath -Quiet -Pattern "world renders this frame=1 \(scene call [1-9]") -or
			(-not (Get-Process Oblivion -ErrorAction SilentlyContinue))
		}

		# Phase A: the game cursor walks down the menu column and clicks at
		# each rung; the OS cursor stays clamped at the monitor's bottom
		# edge, above every button. Any menu reaction proves the click
		# follows the game cursor.
		Write-Host "Click ladder A: game cursor down the menu column"
		[ObvrMouse]::Move(-8000, -8000)
		Start-Sleep -Milliseconds 1500
		[ObvrMouse]::Move(2047, 1250)
		Start-Sleep -Milliseconds 1500
		$reacted = $false
		for ($y = 1250; $y -le 2250 -and -not $reacted; $y += 100) {
			[ObvrMouse]::LeftClick()
			Start-Sleep -Milliseconds 2500
			if (Test-GameChanged) {
				Write-Host "REACTION after game-cursor click at derived y=$y"
				$reacted = $true
				break
			}
			[ObvrMouse]::Move(0, 100)
			Start-Sleep -Milliseconds 800
		}

		# Phase B, only if A moved nothing: push the window up so the OS
		# cursor can reach the button column, then click down a screen
		# ladder. A reaction here proves the click follows the OS cursor.
		if (-not $reacted -and (Get-Process Oblivion -ErrorAction SilentlyContinue)) {
			Write-Host "Click ladder B: window pushed up, OS cursor on the column"
			$p = Get-Process Oblivion
			[ObvrMouse]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, -900, 0, 0,
			                          0x0015) | Out-Null  # NOSIZE|NOZORDER|NOACTIVATE
			Start-Sleep -Milliseconds 1000
			Focus-Oblivion
			for ($sy = 400; $sy -le 1350 -and -not $reacted; $sy += 100) {
				[ObvrMouse]::SetCursorPos(2047, $sy) | Out-Null
				Start-Sleep -Milliseconds 1200
				[ObvrMouse]::LeftClick()
				Start-Sleep -Milliseconds 2500
				if (Test-GameChanged) {
					Write-Host "REACTION after OS-cursor click at screen y=$sy (window at -900)"
					$reacted = $true
				}
			}
		}
		if (-not $reacted) {
			Write-Host "No reaction from either ladder."
		}
	} else {

	# Phase 1: relative movement, which drives the game's own cursor (the
	# probe's pos). A short walk so the run proves the injection arrived.
	Write-Host "Feeding relative mouse movement ($MovePhases phases)..."
	[ObvrMouse]::Move(-4000, -4000)
	Start-Sleep -Milliseconds $SettleMs
	for ($i = 0; $i -lt $MovePhases; ++$i) {
		[ObvrMouse]::Move(200, 300)
		Start-Sleep -Milliseconds $SettleMs
	}

	# Phase 2: the split experiment. The OS cursor is placed ABSOLUTELY on a
	# ladder of window positions while no relative movement runs, so the
	# game's own cursor stands still. If the probe's activeTile changes in
	# this phase although pos does not, the hit test follows the OS cursor
	# and not the game cursor - the split the offset hunt is looking for.
	$oblivion = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($oblivion -and $oblivion.MainWindowHandle -ne 0) {
		$rect = New-Object ObvrMouse+RECT
		[ObvrMouse]::GetWindowRect($oblivion.MainWindowHandle, [ref]$rect) | Out-Null
		Write-Host ("Window rect: {0},{1}..{2},{3}" -f $rect.Left, $rect.Top, $rect.Right, $rect.Bottom)
		$centerX = [Math]::Floor(($rect.Left + $rect.Right) / 2)
		foreach ($y in 300, 700, 1100, 1400, 1742, 2100) {
			$screenY = $rect.Top + $y
			Write-Host "OS cursor to window y=$y (screen $centerX,$screenY)"
			[ObvrMouse]::SetCursorPos($centerX, $screenY) | Out-Null
			Start-Sleep -Milliseconds $SettleMs
		}
	}
	}
}

# Close the run: the loader's game process, asked politely first. This only
# ever reaches a process this script started - a running session was refused
# at the top.
$oblivion = Get-Process Oblivion -ErrorAction SilentlyContinue
if ($oblivion) {
	Write-Host "Closing Oblivion..."
	$oblivion.CloseMainWindow() | Out-Null
	if (-not $oblivion.WaitForExit(15000)) {
		Stop-Process -Id $oblivion.Id -Force
	}
}

Start-Sleep -Seconds 2
Write-Host "--- probe lines of this run ---"
if (Test-Path $logPath) {
	Select-String -Path $logPath -Pattern "Cursor probe|Cursor draw probe|Hud viewport|hooked at table|UiSize|Resolution: the game asked" |
		ForEach-Object { $_.Line }
}
