# Reads the frame-to-frame differences out of a run's "Body trace:" lines
# (Debug.BodyTrace; game/BodyIk.cpp "The trace"), window by window between
# the hand script's marks: how far the body moved against the headset, how
# the head bone's animated offset, the let-down, the torso's yaw and each
# arm's bend and elbow direction changed from one frame to the next, and how
# many of the frame's bone writes survived the draw. With the inputs standing
# still nothing of the body may move; the "slide", "walk", "pitch" windows
# show how it moves when they do.
#
#   tools\body-trace.ps1 -Log artifacts\hand-script\body-ik-smooth\<run>\OBVR.log
#
# Prints one row per window and, with -Jumps, every frame whose change is
# larger than the thresholds (a jump).

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$Log,
	# Frames dropped at the start of each window: the inputs' step settles.
	[int]$Settle = 10,
	[double]$JumpUnits = 0.5,
	[double]$JumpDegrees = 2.0,
	[switch]$Jumps,
	# The rows as CSV instead of a table (a narrow console cuts the table).
	[switch]$Csv
)

$ErrorActionPreference = "Stop"

$trace = [regex]'^Body trace: f=(\d+) cam (\S+) (\S+) (\S+) root (\S+) (\S+) (\S+) par (\S+) (\S+) (\S+) headoff (\S+) (\S+) (\S+) used (\S+) (\S+) (\S+) low (\S+) lift (\S+) yaw (\S+) chest (\S+) held (\d+)/(\d+) fp (\d+)/(\d+)(?: trunk (\d))?(?: feet (\S+?)(s?) (\S+?)(s?))? \| R f(\d) d(\S+) bend (\S+) pole (\S+) (\S+) (\S+) mv(\d) clav (\S+) off (\S+) \| L f(\d) d(\S+) bend (\S+) pole (\S+) (\S+) (\S+) mv(\d) clav (\S+) off (\S+)'
$markLine = [regex]'^HandScript: mark (\S+)'

# Vectors as objects with X, Y and Z (arrays returned from functions get
# unrolled or wrapped by PowerShell's pipeline, objects do not).
function Vec([string]$x, [string]$y, [string]$z) {
	return [pscustomobject]@{ X = [double]$x; Y = [double]$y; Z = [double]$z }
}
function Sub($a, $b) { return [pscustomobject]@{ X = $a.X - $b.X; Y = $a.Y - $b.Y; Z = $a.Z - $b.Z } }
function Len($a) { return [math]::Sqrt($a.X * $a.X + $a.Y * $a.Y + $a.Z * $a.Z) }
function AngleDeg($a, $b) {
	$la = Len $a; $lb = Len $b
	if ($la -lt 1e-6 -or $lb -lt 1e-6) { return 0.0 }
	$d = ($a.X * $b.X + $a.Y * $b.Y + $a.Z * $b.Z) / ($la * $lb)
	if ($d -gt 1.0) { $d = 1.0 }
	if ($d -lt -1.0) { $d = -1.0 }
	return [math]::Acos($d) * 180.0 / [math]::PI
}

