# Writes the loading stage (src/vr/LoadingStage.h) into assets/stage:
#   loading_dome_dark.*    nearly black, a faint warm nebula and a few pale stars, a sepia
#                          glow along the horizon - the only dome OBVR uses (Nadi, 2026-10-30:
#                          "light dome entfernen"). Write-Dome with $dark = $false still paints
#                          the former bright look (parchment, gold horizon); nothing ships it.
# each as
#   .obj  a sphere of 20 m around the playspace's origin, faced inwards, and a floor disc
#         of 4 m radius just above the origin's floor, with a soft light under the feet
#   .mtl  its one material - one texture for both meshes (SteamVR's render models carry
#         one diffuse texture each; whether a stage takes more was not found, so one atlas)
#   .png  the atlas: the dome's lat-long picture in the top half, the floor in the bottom
#
# Both looks carry the same ornament: a thin double line along the horizon with small
# diamonds between - OBVR's own, as every picture here: Bethesda's textures are not
# shipped. Ordered dithering against banding on the long gradients. The bright look stays
# muted, never white: a bright flash in the headset dazzles.
#
#   powershell -ExecutionPolicy Bypass -File tools\make-loading-stage.ps1
#   (-Size 2048 for a lighter atlas; -Preview writes small previews into -PreviewDir)
param(
	[string]$OutDir = (Join-Path $PSScriptRoot "..\assets\stage"),
	[int]$Size = 4096,
	[int]$Segments = 96,
	[int]$Rings = 48,
	[double]$RadiusMetres = 20.0,
	[double]$FloorRadiusMetres = 4.0,
	[double]$FloorHeightMetres = 0.005,
	[string]$PreviewDir = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class LoadingStageArt {
	// Value noise on a lattice that wraps every 'period' cells in x, so the dome's seam
	// at u = 0/1 does not show.
	static double Hash(int x, int y, int seed) {
		unchecked {
			uint h = (uint)(x * 374761393 + y * 668265263 + seed * 2147483647);
			h = (h ^ (h >> 13)) * 1274126177u;
			h ^= h >> 16;
			return (h & 0xFFFFFF) / (double)0xFFFFFF;
		}
	}
	static double Smooth(double t) { return t * t * (3 - 2 * t); }
	static double Noise(double x, double y, int period, int seed) {
		int x0 = (int)Math.Floor(x), y0 = (int)Math.Floor(y);
		double fx = Smooth(x - x0), fy = Smooth(y - y0);
		int xa = ((x0 % period) + period) % period, xb = (xa + 1) % period;
		double a = Hash(xa, y0, seed), b = Hash(xb, y0, seed);
		double c = Hash(xa, y0 + 1, seed), d = Hash(xb, y0 + 1, seed);
		return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
	}
	// Fractal sum, 0..1, the period doubling with each octave so every octave wraps.
	static double Fbm(double u, double v, int basePeriod, int octaves, int seed) {
		double sum = 0, amp = 0.5, norm = 0;
		int period = basePeriod;
		for (int o = 0; o < octaves; o++) {
			sum += amp * Noise(u * period, v * period, period, seed + o * 17);
			norm += amp;
			amp *= 0.5;
			period *= 2;
		}
		return sum / norm;
	}
	static readonly int[] Bayer = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	static byte Dither(double value, int x, int y) {
		double d = (Bayer[(y & 3) * 4 + (x & 3)] + 0.5) / 16.0 - 0.5;
		int v = (int)Math.Floor(value + 0.5 + d);
		return (byte)(v < 0 ? 0 : v > 255 ? 255 : v);
	}
	static double Mix(double a, double b, double t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return a + (b - a) * t; }

	// The ornament's strength at a point of the dome: two thin lines just above and below
	// the horizon, small diamonds between them, 48 round the circle.
	static double Ornament(double u, double v) {
		double dv = (v - 0.5) * 180.0;           // degrees from the horizon
		double line = Math.Exp(-Math.Pow((Math.Abs(dv) - 1.6) / 0.10, 2));
		double cell = u * 48.0; cell -= Math.Floor(cell);
		double du = Math.Abs(cell - 0.5) * 7.5;  // degrees along the band, about
		double diamond = (Math.Abs(dv) + du) < 1.05 ? 1.0 : 0.0;
		double dot = Math.Exp(-(Math.Pow(dv / 0.35, 2) + Math.Pow((cell < 0.5 ? cell : 1 - cell) * 7.5 / 0.35, 2)));
		return Math.Max(line, Math.Max(diamond * 0.85, dot * 0.6));
	}

	// dark: true for the night look. size: the atlas' width and height.
	public static void Write(string path, int size, bool dark, string previewPath) {
		int w = size, domeH = size / 2;
		var bmp = new Bitmap(w, size, PixelFormat.Format24bppRgb);
		var data = bmp.LockBits(new Rectangle(0, 0, w, size), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
		var row = new byte[data.Stride];
		var rng = new Random(dark ? 7 : 11);

		// Palettes (r, g, b).
		double[] zenith = dark ? new double[] { 5, 5, 7 } : new double[] { 136, 114, 82 };
		double[] sky = dark ? new double[] { 15, 12, 10 } : new double[] { 172, 146, 104 };
		double[] glow = dark ? new double[] { 82, 58, 30 } : new double[] { 214, 182, 124 };
		double[] floor = dark ? new double[] { 11, 9, 7 } : new double[] { 146, 122, 87 };
		double[] ornament = dark ? new double[] { 120, 92, 50 } : new double[] { 112, 84, 50 };
		double[] underFeet = dark ? new double[] { 44, 32, 19 } : new double[] { 200, 172, 122 };

		// Stars for the dark look: about 1400, spread evenly over the sky's sphere (the
		// lat-long picture stretches near the top, so fewer rows up there).
		var stars = new System.Collections.Generic.Dictionary<long, double>();
		if (dark) {
			for (int i = 0; i < 1400; i++) {
				double z = rng.NextDouble() * 0.92 + 0.08;          // sin of the elevation, above 4.6 degrees
				double elev = Math.Asin(z);
				double v = 0.5 - elev / Math.PI;
				double u = rng.NextDouble();
				int sx = (int)(u * w), sy = (int)(v * domeH);
				double b = 0.25 + 0.75 * Math.Pow(rng.NextDouble(), 3);
				int r = b > 0.8 ? 1 : 0;
				for (int yy = -r; yy <= r; yy++)
					for (int xx = -r; xx <= r; xx++) {
						long key = (long)(sy + yy) * w + ((sx + xx + w) % w);
						double fall = (xx == 0 && yy == 0) ? 1.0 : 0.35;
						double old; stars.TryGetValue(key, out old);
						stars[key] = Math.Max(old, b * fall);
					}
			}
		}

		for (int y = 0; y < size; y++) {
			for (int x = 0; x < w; x++) {
				double r, g, bl;
				if (y < domeH) {
					double u = (x + 0.5) / w, v = (y + 0.5) / domeH;   // v: 0 the zenith, 0.5 the horizon
					double fromHorizon = Math.Abs(v - 0.5) * 2.0;
					double band = Math.Exp(-Math.Pow(fromHorizon / (dark ? 0.10 : 0.16), 2));
					double[] baseCol = new double[3];
					for (int c = 0; c < 3; c++)
						baseCol[c] = v < 0.5 ? Mix(sky[c], zenith[c], Math.Pow(fromHorizon, 0.7)) : floor[c];
					// The clouds (bright) or the nebula (dark): large soft shapes, stronger up the sky.
					double cloud = Fbm(u, v * 0.5, 6, 5, dark ? 3 : 5);
					double up = v < 0.5 ? Math.Min(1.0, fromHorizon * 2.0) : 0.25;
					double cloudGain = dark ? 1.0 + (cloud - 0.5) * 0.9 * up : 1.0 + (cloud - 0.5) * 0.16 * up;
					// The bright look's paper: a fine even grain and larger soft stains.
					double fibre = dark ? 0.0
					                    : (Fbm(u, v * 0.5, 256, 3, 9) - 0.5) * 0.07 +
					                          (Fbm(u, v * 0.5, 16, 4, 13) - 0.5) * 0.12;
					r = baseCol[0] * (cloudGain + fibre);
					g = baseCol[1] * (cloudGain + fibre);
					bl = baseCol[2] * (cloudGain + fibre);
					if (dark) {   // the nebula's warm tint where it is thickest
						double warm = Math.Max(0, cloud - 0.52) * up * 40;
						r += warm; g += warm * 0.6; bl += warm * 0.25;
					}
					r = Mix(r, glow[0], band); g = Mix(g, glow[1], band); bl = Mix(bl, glow[2], band);
					double orn = Ornament(u, v) * (dark ? 0.75 : 0.55);
					r = Mix(r, ornament[0], orn); g = Mix(g, ornament[1], orn); bl = Mix(bl, ornament[2], orn);
					double star;
					if (dark && stars.TryGetValue((long)y * w + x, out star)) {
						double s = star * 150;
						r += s; g += s * 0.95; bl += s * 0.85;
					}
				} else {
					// The floor: a disc in the bottom-left square, its middle under the feet.
					int fy = y - domeH;
					int half = domeH;                  // the square's side
					if (x < half) {
						double px = (x + 0.5) / half * 2 - 1, pz = (fy + 0.5) / half * 2 - 1;
						double rr = Math.Sqrt(px * px + pz * pz);                 // 0 the middle, 1 the edge
						double light = Math.Exp(-Math.Pow(rr / 0.42, 2));
						double ring = Math.Exp(-Math.Pow((rr - 0.30) / 0.006, 2)) * 0.8 +
						              Math.Exp(-Math.Pow((rr - 0.315) / 0.004, 2)) * 0.5;
						double grain = (Fbm((px + 1) * 0.5, (pz + 1) * 0.5, 16, 4, 21) - 0.5) * (dark ? 0.25 : 0.04);
						double edge = Smooth(Math.Max(0, Math.Min(1, (rr - 0.55) / 0.45)));
						double[] c = new double[3];
						for (int k = 0; k < 3; k++) {
							double v0 = Mix(floor[k], underFeet[k], light) * (1 + grain * (1 - edge));
							v0 = Mix(v0, ornament[k], ring * (1 - edge) * (dark ? 0.7 : 0.5));
							c[k] = Mix(v0, floor[k], edge);
						}
						r = c[0]; g = c[1]; bl = c[2];
					} else {
						r = floor[0]; g = floor[1]; bl = floor[2];
					}
				}
				row[x * 3 + 0] = Dither(bl, x, y);
				row[x * 3 + 1] = Dither(g, x, y);
				row[x * 3 + 2] = Dither(r, x, y);
			}
			Marshal.Copy(row, 0, data.Scan0 + y * data.Stride, data.Stride);
		}
		bmp.UnlockBits(data);
		bmp.Save(path, ImageFormat.Png);
		if (!string.IsNullOrEmpty(previewPath)) {
			using (var small = new Bitmap(bmp, new Size(1024, 1024))) small.Save(previewPath, ImageFormat.Png);
		}
		bmp.Dispose();
	}
}
"@

$inv = [System.Globalization.CultureInfo]::InvariantCulture

# The meshes, y up, metres. The dome's v runs over the atlas' top half (OBJ's v up: the
# zenith at 1, the nadir at 0.5); the floor's over the bottom-left square.
function Write-Dome([string]$name, [bool]$dark) {
	$preview = ""
	if ($PreviewDir -ne "") {
		New-Item -ItemType Directory -Force $PreviewDir | Out-Null
		$preview = Join-Path $PreviewDir "$name-preview.png"
	}
	[LoadingStageArt]::Write((Join-Path $OutDir "$name.png"), $Size, $dark, $preview)

	$mtl = @(
		"# OBVR's loading stage (tools/make-loading-stage.ps1)",
		"newmtl stage",
		"Ka 1.000 1.000 1.000",
		"Kd 1.000 1.000 1.000",
		"Ks 0.000 0.000 0.000",
		"illum 1",
		"map_Kd $name.png")
	Set-Content -Path (Join-Path $OutDir "$name.mtl") -Value $mtl -Encoding ascii

	$v = New-Object System.Collections.Generic.List[string]
	$vt = New-Object System.Collections.Generic.List[string]
	$vn = New-Object System.Collections.Generic.List[string]
	$f = New-Object System.Collections.Generic.List[string]
	for ($r = 0; $r -le $Rings; $r++) {
		$phi = [Math]::PI * $r / $Rings          # 0 at the top
		for ($s = 0; $s -le $Segments; $s++) {
			$theta = 2.0 * [Math]::PI * $s / $Segments
			$x = $RadiusMetres * [Math]::Sin($phi) * [Math]::Cos($theta)
			$y = $RadiusMetres * [Math]::Cos($phi)
			$z = $RadiusMetres * [Math]::Sin($phi) * [Math]::Sin($theta)
			$v.Add([string]::Format($inv, "v {0:F4} {1:F4} {2:F4}", $x, $y, $z))
			$vt.Add([string]::Format($inv, "vt {0:F5} {1:F5}", $s / $Segments, 1.0 - 0.5 * $r / $Rings))
			$vn.Add([string]::Format($inv, "vn {0:F4} {1:F4} {2:F4}",
				-[Math]::Sin($phi) * [Math]::Cos($theta), -[Math]::Cos($phi), -[Math]::Sin($phi) * [Math]::Sin($theta)))
		}
	}
	$row = $Segments + 1
	for ($r = 0; $r -lt $Rings; $r++) {
		for ($s = 0; $s -lt $Segments; $s++) {
			$a = $r * $row + $s + 1; $b = $a + 1; $c = $a + $row; $d = $c + 1
			$f.Add("f $a/$a/$a $c/$c/$c $b/$b/$b")        # counter-clockwise seen from inside
			$f.Add("f $b/$b/$b $c/$c/$c $d/$d/$d")
		}
	}
	# The floor disc: a centre and a ring of 96, facing up.
	$centre = $v.Count + 1
	$v.Add([string]::Format($inv, "v 0.0000 {0:F4} 0.0000", $FloorHeightMetres))
	$vt.Add("vt 0.25000 0.25000")
	$vn.Add("vn 0.0000 1.0000 0.0000")
	$floorSegments = 96
	for ($s = 0; $s -le $floorSegments; $s++) {
		$theta = 2.0 * [Math]::PI * $s / $floorSegments
		$x = $FloorRadiusMetres * [Math]::Cos($theta)
		$z = $FloorRadiusMetres * [Math]::Sin($theta)
		$v.Add([string]::Format($inv, "v {0:F4} {1:F4} {2:F4}", $x, $FloorHeightMetres, $z))
		# The square's u from x, its v from z: 0.25 the middle, 0.25 +- 0.25 the edge.
		$vt.Add([string]::Format($inv, "vt {0:F5} {1:F5}", 0.25 + 0.25 * [Math]::Cos($theta), 0.25 - 0.25 * [Math]::Sin($theta)))
		$vn.Add("vn 0.0000 1.0000 0.0000")
	}
	for ($s = 0; $s -lt $floorSegments; $s++) {
		$a = $centre + 1 + $s; $b = $a + 1
		$f.Add("f $centre/$centre/$centre $b/$b/$b $a/$a/$a")   # counter-clockwise seen from above
	}

	$lines = New-Object System.Collections.Generic.List[string]
	$lines.Add("# OBVR's loading stage (tools/make-loading-stage.ps1): a $RadiusMetres m dome faced inwards and a $FloorRadiusMetres m floor disc")
	$lines.Add("mtllib $name.mtl")
	$lines.AddRange($v); $lines.AddRange($vt); $lines.AddRange($vn)
	$lines.Add("usemtl stage")
	$lines.AddRange($f)
	Set-Content -Path (Join-Path $OutDir "$name.obj") -Value $lines -Encoding ascii
	$png = Get-Item (Join-Path $OutDir "$name.png")
	Write-Output ("Wrote {0}.obj ({1} lines), .mtl and .png ({2} x {2}, {3:N1} MB)" -f $name, $lines.Count, $Size, ($png.Length / 1MB))
}

Write-Dome "loading_dome_dark" $true
