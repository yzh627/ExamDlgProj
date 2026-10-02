# 刷新 发布包\静默安装.bat 里内嵌的「安装程序 SHA256」，让它永远对得上当前发布版本。
# 由 tools\rebuild.ps1 在更新完发布包之后自动调用；也可以单独运行。
#
# 为什么要这么干：静默安装.bat 靠这个哈希「验明正身」，认不出就不肯装。
# 以前这个值要手工改，忘了改它不会报错 —— 只会退到「文件版本信息」那条次优路径，
# 提示语变成"版本与内嵌校验值不同"，看着让人心里没底。现在交给脚本自动同步。
$ErrorActionPreference = 'Stop'
$proj = Split-Path $PSScriptRoot -Parent
$pkg  = Join-Path $proj '发布包'
$setupExe = Join-Path $pkg '对口升学练习系统_安装程序.exe'
$bat      = Join-Path $pkg '静默安装.bat'

if (-not (Test-Path -LiteralPath $setupExe)) {
    Write-Host "[跳过] 找不到安装程序：$setupExe" -ForegroundColor Yellow
    exit 0
}
if (-not (Test-Path -LiteralPath $bat)) {
    Write-Host "[跳过] 找不到静默安装.bat：$bat" -ForegroundColor Yellow
    exit 0
}

$newHash = (Get-FileHash -LiteralPath $setupExe -Algorithm SHA256).Hash.ToLower()

# bat 是 GBK + CRLF 编码（中文 cmd 才不会乱码），所以必须按字节进出，
# 不能 Get-Content/Set-Content —— 那会把编码和换行都改掉。
$gbk  = [System.Text.Encoding]::GetEncoding(936)
$text = $gbk.GetString([System.IO.File]::ReadAllBytes($bat))

$m = [regex]::Match($text, 'set "KNOWN=([0-9a-fA-F]{64})"')
if (-not $m.Success) {
    Write-Host "[警告] 在 静默安装.bat 里找不到 set ""KNOWN=<64位十六进制>"" 这一行，请人工检查。" -ForegroundColor Yellow
    exit 1
}
$oldHash = $m.Groups[1].Value.ToLower()

if ($oldHash -eq $newHash) {
    Write-Host "[跳过] 静默安装.bat 内嵌校验值已是最新：$newHash" -ForegroundColor Green
    exit 0
}

# 三处都要换：注释里那句、KNOWN、SETUP_HASH。
# 用 lookahead / 精确锚点，避免误伤后面那些 base64 命令串。
$new = $text
$new = $new -replace '(?m)^(rem +)[0-9a-fA-F]{64}(?=\r?$)', ('${1}' + $newHash)
$new = $new -replace '(set "KNOWN=)[0-9a-fA-F]{64}(?=")',      ('${1}' + $newHash)
$new = $new -replace '(set "SETUP_HASH=)[0-9a-fA-F]{64}(?=")', ('${1}' + $newHash)

if ($new -eq $text) {
    Write-Host "[警告] 认出了旧校验值但没替换成功，请人工检查：$bat" -ForegroundColor Yellow
    exit 1
}

[System.IO.File]::WriteAllBytes($bat, $gbk.GetBytes($new))
Write-Host "已刷新 静默安装.bat 内嵌校验值：$oldHash -> $newHash" -ForegroundColor Green
