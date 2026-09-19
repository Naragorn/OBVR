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
	Write-Output "Would launch through Explorer HWND $($window.HWND): $loaderPath"
	exit 0
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ObvrExplorerLaunch {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, IntPtr extra);
    public static void PressEnter() {
        keybd_event(0x0D, 0x1C, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(90);
        keybd_event(0x0D, 0x1C, 2, IntPtr.Zero);
    }
}
'@

# SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_ENSUREVISIBLE | SVSI_FOCUSED.
$window.Document.SelectItem($item, 0x1 -bor 0x4 -bor 0x8 -bor 0x10)
$handle = [IntPtr][int64]$window.HWND
[ObvrExplorerLaunch]::SetForegroundWindow($handle) | Out-Null
Start-Sleep -Milliseconds 300
if ([ObvrExplorerLaunch]::GetForegroundWindow() -ne $handle) {
	throw 'The Oblivion Explorer window did not become foreground; no input was sent.'
}
[ObvrExplorerLaunch]::PressEnter()
Write-Output "Launched through Explorer: $loaderPath"
