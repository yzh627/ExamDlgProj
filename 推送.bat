@echo off
chcp 65001 > nul
setlocal

rem ============================================================
rem  推送到 GitHub（走 SSH）
rem
rem  为什么用 SSH 不用令牌：这台电脑的网络环境会把 HTTPS 流量劫持到一个
rem  本地代理上，导致 git push 报 502 / 连接重置。实测 SSH 通道
rem （github.com:22）是通的，所以改走 SSH。
rem
rem  【已配置好的内容】
rem   密钥：  %USERPROFILE%\.ssh\examdlgproj_deploy_ed25519
rem   配置：  %USERPROFILE%\.ssh\config_examdlgproj
rem   公钥已添加到 GitHub 账号 yzh627
rem
rem  免密登录已生效，正常情况下直接推送即可。
rem ============================================================

cd /d "%~dp0"

set "CFG=%USERPROFILE%\.ssh\config_examdlgproj"
set "REMOTE=ssh://git@github-examdlgproj/yzh627/ExamDlgProj.git"

if not exist "%CFG%" (
    echo   找不到 SSH 配置：%CFG%
    echo.
    echo   请联系维护者重新生成部署密钥。
    echo.
    pause
    exit /b 1
)

echo ============================================================
echo   推送到 GitHub（SSH 方式）
echo ============================================================
echo.

git status --short --branch
echo.

echo 正在推送...
echo.

git -c "core.sshCommand=ssh -F %CFG%" push -u "%REMOTE%" main

if errorlevel 1 (
    echo.
    echo ============================================================
    echo   推送失败
    echo ============================================================
    echo.
    echo   先试一次单独登录：
    echo     ssh -F "%CFG%" -T git@github-examdlgproj
    echo.
    echo   如果提示 Permission denied，说明 GitHub 上没有对应公钥，
    echo   把下面这个文件的全部内容加到 GitHub 即可：
    echo     %USERPROFILE%\.ssh\examdlgproj_deploy_ed25519.pub
    echo.
    echo   GitHub 页面：Settings -> SSH and GPG keys -> New SSH key
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   推送成功
echo ============================================================
echo.
echo   查看构建：https://github.com/yzh627/ExamDlgProj/actions
echo.
pause
endlocal
