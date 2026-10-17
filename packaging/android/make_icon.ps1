param([string]$OutDir)
Add-Type -AssemblyName System.Drawing
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }
$png = Join-Path $OutDir "ic_launcher.png"

$bmp = New-Object System.Drawing.Bitmap 512, 512
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

$bg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 20, 16, 44))
$g.FillRectangle($bg, 0, 0, 512, 512)

# silhouette: dark figure against moon
$moonBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 224, 205, 130))
$g.FillEllipse($moonBrush, 250, 60, 200, 200)
$figBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 10, 8, 18))
$g.FillEllipse($figBrush, 120, 220, 80, 80)
$g.FillRectangle($figBrush, 134, 292, 52, 150)
$g.FillEllipse($figBrush, 118, 430, 36, 30)
$g.FillEllipse($figBrush, 166, 430, 36, 30)

$textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 232, 100, 100))
$font = New-Object System.Drawing.Font ("Segoe UI", 88, [System.Drawing.FontStyle]::Bold)
$g.DrawString("SLASH", $font, $textBrush, 20, 20)

$g.Dispose()
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "icon: $png"
