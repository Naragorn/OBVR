[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputPath)

$ErrorActionPreference='Stop'
$games=@(Get-Process Oblivion -ErrorAction Stop)
if($games.Count -ne 1 -or $games[0].MainWindowHandle -eq 0){
	throw 'Expected exactly one focusable Oblivion window.'
}
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ObvrWindowCapture {
 [StructLayout(LayoutKind.Sequential)] public struct RECT {public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct POINT {public int x,y;}
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w,ref POINT p);
}
'@
$rect=New-Object ObvrWindowCapture+RECT
$origin=New-Object ObvrWindowCapture+POINT
$window=$games[0].MainWindowHandle
if(-not [ObvrWindowCapture]::GetClientRect($window,[ref]$rect) -or
   -not [ObvrWindowCapture]::ClientToScreen($window,[ref]$origin)){
	throw 'Could not resolve the Oblivion client rectangle.'
}
$width=$rect.right-$rect.left
$height=$rect.bottom-$rect.top
if($width -le 0 -or $height -le 0){throw "Invalid client size ${width}x${height}."}
$parent=Split-Path -Parent $OutputPath
if($parent){New-Item -ItemType Directory -Force -Path $parent | Out-Null}
$bmp=New-Object Drawing.Bitmap($width,$height,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$graphics=[Drawing.Graphics]::FromImage($bmp)
try {
	$graphics.CopyFromScreen($origin.x,$origin.y,0,0,$bmp.Size)
	$bmp.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)
} finally {
	$graphics.Dispose()
	$bmp.Dispose()
}
Write-Output "Captured ${width}x${height} client at $($origin.x),$($origin.y): $OutputPath"
