# Writes the loading stage (src/vr/LoadingStage.h) into assets/stage:
#   loading_dome.obj  a sphere of 20 m around the playspace's origin, faced inwards
#   loading_dome.mtl  its one material, the texture as diffuse map
#   loading_dome.png  the texture, a lat-long gradient: nearly black above and below,
#                     a warm sepia glow along the horizon (the vanilla menus' colours)
#
# OBVR's own picture, not the game's: Bethesda's textures are not shipped. The glow sits
# in the texture's middle row, so the picture reads the same whichever way SteamVR takes
# the texture's v.
#
#   powershell -ExecutionPolicy Bypass -File tools\make-loading-stage.ps1
param(
	[string]$OutDir = (Join-Path $PSScriptRoot "..\assets\stage"),
	[int]$Segments = 48,
	[int]$Rings = 24,
	[double]$RadiusMetres = 20.0
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$inv = [System.Globalization.CultureInfo]::InvariantCulture
New-Item -ItemType Directory -Force $OutDir | Out-Null

# --- the texture: 512 x 256, one colour per row
function Mix($a, $b, [double]$t) {
	$t = [Math]::Max(0.0, [Math]::Min(1.0, $t))
	return @(
		[int]($a[0] + ($b[0] - $a[0]) * $t),
		[int]($a[1] + ($b[1] - $a[1]) * $t),
		[int]($a[2] + ($b[2] - $a[2]) * $t))
}
$zenith = @(6, 5, 4)       # nearly black, warm
$sky = @(16, 11, 7)        # the sky above the glow
$glow = @(70, 50, 26)      # the horizon: the menus' sepia, dimmed
$floor = @(12, 9, 6)       # below the horizon
$nadir = @(5, 4, 3)
$width = 512
$height = 256
$bitmap = New-Object System.Drawing.Bitmap $width, $height
for ($y = 0; $y -lt $height; $y++) {
	$v = ($y + 0.5) / $height             # 0 at the zenith, 0.5 the horizon, 1 the nadir
	$fromHorizon = [Math]::Abs($v - 0.5) * 2.0   # 0 at the horizon, 1 at either pole
	$band = [Math]::Exp(-[Math]::Pow($fromHorizon / 0.12, 2.0))   # the glow's falloff
	if ($v -lt 0.5) {
		$base = Mix $sky $zenith ([Math]::Pow($fromHorizon, 0.7))
	} else {
		$base = Mix $floor $nadir ([Math]::Pow($fromHorizon, 0.7))
	}
	$c = Mix $base $glow $band
	$color = [System.Drawing.Color]::FromArgb(255, $c[0], $c[1], $c[2])
	for ($x = 0; $x -lt $width; $x++) {
		$bitmap.SetPixel($x, $y, $color)
	}
}
$pngPath = Join-Path $OutDir "loading_dome.png"
$bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

# --- the material
$mtl = @(
	"# OBVR's loading stage (tools/make-loading-stage.ps1)",
	"newmtl dome",
	"Ka 1.000 1.000 1.000",
	"Kd 1.000 1.000 1.000",
	"Ks 0.000 0.000 0.000",
	"illum 1",
	"map_Kd loading_dome.png")
Set-Content -Path (Join-Path $OutDir "loading_dome.mtl") -Value $mtl -Encoding ascii

# --- the sphere, y up, metres; the triangles wound to face the centre
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# OBVR's loading stage (tools/make-loading-stage.ps1): a $RadiusMetres m sphere faced inwards")
$lines.Add("mtllib loading_dome.mtl")
for ($r = 0; $r -le $Rings; $r++) {
	$phi = [Math]::PI * $r / $Rings          # 0 at the top
	for ($s = 0; $s -le $Segments; $s++) {
		$theta = 2.0 * [Math]::PI * $s / $Segments
		$x = $RadiusMetres * [Math]::Sin($phi) * [Math]::Cos($theta)
		$y = $RadiusMetres * [Math]::Cos($phi)
		$z = $RadiusMetres * [Math]::Sin($phi) * [Math]::Sin($theta)
		$lines.Add([string]::Format($inv, "v {0:F4} {1:F4} {2:F4}", $x, $y, $z))
	}
}
for ($r = 0; $r -le $Rings; $r++) {
	for ($s = 0; $s -le $Segments; $s++) {
		$u = $s / $Segments
		$vt = 1.0 - $r / $Rings              # OBJ's v runs up: the top row is 1
		$lines.Add([string]::Format($inv, "vt {0:F5} {1:F5}", $u, $vt))
	}
}
for ($r = 0; $r -le $Rings; $r++) {
	$phi = [Math]::PI * $r / $Rings
	for ($s = 0; $s -le $Segments; $s++) {
		$theta = 2.0 * [Math]::PI * $s / $Segments
		# Inwards, towards the centre.
		$nx = -[Math]::Sin($phi) * [Math]::Cos($theta)
		$ny = -[Math]::Cos($phi)
		$nz = -[Math]::Sin($phi) * [Math]::Sin($theta)
		$lines.Add([string]::Format($inv, "vn {0:F4} {1:F4} {2:F4}", $nx, $ny, $nz))
	}
}
$lines.Add("usemtl dome")
$row = $Segments + 1
for ($r = 0; $r -lt $Rings; $r++) {
	for ($s = 0; $s -lt $Segments; $s++) {
		$a = $r * $row + $s + 1
		$b = $a + 1
		$c = $a + $row
		$d = $c + 1
		# Counter-clockwise seen from inside.
		$lines.Add("f $a/$a/$a $c/$c/$c $b/$b/$b")
		$lines.Add("f $b/$b/$b $c/$c/$c $d/$d/$d")
	}
}
Set-Content -Path (Join-Path $OutDir "loading_dome.obj") -Value $lines -Encoding ascii

Write-Output "Wrote $OutDir\loading_dome.obj ($($lines.Count) lines), .mtl and .png ($width x $height)"
