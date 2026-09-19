param(
	[Parameter(Mandatory=$true)][string]$OutputDir,
	[ValidateRange(2,20)][int]$FramesPerPhase = 6,
	[ValidateRange(20,500)][int]$IntervalMs = 80,
	[ValidateRange(50,1000)][int]$KeyHoldMs = 320
)
$ErrorActionPreference='Stop'
$games=@(Get-Process Oblivion -ErrorAction Stop)
if($games.Count -ne 1 -or $games[0].MainWindowHandle -eq 0){throw 'Expected one focusable Oblivion window'}
$game=$games[0]
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WaterBurstInput {
 [StructLayout(LayoutKind.Sequential)] public struct RECT {public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct POINT {public int x,y;}
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w,ref POINT p);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,IntPtr extra);
}
'@
$window=$game.MainWindowHandle
[WaterBurstInput]::SetForegroundWindow($window) | Out-Null
Start-Sleep -Milliseconds 500
function Assert-Focus {
	if([WaterBurstInput]::GetForegroundWindow() -ne $window){throw 'Lost game focus; refusing input/capture'}
}
function Capture([string]$name) {
	Assert-Focus
	$rect=New-Object WaterBurstInput+RECT
	$origin=New-Object WaterBurstInput+POINT
	if(-not [WaterBurstInput]::GetClientRect($window,[ref]$rect) -or
	   -not [WaterBurstInput]::ClientToScreen($window,[ref]$origin)){throw 'Game client rectangle unavailable'}
	$screen=[System.Windows.Forms.SystemInformation]::VirtualScreen
	$left=[Math]::Max($origin.x,$screen.Left); $top=[Math]::Max($origin.y,$screen.Top)
	$right=[Math]::Min($origin.x+$rect.right,$screen.Right); $bottom=[Math]::Min($origin.y+$rect.bottom,$screen.Bottom)
	if($right -le $left -or $bottom -le $top){throw 'Game client is outside the visible desktop'}
	$bmp=[Drawing.Bitmap]::new($right-$left,$bottom-$top)
	$gfx=[Drawing.Graphics]::FromImage($bmp)
	try {
		$gfx.CopyFromScreen($left,$top,0,0,$bmp.Size)
		$bmp.Save((Join-Path $OutputDir "$name.jpg"),[Drawing.Imaging.ImageFormat]::Jpeg)
	} finally {$gfx.Dispose();$bmp.Dispose()}
}
function Capture-Phase([string]$name) {
	for($i=0;$i -lt $FramesPerPhase;$i++){
		Capture ("{0}-{1:D2}" -f $name,$i)
		Start-Sleep -Milliseconds $IntervalMs
	}
}
function Move-Phase([string]$name,[byte]$key,[byte]$scan) {
	Assert-Focus
	[WaterBurstInput]::keybd_event($key,$scan,0,[IntPtr]::Zero)
	try {
		$deadline=(Get-Date).AddMilliseconds($KeyHoldMs)
		$i=0
		do {
			Capture ("{0}-held-{1:D2}" -f $name,$i); $i++
			Start-Sleep -Milliseconds $IntervalMs
		} while((Get-Date) -lt $deadline)
	} finally {[WaterBurstInput]::keybd_event($key,$scan,2,[IntPtr]::Zero)}
	Capture-Phase "$name-after"
}
Capture-Phase '00-idle'
Move-Phase '01-forward' 0x57 0x11
Move-Phase '02-back' 0x53 0x1F
Move-Phase '03-right' 0x44 0x20
Move-Phase '04-left' 0x41 0x1E
@{
	processId=$game.Id; framesPerPhase=$FramesPerPhase; intervalMs=$IntervalMs
	keyHoldMs=$KeyHoldMs; order=@('idle','forward','back','right','left')
	completedAt=(Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'sequence.json')
