# 一键重新编译：64位主程序 + 32位主程序 + 安装程序，并更新 发布包
# 用法：双击工程根目录的 一键重建.bat
$ErrorActionPreference = 'Stop'
$proj = Split-Path $PSScriptRoot -Parent
Set-Location $proj

function Find-VcTools {
    # 在 Visual Studio 安装目录里找 MSVC 工具集和 Windows SDK
    $vsRoots = @("$env:ProgramFiles\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio")
    foreach ($root in $vsRoots) {
        if (-not (Test-Path $root)) { continue }
        foreach ($d1 in (Get-ChildItem $root -Directory -ErrorAction SilentlyContinue)) {
            foreach ($d2 in (Get-ChildItem $d1.FullName -Directory -ErrorAction SilentlyContinue)) {
            $msvc = Join-Path $d2.FullName 'VC\Tools\MSVC'
            if (Test-Path $msvc) {
                # 同样只认 14.44.35207 这种真实版本号。
                # MSVC 目录下偶有 preview/exp 子目录，靠字符串排序会选错。
                $ver = Get-ChildItem $msvc -Directory |
                       Where-Object { $_.Name -match '^\d+\.\d+' } |
                       Sort-Object { [version]$_.Name } -Descending |
                       Select-Object -First 1
                if ($ver) {
                    # 再确认编译器真的在，只有一个带 cl.exe 的版本才算数
                    if (Test-Path (Join-Path $ver.FullName 'bin\Hostx64\x64\cl.exe')) {
                        return @{
                            Msvc = $ver.FullName
                            Vs   = $d2.FullName
                        }
                    }
                }
            }
            }
        }
    }
    return $null
}

function Find-Sdk {
    $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
    if (-not (Test-Path $sdkRoot)) { $sdkRoot = "$env:ProgramFiles\Windows Kits\10" }
    $inc = Join-Path $sdkRoot 'Include'
    if (-not (Test-Path $inc)) { return $null }
    # 版本目录形如 10.0.22621.0，wdf 之类功能目录不认
    $ver = Get-ChildItem $inc -Directory |
           Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $ver) { return $null }
    return @{ Root = $sdkRoot; Ver = $ver.Name }
}

$vc = Find-VcTools
$sdk = Find-Sdk
if (-not $vc -or -not $sdk) {
    Write-Host "[错误] 没找到 Visual Studio 的编译工具或 Windows SDK。" -ForegroundColor Red
    Write-Host "       请用 Visual Studio 打开 ExamDlgProj.vcxproj，手动'重新生成解决方案'。"
    exit 1
}

Write-Host "编译器: $($vc.Msvc)"
Write-Host "SDK   : $($sdk.Root)\Include\$($sdk.Ver)"

$cl64 = Join-Path $vc.Msvc 'bin\Hostx64\x64\cl.exe'
$cl32 = Join-Path $vc.Msvc 'bin\Hostx64\x86\cl.exe'
$ln64 = Join-Path $vc.Msvc 'bin\Hostx64\x64\link.exe'
$ln32 = Join-Path $vc.Msvc 'bin\Hostx64\x86\link.exe'
$rc = Join-Path $sdk.Root "bin\$($sdk.Ver)\x64\rc.exe"
$mt = Join-Path $sdk.Root "bin\$($sdk.Ver)\x64\mt.exe"
if (-not (Test-Path $mt)) { throw "没找到 mt.exe（清单工具）：$mt" }
# link.exe 在做 /MANIFEST:EMBED 时会自己去调 mt.exe，所以必须让它在 PATH 里能找到，
# 否则报 LNK1158: 无法运行"mt.exe"
$env:PATH = (Split-Path $mt -Parent) + ";" + $env:PATH

