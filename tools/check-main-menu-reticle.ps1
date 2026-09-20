# Checks the visible main-menu screenshot for the gold sneak-eye overlay.
# The checker is intentionally independent of OBVR.log: the acceptance
# criterion is the pixels the wearer sees, not merely that a hide call ran.

param(
	[Parameter(Mandatory = $true)]
	[string]$ImagePath
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resolved = (Resolve-Path -LiteralPath $ImagePath).Path
$image = [Drawing.Bitmap]::FromFile($resolved)
try {
	# The cursor-shot harness captures 2560x1440. Scale the measured region so
	# the same check also works with an unscaled desktop capture.
	$scaleX = $image.Width / 2560.0
	$scaleY = $image.Height / 1440.0
	$x0 = [Math]::Max(0, [int][Math]::Floor(1800 * $scaleX))
	$x1 = [Math]::Min($image.Width, [int][Math]::Ceiling(2200 * $scaleX))
	$y0 = [Math]::Max(0, [int][Math]::Floor(650 * $scaleY))
	$y1 = [Math]::Min($image.Height, [int][Math]::Ceiling(840 * $scaleY))
	$stepX = [Math]::Max(1, [int][Math]::Round(2 * $scaleX))
	$stepY = [Math]::Max(1, [int][Math]::Round(2 * $scaleY))
	# The residual title-screen child is a small horizontal gold/black dash
	# centered just below the title.  The broad eye test above can miss it
	# because the dash has little dark area, so inspect its fixed screen-space
	# location separately.  This is deliberately a tight box: the title and
	# map are outside it in the captured main-menu layout.
	$fragmentX0 = [Math]::Max(0, [int][Math]::Floor(1985 * $scaleX))
	$fragmentX1 = [Math]::Min($image.Width, [int][Math]::Ceiling(2055 * $scaleX))
	$fragmentY0 = [Math]::Max(0, [int][Math]::Floor(700 * $scaleY))
	$fragmentY1 = [Math]::Min($image.Height, [int][Math]::Ceiling(735 * $scaleY))

	$dark = 0
	$gold = 0
	for ($y = $y0; $y -lt $y1; $y += $stepY) {
		for ($x = $x0; $x -lt $x1; $x += $stepX) {
			$pixel = $image.GetPixel($x, $y)
			if ($pixel.R -lt 95 -and $pixel.G -lt 90 -and $pixel.B -lt 75) {
				++$dark
			}
			if ($pixel.R -gt 150 -and $pixel.G -gt 105 -and $pixel.B -lt 105 -and
				($pixel.R - $pixel.B) -gt 60 -and ($pixel.G - $pixel.B) -gt 25) {
				++$gold
			}
		}
	}

	$fragmentDark = 0
	for ($y = $fragmentY0; $y -lt $fragmentY1; $y++) {
		for ($x = $fragmentX0; $x -lt $fragmentX1; $x++) {
			$pixel = $image.GetPixel($x, $y)
			if ($pixel.R -lt 170 -and $pixel.G -lt 165 -and $pixel.B -lt 145) {
				++$fragmentDark
			}
		}
	}

	# The fixed main-menu background can contain dark map lines, especially in
	# a larger unscaled capture. The eye is the combination that matters here:
	# a large dark contour plus the concentrated gold rim. Known failing 2560x
	# 1440 captures measure dark=668..694 and gold=705..807; clean captures
	# have no comparable gold population in this region.
	$fragmentDetected = $fragmentDark -ge 80
	$eyeDetected = ($dark -ge 500 -and $gold -ge 600) -or $fragmentDetected
	Write-Host "Main-menu reticle check: dark=$dark gold=$gold fragmentDark=$fragmentDark region=${x0},${y0}..${x1},${y1} detected=$eyeDetected"
	if ($eyeDetected) {
		throw "The main-menu sneak-eye pixels are still visible in $resolved."
	}
}
finally {
	$image.Dispose()
}
