# Converts an uncompressed 24/32-bit TGA (as written by the Quake 3 "screenshot"
# command) to PNG.  Usage: tga2png.ps1 <in.tga> <out.png>
param([Parameter(Mandatory)][string]$In, [Parameter(Mandatory)][string]$Out)
Add-Type -AssemblyName System.Drawing

$b = [IO.File]::ReadAllBytes((Resolve-Path $In))
if ($b[2] -ne 2) { throw "only uncompressed true-color TGA is supported" }
$w = $b[12] + 256 * $b[13]
$h = $b[14] + 256 * $b[15]
$bpp = $b[16] / 8
$off = 18 + $b[0]
$topDown = ($b[17] -band 0x20) -ne 0

$bmp = New-Object Drawing.Bitmap $w, $h, ([Drawing.Imaging.PixelFormat]::Format24bppRgb)
$data = $bmp.LockBits((New-Object Drawing.Rectangle 0, 0, $w, $h), 'WriteOnly', $bmp.PixelFormat)
$row = New-Object byte[] ($data.Stride)
for ($y = 0; $y -lt $h; $y++) {
    $srcY = if ($topDown) { $y } else { $h - 1 - $y }
    $s = $off + $srcY * $w * $bpp
    for ($x = 0; $x -lt $w; $x++) {
        $row[$x * 3] = $b[$s + $x * $bpp]
        $row[$x * 3 + 1] = $b[$s + $x * $bpp + 1]
        $row[$x * 3 + 2] = $b[$s + $x * $bpp + 2]
    }
    [Runtime.InteropServices.Marshal]::Copy($row, 0, [IntPtr]($data.Scan0.ToInt64() + $y * $data.Stride), $data.Stride)
}
$bmp.UnlockBits($data)
$bmp.Save([IO.Path]::GetFullPath($Out), [Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