$frames = New-Object System.Collections.ArrayList
$window = "start"
foreach ($line in [IO.File]::ReadLines($Log)) {
	$m = $markLine.Match($line)
	if ($m.Success) { $window = $m.Groups[1].Value; continue }
	$t = $trace.Match($line)
	if (-not $t.Success) { continue }
	$g = $t.Groups
	$f = [pscustomobject]@{
		window = $window
		frame = [int]$g[1].Value
		cam = Vec $g[2].Value $g[3].Value $g[4].Value
		root = Vec $g[5].Value $g[6].Value $g[7].Value
		par = Vec $g[8].Value $g[9].Value $g[10].Value
		headoff = Vec $g[11].Value $g[12].Value $g[13].Value
		used = Vec $g[14].Value $g[15].Value $g[16].Value
		low = [double]$g[17].Value
		lift = [double]$g[18].Value
		yaw = [double]$g[19].Value
		chest = [double]$g[20].Value
		held = [int]$g[21].Value
		written = [int]$g[22].Value
		fpHeld = [int]$g[23].Value
		fpWritten = [int]$g[24].Value
		trunk = $(if ($g[25].Success) { [int]$g[25].Value } else { -1 })
		footR = $(if ($g[26].Success) { [double]$g[26].Value } else { 0.0 })
		footRSolved = $(if ($g[27].Success -and $g[27].Value -eq 's') { 1 } else { 0 })
		footL = $(if ($g[28].Success) { [double]$g[28].Value } else { 0.0 })
		footLSolved = $(if ($g[29].Success -and $g[29].Value -eq 's') { 1 } else { 0 })
		rFollows = [int]$g[30].Value
		rReach = [double]$g[31].Value
		rBend = [double]$g[32].Value
		rPole = Vec $g[33].Value $g[34].Value $g[35].Value
		rMoved = [int]$g[36].Value
		rClav = [double]$g[37].Value
		rOff = [double]$g[38].Value
		lFollows = [int]$g[39].Value
		lReach = [double]$g[40].Value
		lBend = [double]$g[41].Value
		lPole = Vec $g[42].Value $g[43].Value $g[44].Value
		lMoved = [int]$g[45].Value
		lClav = [double]$g[46].Value
		lOff = [double]$g[47].Value
	}
	[void]$frames.Add($f)
}
if ($frames.Count -eq 0) { throw "no Body trace lines in $Log" }

# The windows in the order they came.
$order = New-Object System.Collections.ArrayList
foreach ($f in $frames) { if (-not $order.Contains($f.window)) { [void]$order.Add($f.window) } }

