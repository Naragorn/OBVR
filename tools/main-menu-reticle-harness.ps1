# Full runtime acceptance harness for the main-menu sneak-eye regression.
# It starts through the verified Explorer/OBSE path, dismisses onboarding at
# the known native-menu hit, captures a screenshot, and checks the pixels.

param(
	[int]$MenuWaitSec = 150,
	[string]$OutFile = "D:\Modding\OBVR\artifacts\main-menu-reticle-current.png"
)

$ErrorActionPreference = "Stop"
$shot = Join-Path $PSScriptRoot "cursor-shot.ps1"
$check = Join-Path $PSScriptRoot "check-main-menu-reticle.ps1"

# WindowShift=0 keeps the measured eye region inside the desktop capture.
& $shot -MenuWaitSec $MenuWaitSec -WindowShift 0 -CursorX 1500 -CursorY 1200 -Click -RequireMainMenu -OutFile $OutFile
& $check -ImagePath $OutFile
Write-Host "Main-menu reticle harness passed: $OutFile"
