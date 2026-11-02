# The difference between two frames of a water-turn-jitter run as a picture:
# |a - b| per channel, times the gain, so that what moved between the frames
# stands out - the waves as blobs, a reflection's texel jump as the outline
# of the reflected shore drawn twice. The second frame can be read shifted
# to the right by a few pixels first (the turn's own motion taken out, as
# tools/water-turn-jitter.ps1 does).
#
#   powershell -ExecutionPolicy Bypass -File tools\water-frame-diff.ps1 -A <f10.png> -B <f11.png> -Out <diff.png> [-Shift -1] [-Gain 8]

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$A,
	[Parameter(Mandatory = $true)][string]$B,
	[Parameter(Mandatory = $true)][string]$Out,
	[int]$Shift = 0,
	[int]$Gain = 8
)

$ErrorActionPreference = "Stop"
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class WaterFrameDiff {
	static byte[] Bytes(string path, out int w, out int h, out int stride) {
		using (Bitmap bmp = new Bitmap(path)) {
			w = bmp.Width; h = bmp.Height;
			BitmapData d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
			stride = d.Stride;
			byte[] b = new byte[stride * h];
			Marshal.Copy(d.Scan0, b, 0, b.Length);
			bmp.UnlockBits(d);
			return b;
		}
	}

	public static void Write(string a, string b, string output, int shift, int gain) {
		int w, h, stride, w2, h2, stride2;
		byte[] pa = Bytes(a, out w, out h, out stride);
		byte[] pb = Bytes(b, out w2, out h2, out stride2);
		if (w != w2 || h != h2) throw new Exception("the frames differ in size");
		using (Bitmap outBmp = new Bitmap(w, h, PixelFormat.Format24bppRgb)) {
			BitmapData d = outBmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
			byte[] po = new byte[d.Stride * h];
			for (int y = 0; y < h; y++) {
				for (int x = 0; x < w; x++) {
					int xb = x + shift;
					if (xb < 0 || xb >= w) continue;
					for (int c = 0; c < 3; c++) {
						int v = Math.Abs(pa[y * stride + x * 3 + c] - pb[y * stride2 + xb * 3 + c]) * gain;
						po[y * d.Stride + x * 3 + c] = (byte)(v > 255 ? 255 : v);
					}
				}
			}
			Marshal.Copy(po, 0, d.Scan0, po.Length);
			outBmp.UnlockBits(d);
			outBmp.Save(output, ImageFormat.Png);
		}
	}
}
"@
[WaterFrameDiff]::Write((Resolve-Path $A).Path, (Resolve-Path $B).Path, $Out, $Shift, $Gain)
Write-Output "wrote $Out"