function Build-Config([string]$arch, [string]$clExe, [string]$lnExe) {
    $lib = Join-Path $vc.Msvc "lib\$arch"
    $atlmfc = Join-Path $vc.Msvc "atlmfc\lib\$arch"
    $sdkLibUcrt = Join-Path $sdk.Root "Lib\$($sdk.Ver)\ucrt\$arch"
    $sdkLibUm = Join-Path $sdk.Root "Lib\$($sdk.Ver)\um\$arch"
    $incSdk = Join-Path $sdk.Root "Include\$($sdk.Ver)"

    $env:INCLUDE = "$($vc.Msvc)\include;$($vc.Msvc)\atlmfc\include;$incSdk\ucrt;$incSdk\shared;$incSdk\um"
    $env:LIB = "$lib;$atlmfc;$sdkLibUcrt;$sdkLibUm"
    $outDir = Join-Path $proj "x$($arch -replace 'x64','64' -replace 'x86','86')\Release"
    if ($arch -eq 'x64') { $outDir = Join-Path $proj 'x64\Release' } else { $outDir = Join-Path $proj 'x86\Release' }
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $env:TMP = $outDir; $env:TEMP = $outDir

    Write-Host "`n===== 编译 $arch =====" -ForegroundColor Cyan
    & $rc /nologo /fo "$outDir\ExamDlgProj.res" /d_NDEBUG "ExamDlgProj.rc"
    if ($LASTEXITCODE -ne 0) { throw "资源编译失败 ($arch)" }
    # /guard:cf 开启控制流保护（CFG）。这一项原来完全没开，验收时属于"缓解不完整"。
    # /GS 显式写出（MSVC 在 Release 下默认开，但写成显式参数才能保证不被默认值悄悄改掉）。
    # 两者都是发布质量门禁，与代码签名是同一条链上的事，一起补齐。
    & $clExe /nologo /c /EHsc /MT /O2 /GL /W3 /guard:cf /GS /D_WINDOWS /DNDEBUG /DUNICODE /D_UNICODE /Fo"$outDir\\" pch.cpp ExamDlgProj.cpp ExamDlgProjDlg.cpp PracticeDlg.cpp QuestionBank.cpp
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($arch)" }
    # 清单：/MANIFEST:EMBED 让链接器把 app.manifest（DPI 感知、supportedOS、长路径）
    # 和它自己生成的那份（asInvoker + 公共控件 6.0，架构自动正确）合并后【内嵌】进 exe。
    # 注意 /MANIFESTINPUT 必须配 /MANIFEST:EMBED，否则链接器直接报 LNK1220。
    # /DYNAMICBASE(ASLR) /NXCOMPAT(DEP) 原来靠默认打开，这里显式写死，防止默认值变化导致
    # 某个架构悄悄丢掉缓解措施。/CETCOMPAT(x64 影子栈) 是双保险，x86 上不支持所以不加。
    $cetFlag = if ($arch -eq 'x64') { '/CETCOMPAT' } else { '' }
    & $lnExe /nologo /subsystem:windows /LTCG /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /guard:cf $cetFlag /entry:wWinMainCRTStartup /MANIFEST:EMBED /MANIFESTINPUT:"$proj\app.manifest" /out:"$outDir\ExamDlgProj.exe" "$outDir\pch.obj" "$outDir\ExamDlgProj.obj" "$outDir\ExamDlgProjDlg.obj" "$outDir\PracticeDlg.obj" "$outDir\QuestionBank.obj" "$outDir\ExamDlgProj.res"
    if ($LASTEXITCODE -ne 0) { throw "链接失败 ($arch)" }

    # 再把内嵌的清单导出成同目录的 .exe.manifest。两个用处：
    #   1) 安装程序要内嵌这份文件（Setup.rc 里的 IDR_PAYLOAD_MANIFEST）
    #   2) 导出来的和 exe 里内嵌的必然一字不差，永远不会出现"内嵌和外挂对不上"
    & $mt -nologo "-inputresource:$outDir\ExamDlgProj.exe;#1" "-out:$outDir\ExamDlgProj.exe.manifest"
    if ($LASTEXITCODE -ne 0) { throw "导出清单失败 ($arch)" }

    # 自检：清单里必须真的含 dpiAware / supportedOS，否则等于白改
    $mfText = Get-Content "$outDir\ExamDlgProj.exe.manifest" -Raw
    if ($mfText -notmatch 'dpiAware') { throw "清单自检失败：没有 dpiAware ($arch)" }
    if ($mfText -notmatch 'supportedOS') { throw "清单自检失败：没有 supportedOS ($arch)" }
    Write-Host "完成：$outDir\ExamDlgProj.exe（清单已内嵌，含 DPI 感知）" -ForegroundColor Green
}

