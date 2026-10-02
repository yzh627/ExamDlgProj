# 发布前安全自检：私钥泄露排查 + 签名状态 + 加固标志 + 产物一致性
#
# 只读脚本：不修改任何文件、不碰证书私钥、不改系统设置。
# 可以随时安全地重复运行。
#
# 用法：双击运行，或
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_signing.ps1
#
# 退出码：0=全部通过；1=有阻断项（未签名 / 私钥入库 / 签名后被改）

$ErrorActionPreference = 'Stop'
$proj = Split-Path $PSScriptRoot -Parent
$pkg  = Join-Path $proj '发布包'

$script:fail = 0
$script:warn = 0

function Say($text, $color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Bad($text) { $script:fail++; Write-Host "  [阻断] $text" -ForegroundColor Red }
function Warn($text) { $script:warn++; Write-Host "  [警告] $text" -ForegroundColor Yellow }
function Good($text) { Write-Host "  [通过] $text" -ForegroundColor Green }

Say ""
Say "==================== 发布前安全自检 ====================" -Color Cyan
Say "工程：$proj"

# ============================================================
# 第 1 项：私钥泄露前置检查（最高优先级）
# ============================================================
Say ""
Say "---- 1. 私钥泄露排查 ----" -Color Cyan

# 1a. 磁盘上不该存在任何私钥材料
$keyExts = @('*.pfx', '*.p12', '*.pem', '*.key', '*.p8', '*.jks', '*.keystore')
$found = @()
foreach ($pat in $keyExts) {
    $found += Get-ChildItem -LiteralPath $proj -Recurse -File -Filter $pat -ErrorAction SilentlyContinue
}
# 顺手排掉构建中间目录里的噪声
$found = $found | Where-Object { $_.FullName -notmatch '\\(x64|x86|Debug|Release|\.git)\\' }
if ($found) {
    Bad "工程目录里发现疑似私钥/证书文件 $($found.Count) 个（私钥绝不能入库！）："
    foreach ($f in $found) { Write-Host "         $($f.FullName)" -ForegroundColor Red }
} else {
    Good "工程目录无私钥文件（pfx/p12/pem/key/p8/jks/keystore）"
}

# 1b. 脚本与配置里不该出现明文口令
$scanFiles = @()
$scanFiles += Get-ChildItem -LiteralPath $proj -Recurse -File -Include *.ps1, *.bat, *.cmd, *.yml, *.yaml, *.json, *.md, *.txt -ErrorAction SilentlyContinue
$scanFiles = $scanFiles | Where-Object { $_.FullName -notmatch '\\(\.git|x64|x86)\\' }
$secretHits = @()
foreach ($f in $scanFiles) {
    try {
        $t = Get-Content -LiteralPath $f.FullName -Raw -ErrorAction Stop
        if (-not $t) { continue }
        # 注意：这四个正则一律用双引号串。PowerShell 的单引号串里写不出 '，用单引号会直接解析报错。
        foreach ($pat in @("SIGNTOOL_PFX_PASSWORD\s*=\s*`"[^`"]+",
                           "password\s*=\s*`"[^`"]{3,}",
                           'PRIVATE\s*KEY',
                           'BEGIN\s+(RSA\s+)?PRIVATE')) {
            if ($t -match $pat) { $secretHits += "$($f.Name)  ← 疑似明文口令/私钥字样" }
        }
    } catch { }
}
if ($secretHits) {
    Bad "以下文件疑似含明文口令或私钥字样："
    $secretHits | Select-Object -Unique | ForEach-Object { Write-Host "         $_" -ForegroundColor Red }
} else {
    Good "脚本与配置里未发现明文口令/私钥字样"
}

# 1c. 确认走的是 HSM 引用而不是 PFX 文件
$rb = Join-Path $proj 'tools\rebuild.ps1'
if (Test-Path $rb) {
    $rbText = Get-Content $rb -Raw
if ($rbText -match 'signtool' -and $rbText -match '/kc\s+\$certRef') {
    Good "rebuild.ps1 用证书保管库引用签名（私钥不出 HSM），未使用 -f PFX"
} elseif ($rbText -match 'signtool\s+sign[^\r\n]*\s-f\s') {
    Bad "rebuild.ps1 疑似用 -f 指定 PFX 文件签名 —— 私钥会落盘，必须改用保管库引用"
} else {
    Warn "rebuild.ps1 里没识别到签名调用（可能还没接入，或写法需人工确认）"
}
# HSM 路线别用错：signtool 的 /kc 只对 DigiCert KeyLocker 有效，对 Azure Key Vault 无效
if ($rbText -match "hsm\s*=\s*'azure'" -and $rbText -match 'AzureSignTool') {
    Good "已按 HSM 区分工具链：Azure 路线用 AzureSignTool，不会误用 signtool /kc"
}
if ($rbText -match "SIGNTOOL_KV_CLIENTSECRET") {
    Warn "脚本里引用了 Azure 客户端密钥环境变量 —— 确认它的值只来自 CI 密钥库，不在任何文件里硬编码"
}
}

# ============================================================
# 第 2 项：签名状态
# ============================================================
Say ""
Say "---- 2. 签名状态 ----" -Color Cyan

$targets = @(
    @{ Path = "$pkg\对口升学练习系统_安装程序.exe"; Name = '安装程序（学生双击的就是它）' },
    @{ Path = "$pkg\ExamDlgProj.exe";           Name = '主程序 x64' },
    @{ Path = "$pkg\ExamDlgProj_32位.exe";      Name = '主程序 x86' },
    @{ Path = "$proj\安装程序\Uninst.exe";      Name = '卸载程序' }
)
$hashes = @{}
foreach ($t in $targets) {
    if (-not (Test-Path $t.Path)) {
        Bad "文件不存在：$($t.Path)"
        continue
    }
    $h = (Get-FileHash -LiteralPath $t.Path -Algorithm SHA256).Hash
    $hashes[$t.Path] = $h
    $sig = Get-AuthenticodeSignature -LiteralPath $t.Path
    Write-Host ""
    Write-Host "  文件：$($t.Name)"
    Write-Host "    路径：$($t.Path)"
    Write-Host "    SHA256：$h"
    if ($sig.Status -eq 'Valid') {
        Good "签名状态 Valid"
        Write-Host "    签名者：$($sig.SignerCertificate.Subject)"
        Write-Host "    证书到期：$($sig.SignerCertificate.NotAfter)"
        if ($sig.TimeStamperCertificate) {
            Good "含 RFC3161 时间戳（证书到期后签名仍有效）"
        } else {
            Bad "缺时间戳 —— 证书到期后签名会失效"
        }
    } else {
        Bad "签名状态 = $($sig.Status)"
        Write-Host "    影响：SmartScreen / Smart App Control 会拦截；Gatekeeper 必拦" -ForegroundColor Red
    }
}

# ============================================================
# 第 3 项：二进制加固标志
# ============================================================
Say ""
Say "---- 3. 加固标志（ASLR / DEP / CFG） ----" -Color Cyan

function Get-PEMit($path) {
    $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Position = 0x3C
        $pe = $br.ReadInt32()
        $fs.Position = $pe + 24
        $magic = $br.ReadUInt16()
        $is64 = ($magic -eq 0x20B)
        $optSize = if ($is64) { 240 } else { 224 }
        $fs.Position = $pe + 24 + 70
        $dllc = $br.ReadUInt16()
        $nsec = 0
        $fs.Position = $pe + 6
        $nsec = $br.ReadUInt16()
        $secOff = $pe + 24 + $optSize
        $maxEnd = 0
        for ($i = 0; $i -lt $nsec; $i++) {
            $fs.Position = $secOff + $i * 40
            $null = $br.ReadBytes(8); $null = $br.ReadUInt32(); $null = $br.ReadUInt32()
            $rsz = $br.ReadUInt32(); $rptr = $br.ReadUInt32()
            if (($rptr + $rsz) -gt $maxEnd) { $maxEnd = $rptr + $rsz }
        }
        $fs.Position = 0
        $fs.Seek(0, 'End') | Out-Null
        $endpos = $fs.Position
        return [pscustomobject]@{
            Is64 = $is64
            DllCharacteristics = $dllc
            ASLR = [bool]($dllc -band 0x40)
            DEP  = [bool]($dllc -band 0x100)
            CFG  = [bool]($dllc -band 0x4000)
            Overlay = $endpos - $maxEnd
        }
    } finally { $fs.Close() }
}

