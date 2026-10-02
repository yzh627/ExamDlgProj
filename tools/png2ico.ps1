# 把任意图片（png/jpg/bmp/gif）转成多尺寸 .ico，替换工程里的图标文件
#   -Target app       只换主程序图标（res\ExamDlgProj.ico）
#   -Target installer 只换安装程序图标（res\Setup.ico）
#   -Target both      两个都换（默认）
# 一般不用手动调用，拖图片到 换图标.bat / 换程序图标.bat / 换安装程序图标.bat 即可
param(
    [Parameter(Mandatory = $true)][string]$ImagePath,
    [ValidateSet('both', 'app', 'installer')][string]$Target = 'both',
    [string]$OutPath = ""
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$projRoot = Split-Path $PSScriptRoot -Parent
$iconApp = Join-Path $projRoot 'res\ExamDlgProj.ico'   # 主程序图标
$iconSetup = Join-Path $projRoot 'res\Setup.ico'       # 安装程序图标

if (-not [string]::IsNullOrWhiteSpace($OutPath)) {
    $targets = @($OutPath)
}
elseif ($Target -eq 'app') { $targets = @($iconApp) }
elseif ($Target -eq 'installer') { $targets = @($iconSetup) }
else { $targets = @($iconApp, $iconSetup) }

if (-not (Test-Path -LiteralPath $ImagePath)) {
    Write-Host "[错误] 找不到图片：$ImagePath" -ForegroundColor Red
    exit 1
}

$src = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $ImagePath).Path)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngList = New-Object System.Collections.ArrayList

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $scale = [Math]::Min($s / $src.Width, $s / $src.Height)
    $w = [int][Math]::Round($src.Width * $scale)
    $h = [int][Math]::Round($src.Height * $scale)
    $x = [int](($s - $w) / 2)
    $y = [int](($s - $h) / 2)
    $g.DrawImage($src, $x, $y, $w, $h)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [void]$pngList.Add($ms.ToArray())
    $ms.Dispose()
    $bmp.Dispose()
}
$src.Dispose()

foreach ($out in $targets) {
    if (Test-Path -LiteralPath $out) {
        $bak = Join-Path (Split-Path $out -Parent) `
            (([System.IO.Path]::GetFileNameWithoutExtension($out)) + '_备份.ico')
        Copy-Item -LiteralPath $out -Destination $bak -Force
        Write-Host "已备份原图标 → $(Split-Path $bak -Leaf)"
    }

    $fs = [System.IO.File]::Create($out)
    $bw = New-Object System.IO.BinaryWriter($fs)
    $bw.Write([UInt16]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $s = $sizes[$i]
        $dim = if ($s -ge 256) { 0 } else { $s }
        $bw.Write([byte]$dim)
        $bw.Write([byte]$dim)
        $bw.Write([byte]0)
        $bw.Write([byte]0)
        $bw.Write([UInt16]1)
        $bw.Write([UInt16]32)
        $bw.Write([UInt32]$pngList[$i].Length)
        $bw.Write([UInt32]$offset)
        $offset += $pngList[$i].Length
    }
    foreach ($p in $pngList) { $bw.Write($p) }
    $bw.Close()
    $fs.Close()
    Write-Host "图标已生成：$out" -ForegroundColor Green
}
Write-Host ("包含尺寸：" + ($sizes -join '、') + " 像素")