Build-Config 'x64' $cl64 $ln64
Build-Config 'x86' $cl32 $ln32

# ===== 重新打包加密题库：questions.txt -> questions.dat =====
# 以前这一步要手动跑，忘了就会出现"题改了但学生机上还是旧题"。
# 位置很关键：必须赶在编译安装程序之前 —— 安装程序会把 questions.dat 内嵌进去。
#
# CI 注意：这一步会实际运行 ExamDlgProj.exe /pack（GUI 子系统程序）。
# 在 GitHub Actions 的 windows runner 上是可行的（runner 有桌面会话），
# 但比本机慢，最多可能等满下面的 60 秒超时。
# 若 CI 上这一步超时，说明 runner 上无法启动 GUI 子系统程序，
# 届时可改用预先打包好的 questions.dat 提交到仓库（注意：那会让题库明文入库）。
Write-Host "`n===== 重新打包加密题库 =====" -ForegroundColor Cyan
$txtBank = Join-Path $proj 'questions.txt'
if (Test-Path $txtBank) {
    $relDir = Join-Path $proj 'x64\Release'
    Copy-Item $txtBank (Join-Path $relDir 'questions.txt') -Force
    $exeRel = Join-Path $relDir 'ExamDlgProj.exe'
    $newDat = Join-Path $relDir 'questions.dat'
    # 先删旧的：否则打包万一没成功，脚本会把上一次的旧 dat 当成新产物拷出去，
    # 结果就是"改了题但学生机上还是旧题"，而且没有任何报错。
    if (Test-Path $newDat) { [System.IO.File]::Delete($newDat) }
    $packStart = Get-Date
    & $exeRel /pack | Out-Null
    # ExamDlgProj.exe 是 GUI 子系统程序，PowerShell 的 & 不保证等它退出，
    # 所以要轮询确认 questions.dat 真的被写出来了（带时间戳，防认错文件）。
    $deadline = $packStart.AddSeconds(60)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $newDat) -and ((Get-Item $newDat).LastWriteTime -ge $packStart)) { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path $newDat)) { throw "题库打包失败：questions.dat 没有被生成" }
    if ((Get-Item $newDat).LastWriteTime -lt $packStart) { throw "题库打包失败：questions.dat 还是旧的" }
    Copy-Item $newDat (Join-Path $proj 'questions.dat') -Force
    Write-Host ("题库已重打：" + (Get-Item (Join-Path $proj 'questions.dat')).Length + " 字节（明文 " + (Get-Item $txtBank).Length + " 字节）") -ForegroundColor Green
} else {
    Write-Host "没有 questions.txt，跳过题库重新打包" -ForegroundColor Yellow
}