$rows = New-Object System.Collections.ArrayList
$jumpLines = New-Object System.Collections.ArrayList
foreach ($w in $order) {
	$in = @($frames | Where-Object { $_.window -eq $w })
	if ($in.Count -le $Settle + 1) { continue }
	$in = $in[$Settle..($in.Count - 1)]
	$maxBody = 0.0; $sumBody = 0.0; $maxHead = 0.0; $maxLow = 0.0; $maxYaw = 0.0
	$maxBendR = 0.0; $maxBendL = 0.0; $maxPoleR = 0.0; $maxPoleL = 0.0; $maxPar = 0.0
	$jumpsBend = 0; $jumpsBody = 0; $minHeld = 1.0; $movedR = 0; $n = 0
	$worstBody = 0; $worstBend = 0; $trunkHeld = 0
	$feetSolved = 0; $footMin = 0.0; $footMax = 0.0
	for ($i = 1; $i -lt $in.Count; $i++) {
		$a = $in[$i - 1]; $b = $in[$i]
		$n++
		$dBody = Len (Sub (Sub $b.root $b.cam) (Sub $a.root $a.cam))
		$dHead = Len (Sub $b.headoff $a.headoff)
		$dPar = Len (Sub $b.par $a.par)
		$dLow = [math]::Abs($b.low - $a.low)
		$dYaw = [math]::Abs($b.yaw - $a.yaw)
		$dBendR = [math]::Abs($b.rBend - $a.rBend)
		$dBendL = [math]::Abs($b.lBend - $a.lBend)
		$dPoleR = AngleDeg $a.rPole $b.rPole
		$dPoleL = AngleDeg $a.lPole $b.lPole
		$sumBody += $dBody
		if ($dBody -gt $maxBody) { $maxBody = $dBody; $worstBody = $b.frame }
		if ($dHead -gt $maxHead) { $maxHead = $dHead }
		if ($dPar -gt $maxPar) { $maxPar = $dPar }
		if ($dLow -gt $maxLow) { $maxLow = $dLow }
		if ($dYaw -gt $maxYaw) { $maxYaw = $dYaw }
		if ($dBendR -gt $maxBendR) { $maxBendR = $dBendR; $worstBend = $b.frame }
		if ($dBendL -gt $maxBendL) { $maxBendL = $dBendL }
		if ($dPoleR -gt $maxPoleR) { $maxPoleR = $dPoleR }
		if ($dPoleL -gt $maxPoleL) { $maxPoleL = $dPoleL }
		if ($dBendR -gt $JumpDegrees -or $dBendL -gt $JumpDegrees -or $dPoleR -gt $JumpDegrees -or $dPoleL -gt $JumpDegrees) { $jumpsBend++ }
		if ($dBody -gt $JumpUnits) { $jumpsBody++ }
		if ($b.rMoved -eq 1) { $movedR++ }
		if ($b.trunk -eq 1) { $trunkHeld++ }
		if ($b.footRSolved -eq 1 -or $b.footLSolved -eq 1) { $feetSolved++ }
		foreach ($fd in @($b.footR, $b.footL)) {
			if ($fd -lt $footMin) { $footMin = $fd }
			if ($fd -gt $footMax) { $footMax = $fd }
		}
		if ($b.written -gt 0) {
			$ratio = $b.held / $b.written
			if ($ratio -lt $minHeld) { $minHeld = $ratio }
		}
		if ($Jumps -and ($dBody -gt $JumpUnits -or $dBendR -gt $JumpDegrees -or $dBendL -gt $JumpDegrees -or $dPoleR -gt $JumpDegrees -or $dPoleL -gt $JumpDegrees)) {
			[void]$jumpLines.Add(("  {0,-10} f={1,-5} body {2,6:0.00} u  bendR {3,6:0.00}  bendL {4,6:0.00}  poleR {5,6:0.00}  poleL {6,6:0.00} deg  low {7,6:0.00}  reachR {8:0.000}" -f $w, $b.frame, $dBody, $dBendR, $dBendL, $dPoleR, $dPoleL, $dLow, $b.rReach))
		}
	}
	$meanBody = 0.0
	if ($n -gt 0) { $meanBody = $sumBody / $n }
	[void]$rows.Add([pscustomobject]@{
		window = $w
		frames = $n
		"body max" = ("{0:0.000}" -f $maxBody)
		"body mean" = ("{0:0.0000}" -f $meanBody)
		"body jumps" = $jumpsBody
		"parent max" = ("{0:0.000}" -f $maxPar)
		"headoff max" = ("{0:0.000}" -f $maxHead)
		"low max" = ("{0:0.000}" -f $maxLow)
		"yaw max" = ("{0:0.00}" -f $maxYaw)
		"bendR max" = ("{0:0.00}" -f $maxBendR)
		"bendL max" = ("{0:0.00}" -f $maxBendL)
		"poleR max" = ("{0:0.00}" -f $maxPoleR)
		"poleL max" = ("{0:0.00}" -f $maxPoleL)
		"arm jumps" = $jumpsBend
		"moved frames" = $movedR
		"trunk held" = $trunkHeld
		"feet solved" = $feetSolved
		"foot off min" = ("{0:0.00}" -f $footMin)
		"foot off max" = ("{0:0.00}" -f $footMax)
		"held min" = ("{0:0.00}" -f $minHeld)
		"reachR" = ("{0:0.000}" -f $in[-1].rReach)
		"reachL" = ("{0:0.000}" -f $in[-1].lReach)
	})
}

"Body trace: $($frames.Count) frames in $Log"
"Per window (the first $Settle frames of each dropped): the largest change from one frame to the next - the body against the headset in units, the skeleton's parent, the head bone's animated offset, the let-down; the torso's yaw and each arm's bend and elbow direction in degrees; frames over $JumpUnits units / $JumpDegrees degrees counted as jumps; 'held min' the share of the frame's bone writes the engine left through the draw."
if ($Csv) {
	$rows | ConvertTo-Csv -NoTypeInformation
} else {
	$rows | Format-Table -AutoSize | Out-String -Width 400
}
if ($Jumps -and $jumpLines.Count -gt 0) {
	"Jumps:"
	$jumpLines
}
