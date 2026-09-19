# Reuse the measured keyboard path from esc-run.ps1 against an existing game.
param([ValidateSet('Escape','Down','Up','Left','Right','Enter','Tab')][string]$Key = 'Escape')
$ErrorActionPreference = 'Stop'
$game = @(Get-Process Oblivion -ErrorAction Stop)
if ($game.Count -ne 1 -or $game[0].MainWindowHandle -eq 0) {
    throw 'Expected one running Oblivion window.'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ObvrInputProbe {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, IntPtr extra);
    public static void Press(byte key, byte scan, bool extended) {
        uint flags = extended ? 1u : 0u;
        keybd_event(key, scan, flags, IntPtr.Zero);
        System.Threading.Thread.Sleep(70);
        keybd_event(key, scan, flags | 2u, IntPtr.Zero);
    }
}
'@
$window = $game[0].MainWindowHandle
[ObvrInputProbe]::SetForegroundWindow($window) | Out-Null
Start-Sleep -Milliseconds 250
if ([ObvrInputProbe]::GetForegroundWindow() -ne $window) {
    throw 'Oblivion did not become foreground; no input sent.'
}
switch ($Key) {
    'Escape' { [ObvrInputProbe]::Press(0x1B, 0x01, $false) }
    'Down' { [ObvrInputProbe]::Press(0x28, 0x50, $true) }
    'Up' { [ObvrInputProbe]::Press(0x26, 0x48, $true) }
    'Left' { [ObvrInputProbe]::Press(0x25, 0x4B, $true) }
    'Right' { [ObvrInputProbe]::Press(0x27, 0x4D, $true) }
    'Enter' { [ObvrInputProbe]::Press(0x0D, 0x1C, $false) }
    'Tab' { [ObvrInputProbe]::Press(0x09, 0x0F, $false) }
}
Write-Output "Sent $Key to Oblivion PID $($game[0].Id); verify the resulting menu visually."