foreach ($t in $targets) {
    if (-not (Test-Path $t.Path)) { continue }
    $m = Get-PEMit $t.Path
    $arch = if ($m.Is64) { 'x64' } else { 'x86' }
    # 注意：格式串要先算成变量再传给 Write-Host。
    # 直接写 Write-Host "x={0}" -f $v 会让 -f 被当成 Write-Host 自己的参数，导致绑定错误。
    $line = "  文件：$($t.Name)  [$arch]  DllCharacteristics=0x{0:X4}" -f $m.DllCharacteristics
    Write-Host $line
    if ($m.ASLR) { Good "ASLR 已开" } else { Bad "ASLR 未开" }
    if ($m.DEP)  { Good "DEP/NX 已开" } else { Bad "DEP/NX 未开" }
    if ($m.CFG)  { Good "CFG 已开" } else { Bad "CFG 未开 —— 需在 rebuild.ps1 的 cl 与 link 行加 /guard:cf" }
}

# ============================================================
# 第 4 项：产物一致性（签名后是否被改）
# ============================================================
Say ""
Say "---- 4. 产物一致性（签名后是否被改过） ----" -Color Cyan

# 4a. bat 内嵌 SHA256 必须等于磁盘上安装程序的实际哈希
$batPath = "$pkg\静默安装.bat"
$instPath = "$pkg\对口升学练习系统_安装程序.exe"
if ((Test-Path $batPath) -and (Test-Path $instPath)) {
    $bat = Get-Content $batPath -Raw
    $batHashes = [regex]::Matches($bat, '[0-9A-Fa-f]{64}') | ForEach-Object { $_.Value.ToUpper() }
    $actual = $hashes[$instPath]
    if ($batHashes -contains $actual) {
        Good "静默安装.bat 内嵌哈希 == 磁盘安装程序实际哈希（bat 未过期）"
    } else {
        Bad "静默安装.bat 内嵌哈希与磁盘实际哈希不一致 —— 签名或重编后忘了同步 bat"
        Write-Host "    bat 里的值：$($batHashes -join ', ')" -ForegroundColor Red
        Write-Host "    实际的值  ：$actual" -ForegroundColor Red
        Write-Host "    修法：跑 tools\sync_silent_bat.ps1" -ForegroundColor Yellow
    }
} else {
    Warn "没找到 静默安装.bat 或安装程序，跳过这项"
}

