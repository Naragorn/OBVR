param([string]$Path)
Add-Type -AssemblyName System.Drawing
foreach ($path in @($Path)) {
  $bmp = New-Object System.Drawing.Bitmap -ArgumentList $path
  $w = $bmp.Width; $h = $bmp.Height; $half = [int]($w / 2)
  $rect = New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $w, $h
  $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $stride = $data.Stride
  $bytes = [byte[]]::new($stride * $h)
  [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
  $bmp.UnlockBits($data); $bmp.Dispose()
  $g = New-Object 'int[,]' $h, $w
  for ($y = 0; $y -lt $h; $y++) { $o = $y * $stride; for ($x = 0; $x -lt $w; $x++) { $i = $o + $x * 3; $g[$y, $x] = [int]$bytes[$i] + [int]$bytes[$i + 1] + [int]$bytes[$i + 2] } }
  Write-Output ("{0} {1}x{2}: best horizontal shift of the right eye against the left, per band (search -60..60 px)" -f [IO.Path]::GetFileName($path), $w, $h)
  foreach ($band in 0..7) {
    $y0 = [int]($h * $band / 8); $y1 = [int]($h * ($band + 1) / 8) - 1
    $best = 0; $bestErr = [double]::MaxValue; $zeroErr = 0.0
    foreach ($s in -160..40) {
      $err = 0.0; $n = 0
      for ($y = $y0; $y -le $y1; $y += 4) {
        for ($x = 170; $x -lt ($half - 170); $x += 2) {
          $a = $g[$y, $x]; $b = $g[$y, ($x + $s + $half)]
          $d = $a - $b; if ($d -lt 0) { $d = -$d }
          $err += $d; $n++
        }
      }
      $err = $err / $n
      if ($s -eq 0) { $zeroErr = $err }
      if ($err -lt $bestErr) { $bestErr = $err; $best = $s }
    }
    Write-Output ("  band {0} (y {1}-{2}): best shift {3,3} px, mean diff {4,6:N1}; at shift 0: {5,6:N1}" -f $band, $y0, $y1, $best, $bestErr, $zeroErr)
  }
}
