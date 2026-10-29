# Writes the loading stage (src/vr/LoadingStage.h) into assets/stage, two domes:
#   loading_dome_bright.*  while the menus are what the player sees - the start and the
#                          main menu: parchment above and below, gold along the horizon
#   loading_dome_dark.*    once a world has been drawn: nearly black, a warm sepia glow
#                          along the horizon
# each as
#   .obj  a sphere of 20 m around the playspace's origin, faced inwards
#   .mtl  its one material, the texture as diffuse map
#   .png  the texture, a lat-long gradient
#
# OBVR's own pictures, not the game's: Bethesda's textures are not shipped. The glow sits
# in the texture's middle row and the halves are near each other in tone, so the picture
# reads the same whichever way SteamVR takes the texture's v. The bright dome stays warm
# and muted, never white: a bright flash in the headset dazzles.
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

function Mix($a, $b, [double]$t) {
	$t = [Math]::Max(0.0, [Math]::Min(1.0, $t))
	return @(
		[int]($a[0] + ($b[0] - $a[0]) * $t),
		[int]($a[1] + ($b[1] - $a[1]) * $t),
		[int]($a[2] + ($b[2] - $a[2]) * $t))
}

# The texture: 512 x 256, one colour per row.
function Write-Texture([string]$path, $palette) {
	$width = 512
	$height = 256
	$bitmap = New-Object System.Drawing.Bitmap $width, $height
	for ($y = 0; $y -lt $height; $y++) {
		$v = ($y + 0.5) / $height                    # 0 at the zenith, 0.5 the horizon, 1 the nadir
		$fromHorizon = [Math]::Abs($v - 0.5) * 2.0   # 0 at the horizon, 1 at either pole
		$band = [Math]::Exp(-[Math]::Pow($fromHorizon / $palette.GlowWidth, 2.0))
		if ($v -lt 0.5) {
			$base = Mix $palette.Sky $palette.Zenith ([Math]::Pow($fromHorizon, 0.7))
		} else {
			$base = Mix $palette.Floor $palette.Nadir ([Math]::Pow($fromHorizon, 0.7))
		}
		$c = Mix $base $palette.Glow $band
		$color = [System.Drawing.Color]::FromArgb(255, $c[0], $c[1], $c[2])
		for ($x = 0; $x -lt $width; $x++) {
			$bitmap.SetPixel($x, $y, $color)
		}
	}
	$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
	$bitmap.Dispose()
}

# The sphere, y up, metres; the triangles wound to face the centre.
function Write-Dome([string]$name, $palette) {
	Write-Texture (Join-Path $OutDir "$name.png") $palette

	$mtl = @(
		"# OBVR's loading stage (tools/make-loading-stage.ps1)",
		"newmtl dome",
		"Ka 1.000 1.000 1.000",
		"Kd 1.000 1.000 1.000",
		"Ks 0.000 0.000 0.000",
		"illum 1",
		"map_Kd $name.png")
	Set-Content -Path (Join-Path $OutDir "$name.mtl") -Value $mtl -Encoding ascii

	$lines = New-Object System.Collections.Generic.List[string]
	$lines.Add("# OBVR's loading stage (tools/make-loading-stage.ps1): a $RadiusMetres m sphere faced inwards")
	$lines.Add("mtllib $name.mtl")
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
	Set-Content -Path (Join-Path $OutDir "$name.obj") -Value $lines -Encoding ascii
	Write-Output "Wrote $OutDir\$name.obj ($($lines.Count) lines), .mtl and .png"
}

# Nearly black, warm; the horizon the menus' sepia, dimmed.
Write-Dome "loading_dome_dark" @{
	Zenith = @(6, 5, 4); Sky = @(16, 11, 7); Glow = @(70, 50, 26)
	Floor = @(12, 9, 6); Nadir = @(5, 4, 3); GlowWidth = 0.12
}
# The main menu's parchment, muted: never white.
Write-Dome "loading_dome_bright" @{
	Zenith = @(120, 100, 70); Sky = @(160, 134, 94); Glow = @(205, 172, 112)
	Floor = @(140, 116, 80); Nadir = @(100, 82, 56); GlowWidth = 0.18
}