Write-Host "`n===== 编译安装程序 =====" -ForegroundColor Cyan
$env:INCLUDE = "$($vc.Msvc)\include;$($vc.Msvc)\atlmfc\include;$($sdk.Root)\Include\$($sdk.Ver)\ucrt;$($sdk.Root)\Include\$($sdk.Ver)\shared;$($sdk.Root)\Include\$($sdk.Ver)\um"
$env:LIB = "$($vc.Msvc)\lib\x64;$($vc.Msvc)\atlmfc\lib\x64;$($sdk.Root)\Lib\$($sdk.Ver)\ucrt\x64;$($sdk.Root)\Lib\$($sdk.Ver)\um\x64"
Set-Location (Join-Path $proj '安装程序')
$env:TMP = Join-Path $proj 'x64\Debug'; $env:TEMP = $env:TMP
# 先编"专用卸载程序"（安装程序要把它内嵌进去，所以必须在 Setup.rc 之前编好）
& $rc /nologo /fo "Uninst.res" "Uninst.rc"
if ($LASTEXITCODE -ne 0) { throw "卸载程序资源编译失败" }
& $cl64 /nologo /c /EHsc /MT /O2 /W3 /guard:cf /GS /DUNICODE /D_UNICODE /DPURE_UNINSTALL /Fo"Uninst.obj" Setup.cpp
if ($LASTEXITCODE -ne 0) { throw "卸载程序编译失败" }
& $ln64 /nologo /subsystem:windows /DYNAMICBASE /NXCOMPAT /guard:cf /CETCOMPAT /entry:wWinMainCRTStartup /out:"Uninst.exe" Uninst.obj Uninst.res
if ($LASTEXITCODE -ne 0) { throw "卸载程序链接失败" }
Write-Host "完成：安装程序\\Uninst.exe（专用卸载程序，约几十 KB）" -ForegroundColor Green

& $rc /nologo /fo "Setup.res" "Setup.rc"
if ($LASTEXITCODE -ne 0) { throw "安装程序资源编译失败" }
& $cl64 /nologo /c /EHsc /MT /O2 /W3 /guard:cf /GS /DUNICODE /D_UNICODE Setup.cpp
if ($LASTEXITCODE -ne 0) { throw "安装程序编译失败" }
& $ln64 /nologo /subsystem:windows /DYNAMICBASE /NXCOMPAT /guard:cf /CETCOMPAT /entry:wWinMainCRTStartup /out:"对口升学练习系统_安装程序.exe" Setup.obj Setup.res
if ($LASTEXITCODE -ne 0) { throw "安装程序链接失败" }

Write-Host "`n===== 更新发布包 =====" -ForegroundColor Cyan
Set-Location $proj
$pkg = Join-Path $proj '发布包'
New-Item -ItemType Directory -Force -Path $pkg | Out-Null
Copy-Item "$proj\x64\Release\ExamDlgProj.exe"            "$pkg\ExamDlgProj.exe" -Force
Copy-Item "$proj\x64\Release\ExamDlgProj.exe.manifest"   "$pkg\ExamDlgProj.exe.manifest" -Force
Copy-Item "$proj\x86\Release\ExamDlgProj.exe"            "$pkg\ExamDlgProj_32位.exe" -Force
# 32 位 exe 以前在发布包里没有对应清单（旧清单里还写死 amd64），单独发这个 exe 会是经典外观
Copy-Item "$proj\x86\Release\ExamDlgProj.exe.manifest" "$pkg\ExamDlgProj_32位.exe.manifest" -Force
Copy-Item "$proj\安装程序\对口升学练习系统_安装程序.exe"  "$pkg\对口升学练习系统_安装程序.exe" -Force
if (Test-Path "$proj\questions.dat") { Copy-Item "$proj\questions.dat" "$pkg\questions.dat" -Force }
if (Test-Path "$proj\support.png") { Copy-Item "$proj\support.png" "$pkg\support.png" -Force }
if (Test-Path "$proj\说明.txt") { Copy-Item "$proj\说明.txt" "$pkg\说明.txt" -Force }
# 说明文字也拷一份到输出目录，方便开发时直接改
if (Test-Path "$proj\说明.txt") {
    Copy-Item "$proj\说明.txt" "$proj\x64\Release\说明.txt" -Force
    Copy-Item "$proj\说明.txt" "$proj\x86\Release\说明.txt" -Force
}

