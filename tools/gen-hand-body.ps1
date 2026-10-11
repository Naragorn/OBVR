# Writes tools/hand-scripts/hand-body.txt (docs/hand-script-harness.md): the
# left hand swept slowly - 5 cm every 0.4 s - through the space beside and
# behind the player at three heights, then the same path fast, 10 cm every
# 0.05 s. The steps are small because the harness moves a hand in one frame
# and the speed the hands' contact judges is the way come over the last tenth
# of a second (game/HandContactLogic.h, HandTravel). Numbers are written
# with a decimal point whatever the machine's language.
#
# Run: powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen-hand-body.ps1

[Threading.Thread]::CurrentThread.CurrentCulture = [Globalization.CultureInfo]::InvariantCulture
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$target = Join-Path $root "tools\hand-scripts\hand-body.txt"

$head = @'
# The free hand meets people ([Hands] HandsStopAtBodies,
# game/HandContactLogic.h, the living of game/BladeContactLogic.h): a beggar
# is placed by the player and frozen (tai), nothing is drawn, and the left
# hand is swept slowly - 5 cm every 0.4 s, under the shove's speed over any
# tenth of a second, one polyline without a jump - through the space beside,
# ahead of, behind and through the middle of where PlaceAtMe puts them,
# standing or lying (0.2 m behind the head or 0.85 m ahead of it, in the
# middle, 2026-10-11; 30 to 110 units behind in blade-body.txt), at three
# heights, so that wherever they are the hand comes to rest on them; then
# the same path fast, 10 cm every 0.05 s, a shove's speed - the hand goes
# through them and they are passed. The "Contact: hands" lines at the marks
# give their capsules. Written by tools/gen-hand-body.ps1 - edit that, not
# this.
# Run with: tools\hand-script-run.ps1 -Script tools\hand-scripts\hand-body.txt

ini Hands Enabled=1
ini Hands HandsStopAtWalls=1
ini Hands HandsStopAtBodies=1
ini Hands WeaponLetGoMetres=0.30
ini Hands PushPeople=0
ini Hands Shove=0
console-at ready player.placeatme 0001C45B 1 60 0
console-at ready tai

expect HandScript: finished
expect skeleton read
expect Contact: hands - 1 near
expect Contact: the left hand rests on
expect Contact: a fast left hand went into someone
reject HandScript: WARNING

wait 12
left rot 0 0 0
left pos -0.20 -0.40 -0.30
right rot 0 0 0
right pos 0.20 -0.40 -0.30
wait 1
mark ready
wait 20
mark placed

'@

# The points along a polyline, $step metres apart at most.
function Leg($points, $step) {
	$out = @()
	for ($i = 0; $i + 1 -lt $points.Count; $i++) {
		$a = $points[$i]
		$b = $points[$i + 1]
		$span = [Math]::Max([Math]::Max([Math]::Abs($b[0] - $a[0]), [Math]::Abs($b[1] - $a[1])), [Math]::Abs($b[2] - $a[2]))
		$n = [Math]::Max(1, [int][Math]::Round($span / $step))
		for ($k = 1; $k -le $n; $k++) {
			$out += ,@(($a[0] + ($b[0] - $a[0]) * $k / $n), ($a[1] + ($b[1] - $a[1]) * $k / $n), ($a[2] + ($b[2] - $a[2]) * $k / $n))
		}
	}
	return ,$out
}

# Metres: x right, y up, z back - down the left side from 0.85 m ahead to
# 0.6 m behind, across to the middle, up the middle to ahead, across to the
# right, and down the right side; or the reverse, so one height's loop goes
# on from where the last ended without a jump (a jump is a fast hand). The
# middle pass is what meets a standing body: PlaceAtMe put the beggar 0.2 m
# behind the head once and 0.85 m ahead of it the next time, in the middle
# both times (2026-10-11) - the sides, 0.35 m out, passed 25 units off his
# capsules.
function LoopAt($y, $forward) {
	$pts = @(@(-0.35, $y, -0.85), @(-0.35, $y, 0.60), @(0.0, $y, 0.60), @(0.0, $y, -0.85), @(0.35, $y, -0.85), @(0.35, $y, 0.60))
	if (-not $forward) { [array]::Reverse($pts) }
	return ,$pts
}

function Pos($p, $digits) {
	return [string]::Format([Globalization.CultureInfo]::InvariantCulture, "left pos {0:F$digits} {1:F$digits} {2:F$digits}", $p[0], $p[1], $p[2])
}

# The whole slow sweep as one polyline from the ready pose: each height's
# loop begins where the last one ended.
$rest = @(-0.20, -0.40, -0.30)
$heights = @(@(-0.50, "chest", $true), @(-0.95, "hip", $false), @(-1.30, "knee", $true))
$sb = New-Object System.Text.StringBuilder
[void]$sb.Append($head)
$last = $rest
foreach ($h in $heights) {
	$points = @(,$last) + (LoopAt $h[0] $h[2])
	foreach ($p in (Leg $points 0.05)) { [void]$sb.Append((Pos $p 3) + "`nwait 0.4`n") }
	$last = $points[$points.Count - 1]
	[void]$sb.Append("mark $($h[1])`n")
}
[void]$sb.Append("left pos -0.20 -0.40 -0.30`nwait 1.5`nmark away`n")
$last = $rest
foreach ($h in $heights) {
	$points = @(,$last) + (LoopAt $h[0] $h[2])
	foreach ($p in (Leg $points 0.10)) { [void]$sb.Append((Pos $p 3) + "`nwait 0.05`n") }
	$last = $points[$points.Count - 1]
	[void]$sb.Append("wait 0.5`nmark fast_$($h[1])`n")
}
[void]$sb.Append("left pos -0.20 -0.40 -0.30`nwait 1`nmark end`nwait 1`n")
$text = $sb.ToString() -replace "`r`n", "`n"
[IO.File]::WriteAllText($target, $text, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("wrote {0}, {1} lines" -f $target, ($text -split "`n").Count)