# 4b. zip 内成员必须与磁盘一致
$zipPath = "$pkg\对口升学练习系统_安装程序.zip"
if ((Test-Path $zipPath) -and (Test-Path $instPath)) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    try {
        $za = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
        $entry = $za.Entries | Where-Object { $_.FullName -like '*.exe' } | Select-Object -First 1
        if ($entry) {
            $s = $entry.Open()
            $ms = New-Object System.IO.MemoryStream
            $s.CopyTo($ms); $s.Close()
            $zh = [System.BitConverter]::ToString(
                [System.Security.Cryptography.SHA256]::Create().ComputeHash($ms.ToArray())).Replace('-', '')
            $za.Dispose()
            $ah = $hashes[$instPath]
            if ($zh -eq $ah) {
                Good "zip 内安装程序 == 磁盘安装程序（zip 未过期）"
            } else {
                Bad "zip 内安装程序与磁盘不一致 —— 重新打的 zip 过期了"
                Write-Host "    zip 内：$zh" -ForegroundColor Red
                Write-Host "    磁盘  ：$ah" -ForegroundColor Red
            }
        } else {
            $za.Dispose()
            Warn "zip 里没找到 exe 成员"
        }
    } finally {
        $zaDispose = $true
    }
} else {
    Warn "没找到 zip 或安装程序，跳过 zip 一致性检查"
}

# 4c. 两处安装程序副本必须一致
$altInst = "$proj\安装程序\对口升学练习系统_安装程序.exe"
if ((Test-Path $altInst) -and (Test-Path $instPath)) {
    $ah2 = (Get-FileHash -LiteralPath $altInst -Algorithm SHA256).Hash
    if ($hashes[$instPath] -eq $ah2) {
        Good "安装程序\ 与 发布包\ 两份安装程序完全一致"
    } else {
        Warn "两份安装程序不一致：安装程序\=$ah2  发布包\=$($hashes[$instPath])（发布前请重跑 rebuild.ps1）"
    }
}

# ============================================================
# 汇总
# ============================================================
Say ""
Say "==================== 自检汇总 ====================" -Color Cyan
Say "  阻断项：$script:fail"
Say "  警告项：$script:warn"
if ($script:fail -gt 0) {
    Say ""
    Say "  结论：不通过。有阻断项，不建议发布。" -Color Red
    Say "  说明：未签名不等于有病毒，但会被系统拦；请走正规代码签名流程解决。" -Color Red
    Say "  不要用关闭防护 / 加壳 / 共享证书 / 所谓'免杀'的方式绕过。" -Color Red
    exit 1
} else {
    Say ""
    Say "  结论：技术侧通过。" -Color Green
    if ($script:warn -gt 0) {
        Say "  仍有 $script:warn 项警告需人工确认；且技术通过 ≠ 干净机一定不拦，" -Color Yellow
        Say "  仍需在真实干净机（Defender 实时防护开启）上按验收步骤实测。" -Color Yellow
    }
    exit 0
}