# ===== 代码签名（骨架，默认不执行）=====
# 位置很关键：必须在 发布包 更新完之后、刷新 bat 内嵌校验值之前。
# 因为 Authenticode 签名会改变安装程序的 SHA256 —— 签完名再同步 bat，bat 里才是对的。
# 顺序反过来（先同步 bat 再签名），bat 就会拿着签名前的旧哈希，
# 静默安装时永远走"次优路径"，而且不报错，很难查。
#
# ★ 私钥红线 ★
# 本脚本只接受"证书保管库引用名"（环境变量 SIGNTOOL_CERT_REF），不接触私钥本身。
# 私钥留在云 HSM / 硬件令牌里，永远不导出、永远不落盘、永远不进命令行。
# 严禁把 PFX 密码写进本脚本、bat、日志或任何聊天记录。
# 严禁改用 -f 指定 PFX 文件的方式签名 —— 那等于把私钥搬到磁盘上。
# 需要换证书时改环境变量即可，脚本本身不用动。
#
# ★ 两家云 HSM 的命令行不一样，别混用 ★
#   DigiCert KeyLocker —— 用 Windows 自带 signtool.exe
#       签名走 KSP：/csp "DigiCert Signing Manager KSP" /kc <别名>
#   Azure Key Vault  —— 必须用第三方工具 AzureSignTool
#       signtool.exe 的 /kc 对 Azure Key Vault 无效，签名会失败
#   所以用 SIGNTOOL_HSM 指定走哪条：keylocker（默认） / azure
#   未显式指定时按"有 AzureSignTool 就用 Azure，否则用 KeyLocker"自动判断。
$signtool   = Join-Path $sdk.Root "bin\$($sdk.Ver)\x64\signtool.exe"
$certRef    = $env:SIGNTOOL_CERT_REF
$hsm        = $env:SIGNTOOL_HSM
$tsUrl      = if ($env:SIGNTOOL_TIMESTAMP_URL) { $env:SIGNTOOL_TIMESTAMP_URL } else { 'http://timestamp.digicert.com' }
$azSignTool = $env:SIGNTOOL_AZURE_SIGNER   # AzureSignTool.exe 的完整路径

# 决定用哪条路线：显式指定 > 自动判断
if (-not $hsm) {
    $hsm = if ($azSignTool -and (Test-Path $azSignTool)) { 'azure' } else { 'keylocker' }
}

# 要签的 4 个文件：先内层主程序，再外层安装器（安装器是学生实际双击的那个，最关键）
$toSign = @(
    "$pkg\ExamDlgProj.exe",
    "$pkg\ExamDlgProj_32位.exe",
    "$proj\安装程序\Uninst.exe",
    "$pkg\对口升学练习系统_安装程序.exe"
)

