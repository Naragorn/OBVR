[CmdletBinding()]
param(
	[string]$GameDir = 'D:\SteamLibrary\steamapps\common\Oblivion',
	[switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$gameDirPath = (Resolve-Path -LiteralPath $GameDir).Path.TrimEnd('\')
$loaderPath = Join-Path $gameDirPath 'obse_loader.exe'
if (-not (Test-Path -LiteralPath $loaderPath -PathType Leaf)) {
	throw "OBSE loader not found: $loaderPath"
}
if (Get-Process -Name Oblivion -ErrorAction SilentlyContinue) {
	throw 'Oblivion.exe is already running.'
}

$shell = New-Object -ComObject Shell.Application
$expectedUri = ([Uri]$gameDirPath).AbsoluteUri.TrimEnd('/')
$windows = @($shell.Windows() | Where-Object {
	try {
		$fullName = [string]$_.FullName
		$location = ([Uri][string]$_.LocationURL).AbsoluteUri.TrimEnd('/')
		[IO.Path]::GetFileName($fullName) -ieq 'explorer.exe' -and
			$location -ieq $expectedUri
	} catch { $false }
})
if ($windows.Count -ne 1) {
	throw "Expected exactly one Explorer window at $gameDirPath; found $($windows.Count)."
}
$window = $windows[0]
$item = $window.Document.Folder.ParseName('obse_loader.exe')
if ($null -eq $item -or [string]$item.Path -ine $loaderPath) {
	throw 'Explorer did not resolve the exact OBSE loader path.'
}
if ($DryRun) {
	Write-Output "Would invoke Explorer's open verb for: $loaderPath"
	exit 0
}

# Calling the selected Explorer item's open verb follows the same Shell launch
# path as pressing Enter, without relying on SetForegroundWindow. Windows can
# legally refuse foreground activation for a background test process.
$item.InvokeVerb('open')
$deadline = (Get-Date).AddSeconds(8)
while ((Get-Date) -lt $deadline) {
	if (Get-Process -Name Oblivion -ErrorAction SilentlyContinue) {
		Write-Output "Launched through Explorer: $loaderPath"
		exit 0
	}
	Start-Sleep -Milliseconds 500
}
if (Get-Process -Name OblivionLauncher -ErrorAction SilentlyContinue) {
	throw 'Explorer activation opened OblivionLauncher instead of Oblivion.exe.'
}

# Some Explorer builds expose InvokeVerb but ignore it for this executable.
# Fall back to selecting the verified item and pressing Enter. Attach the
# current input queue to the existing foreground queue so Windows permits the
# Explorer window to become foreground before the key is sent.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ObvrExplorerLaunch {
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, IntPtr process);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from, uint to, bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, IntPtr extra);
    public static bool FocusAndPressEnter(IntPtr target) {
        IntPtr foreground = GetForegroundWindow();
        uint foregroundThread = GetWindowThreadProcessId(foreground, IntPtr.Zero);
        uint currentThread = GetCurrentThreadId();
        bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
                        AttachThreadInput(currentThread, foregroundThread, true);
        try {
            BringWindowToTop(target);
            SetForegroundWindow(target);
        } finally {
            if (attached) AttachThreadInput(currentThread, foregroundThread, false);
        }
        if (GetForegroundWindow() != target) return false;
        keybd_event(0x0D, 0x1C, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(90);
        keybd_event(0x0D, 0x1C, 2, IntPtr.Zero);
        return true;
    }
}
'@
# SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_ENSUREVISIBLE | SVSI_FOCUSED.
$window.Document.SelectItem($item, 0x1 -bor 0x4 -bor 0x8 -bor 0x10)
$handle = [IntPtr][int64]$window.HWND
if (-not [ObvrExplorerLaunch]::FocusAndPressEnter($handle)) {
	throw 'Explorer could not become foreground; no keyboard input was sent.'
}
$deadline = (Get-Date).AddSeconds(30)
while ((Get-Date) -lt $deadline) {
	if (Get-Process -Name Oblivion -ErrorAction SilentlyContinue) {
		Write-Output "Launched through Explorer keyboard fallback: $loaderPath"
		exit 0
	}
	Start-Sleep -Milliseconds 500
}
if (Get-Process -Name OblivionLauncher -ErrorAction SilentlyContinue) {
	throw 'Explorer keyboard activation opened OblivionLauncher instead of Oblivion.exe.'
}
throw 'Explorer activation did not produce Oblivion.exe.'
