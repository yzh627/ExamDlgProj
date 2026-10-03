@echo off
chcp 65001 > nul
setlocal

rem ============================================================
rem  把发布包里的文件上传到 GitHub Release v1.0.0
rem
rem  走命令行上传，比在网页上拖文件稳（6 MB 的 exe 网页上传要等很久）。
rem  用法：双击本文件。
rem ============================================================

cd /d "%~dp0"

set "CFG=%USERPROFILE%\.ssh\config_examdlgproj"
set "CFG_FWD=%USERPROFILE:\=/%/.ssh/config_examdlgproj"
set "REPO=ssh://git@github-examdlgproj/yzh627/ExamDlgProj.git"
set "TAG=v1.0.0"
set "PKG=%~dp0发布包"

if not exist "%CFG%" (
    echo   找不到 SSH 配置：%CFG%
    echo.
    pause
    exit /b 1
)

echo ============================================================
echo   上传到 GitHub Release %TAG%
echo ============================================================
echo.

if not exist "%PKG%\对口升学练习系统_安装程序.zip" (
    echo   没找到 %PKG%\对口升学练习系统_安装程序.zip
    echo   请确认发布包文件夹还在。
    echo.
    pause
    exit /b 1
)

rem ---- Release 附件是走 git 标签的，所以文件必须先在仓库里 ----
rem 但【不要】把 发布包\ 整个加进 git：
rem   · 里面有 6 MB 的 exe 和 300 KB 的加密题库，会把仓库撑大
rem   · 安装包是每次重编都变的产物，不该进版本历史
rem 所以这里只强制加入要作为 Release 附件分发的两个文件，
rem 并在 .gitignore 里为它们开例外。

echo   1/3  把 zip 与说明文件加入版本控制...
git add -f "发布包/对口升学练习系统_安装程序.zip" "发布包/常见问题.txt"
if errorlevel 1 (
    echo   加入失败，跳过这一步（若文件已在仓库里属正常）。
)

echo.
echo   2/3  提交...
set "HADCHANGE="
for /f "tokens=*" %%i in ('git status --porcelain') do set "HADCHANGE=1"
if defined HADCHANGE (
    git commit -m "Add release assets: installer zip and FAQ"
) else (
    echo       没有变化，文件应已在仓库里。
)

echo.
echo   3/3  推送到 GitHub...
git -c "core.sshCommand=ssh -F %CFG_FWD%" push origin main
if errorlevel 1 (
    echo.
    echo   推送失败。检查网络或 SSH 配置。
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   已推送到 main 分支
echo ============================================================
echo.
echo   还差最后一步：把文件挂到 Release 的 Assets 上。
echo.
echo   网页操作：打开下面地址，点 Add files 上传
echo     https://github.com/yzh627/ExamDlgProj/releases/edit/v1.0.0
echo.
echo   要传的文件在：
echo     %PKG%
echo.
echo     对口升学练习系统_安装程序.zip   （学生下载这个）
echo     常见问题.txt
echo.
echo   建议：优先只传 zip（2.9 MB，上传快），学生解压就能用。
echo.
pause
endlocal