if (-not $certRef) {
    Write-Host "`n===== 代码签名：跳过 =====" -ForegroundColor Yellow
    Write-Host "  没有设置 SIGNTOOL_CERT_REF 环境变量，本轮产物保持【未签名】。" -ForegroundColor Yellow
    Write-Host "  未签名会被 SmartScreen / Smart App Control 拦截，发布前必须签名。" -ForegroundColor Yellow
    Write-Host "  签名前请先跑 tools\check_release_signing.ps1 确认基线。" -ForegroundColor Yellow
} else {
    if (-not (Test-Path $signtool)) {
        throw "没找到 signtool.exe：$signtool（代码签名已启用但工具缺失，中止以免产出未签名包）"
    }
    # Azure 路线要额外有 AzureSignTool；KeyLocker 路线要装好 KSP 并同步过证书
    if ($hsm -eq 'azure' -and (-not $azSignTool -or -not (Test-Path $azSignTool))) {
        throw "选了 azure 路线，但找不到 AzureSignTool。请设置 SIGNTOOL_AZURE_SIGNER 指向它的完整路径（dotnet tool install --global AzureSignTool）。"
    }
    Write-Host "`n===== 代码签名 =====" -ForegroundColor Cyan
    Write-Host "  HSM 路线：$hsm"
    Write-Host "  证书引用：$certRef"
    Write-Host "  时间戳：  $tsUrl"
    Write-Host "  私钥不出云 HSM，本机不落地任何密钥材料。"

    foreach ($f in $toSign) {
        if (-not (Test-Path $f)) { throw "待签名文件不存在：$f" }
        Write-Host "  签名：$(Split-Path $f -Leaf)"
        # /fd SHA256：文件摘要用 SHA-256（不再用旧的 SHA-1）
        # /tr + /td SHA256：RFC3161 时间戳，证书到期后签名依然有效，且抗伪造
        if ($hsm -eq 'azure') {
            # Azure Key Vault 专用。signtool 的 /kc 对 Azure 无效，必须用 AzureSignTool。
            & $azSignTool sign -kvu $env:SIGNTOOL_KV_URL -kvc $certRef `
                               -kvt $env:SIGNTOOL_KV_TENANT -kvi $env:SIGNTOOL_KV_CLIENTID `
                               -kvs $env:SIGNTOOL_KV_CLIENTSECRET `
                               -fd sha256 -tr $tsUrl -td sha256 $f
        } else {
            # DigiCert KeyLocker：走 Windows KSP，/csp 指定 KSP 提供者，/kc 给密钥对别名
            & $signtool sign /csp 'DigiCert Signing Manager KSP' /fd SHA256 /tr $tsUrl /td SHA256 /kc $certRef $f
        }
        if ($LASTEXITCODE -ne 0) { throw "签名失败：$f（退出码 $LASTEXITCODE）—— 中止发布，绝不放过未签名产物" }
    }

    # 签名后立即自检：这一步是"签名没生效"和"文件签完被改"的第一道闸门。
    # 只要有一条不过就中止 —— 未签名包流到学生手上的代价，远大于停下来重跑一次。
    Write-Host "  验签自检："
    foreach ($f in $toSign) {
        $sig = Get-AuthenticodeSignature -LiteralPath $f
        if ($sig.Status -ne 'Valid') {
            throw "验签未通过：$(Split-Path $f -Leaf) 状态=$($sig.Status)"
        }
        if (-not $sig.TimeStamperCertificate) {
            throw "缺少时间戳：$(Split-Path $f -Leaf) —— 未带 /tr 的签名，证书到期即失效"
        }
        Write-Host ("    [OK] {0}  签名者={1}  证书到期={2}  含时间戳=是" -f `
            (Split-Path $f -Leaf), $sig.SignerCertificate.Subject, $sig.SignerCertificate.NotAfter) -ForegroundColor Green
    }
}

# ===== 刷新 静默安装.bat 内嵌的安装程序校验值 =====
# 静默安装.bat 靠一个内嵌的 SHA256 认自家安装程序。安装程序每次重编/重签哈希都会变，
# 忘了同步不会报错 —— 只会让 bat 退到「文件版本信息」那条次优路径，
# 提示语变成"版本与内嵌校验值不同"，看着让人心里没底。所以这里自动做掉。
# 放在最后：必须等签名步骤跑完 —— 签名改的就是这个文件的哈希。
$syncBat = Join-Path $proj 'tools\sync_silent_bat.ps1'
if (Test-Path $syncBat) {
    # 用子进程跑：脚本里用 exit 返回状态，隔一层进程最干净
    & powershell -NoProfile -ExecutionPolicy Bypass -File $syncBat
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[警告] 静默安装.bat 校验值同步失败（退出码 $LASTEXITCODE），请人工检查。" -ForegroundColor Yellow
    }
} else {
    Write-Host "[跳过] 没有 $syncBat" -ForegroundColor Yellow
}

Write-Host "`n全部完成！发布包已更新：$pkg" -ForegroundColor Green
Get-ChildItem $pkg | Select-Object Name, Length | Format-Table -AutoSize
