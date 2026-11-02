# Reads a water-turn-jitter run (tools/hand-scripts/water-turn-jitter.txt and
# its -engine control): the right eye's frames in a row at the marks "still"
# and "turn" (OBVR-Eye-Right-<mark>-f<NN>.png, half size), and for every pair
# of frames in a row the horizontal shift of the water box that matches them
# best and the mean difference left after it. The turn moves the whole
# picture about a pixel a frame, which the shift takes out; what is left is
# the waves' own change (the same in both runs) and the reflection's texel
# jumps (the turning run's excess over the still one). A bigger reflection
# map (WaterReflectionMapSize) must shrink that excess; the two runs of one
# build, and the same of the control build, say by how much.
#
#   powershell -ExecutionPolicy Bypass -File tools\water-turn-jitter.ps1 -Run artifacts\hand-script\water-turn-jitter\<time>
#
# -Box l,t,r,b picks the water box in the half-size picture's pixels; the
# default takes the lower middle (15 to 85 % across, 45 to 90 % down), under
# the horizon at the scenario's pitch of -15 degrees. The result is printed
# and written to the run as water-turn-jitter.txt.

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$Run,
	[string]$Box = "",
	[int]$MaxShift = 12,
	[string[]]$Series = @("still", "turn")
)

$ErrorActionPreference = "Stop"
$runDir = (Resolve-Path -LiteralPath $Run).Path

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class WaterJitter {
	// The picture as grey values (the three channels added), row-major.
	public static int[] Gray(string path, out int width, out int height) {
		using (Bitmap bmp = new Bitmap(path)) {
			width = bmp.Width; height = bmp.Height;
			Rectangle rect = new Rectangle(0, 0, width, height);
			BitmapData data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
			byte[] bytes = new byte[data.Stride * height];
			Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
			bmp.UnlockBits(data);
			int[] gray = new int[width * height];
			for (int y = 0; y < height; y++) {
				int o = y * data.Stride;
				for (int x = 0; x < width; x++) {
					int i = o + x * 3;
					gray[y * width + x] = bytes[i] + bytes[i + 1] + bytes[i + 2];
				}
			}
			return gray;
		}
	}

	// The mean absolute difference of the box between a and b with b read
	// `shift` pixels to the right.
	public static double Diff(int[] a, int[] b, int width, int l, int t, int r, int bottom, int shift) {
		double sum = 0; long n = 0;
		for (int y = t; y < bottom; y++) {
			int row = y * width;
			for (int x = l; x < r; x++) {
				int xb = x + shift;
				if (xb < 0 || xb >= width) continue;
				int d = a[row + x] - b[row + xb];
				sum += d < 0 ? -d : d; n++;
			}
		}
		return n == 0 ? 0.0 : sum / n;
	}

	// The shift that matches the pair best, and the difference at it and at none.
	public static void Best(int[] a, int[] b, int width, int l, int t, int r, int bottom, int maxShift,
	                        out int bestShift, out double bestDiff, out double zeroDiff) {
		bestShift = 0; bestDiff = double.MaxValue; zeroDiff = 0;
		for (int s = -maxShift; s <= maxShift; s++) {
			double d = Diff(a, b, width, l, t, r, bottom, s);
			if (s == 0) zeroDiff = d;
			if (d < bestDiff) { bestDiff = d; bestShift = s; }
		}
	}
}
"@

function Read-Series([string]$name) {
	$files = @(Get-ChildItem -LiteralPath $runDir -Filter "OBVR-Eye-Right-$name-f*.png" |
		Sort-Object { [int]($_.BaseName -replace '.*-f(\d+)$', '$1') })
	if ($files.Count -lt 2) { throw "fewer than two frames of the run '$name' in $runDir" }
	$frames = @()
	$w = 0; $h = 0
	foreach ($f in $files) {
		$frames += ,[WaterJitter]::Gray($f.FullName, [ref]$w, [ref]$h)
	}
	return @{ Frames = $frames; Width = $w; Height = $h; Count = $files.Count }
}

$lines = @()
$summary = @{}
foreach ($name in $Series) {
	$s = Read-Series $name
	$w = $s.Width; $h = $s.Height
	if ($Box -ne "") {
		$parts = $Box.Split(",") | ForEach-Object { [int]$_ }
		$l = $parts[0]; $t = $parts[1]; $r = $parts[2]; $b = $parts[3]
	} else {
		$l = [int]($w * 0.15); $r = [int]($w * 0.85); $t = [int]($h * 0.45); $b = [int]($h * 0.90)
	}
	$lines += ("{0}: {1} frames of {2}x{3}, the water box x {4}-{5} y {6}-{7}, shifts to {8} px" -f $name, $s.Count, $w, $h, $l, $r, $t, $b, $MaxShift)
	$residuals = @()
	$spikes = 0
	for ($i = 1; $i -lt $s.Count; $i++) {
		$shift = 0; $best = 0.0; $zero = 0.0
		[WaterJitter]::Best($s.Frames[$i - 1], $s.Frames[$i], $w, $l, $t, $r, $b, $MaxShift, [ref]$shift, [ref]$best, [ref]$zero)
		$residuals += $best
		$lines += ("  f{0:D2}->f{1:D2}: shift {2,3} px, left {3,6:N2}, at no shift {4,6:N2}" -f ($i - 1), $i, $shift, $best, $zero)
	}
	$sorted = $residuals | Sort-Object
	$mean = ($residuals | Measure-Object -Average).Average
	$max = ($residuals | Measure-Object -Maximum).Maximum
	$median = $sorted[[int]($sorted.Count / 2)]
	$summary[$name] = @{ Mean = $mean; Median = $median; Max = $max; Values = $residuals }
	$lines += ("  {0}: mean {1:N2}, median {2:N2}, max {3:N2} (grey units, three channels added)" -f $name, $mean, $median, $max)
}
if ($summary.ContainsKey("still") -and $summary.ContainsKey("turn")) {
	$stillMean = $summary["still"].Mean
	$turnMean = $summary["turn"].Mean
	$threshold = $stillMean * 1.5
	$spikes = @($summary["turn"].Values | Where-Object { $_ -gt $threshold }).Count
	$lines += ("turning over still: mean {0:N2} against {1:N2} ({2:N2} excess, x{3:N2}); {4} of {5} turning pairs over 1.5 x the still mean" -f $turnMean, $stillMean, ($turnMean - $stillMean), ($(if ($stillMean -gt 0) { $turnMean / $stillMean } else { 0 })), $spikes, $summary["turn"].Values.Count)
}
$lines | Set-Content -LiteralPath (Join-Path $runDir "water-turn-jitter.txt") -Encoding UTF8
$lines | ForEach-Object { Write-Output $_ }
