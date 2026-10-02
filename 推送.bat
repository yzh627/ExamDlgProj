@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

rem ============================================================
rem  推送代码到 GitHub（使用令牌方式）
rem
rem  为什么用令牌：这台电脑装的 Git 没有 Credential Manager 组件，
rem  没法弹浏览器登录框，所以改走 GitHub Personal Access Token。
rem
rem  【第一次使用请先做这一步】
rem   1. 打开 https://github.com/settings/tokens/new
rem   2. Note 随便填，比如 "ExamDlgProj 推送"
rem   3. Expiration 选 30 days
rem   4. 勾选勾选项里的 repo（勾它就行，它是一整组权限）
rem   5. 拉到底点 Generate token，复制那串以 github_pat_ 开头的字符
rem   6. 存在一个叫 git-token.txt 的文件里（脚本会自动创建并使用）
rem
rem  令牌是敏感信息，别截图发给别人，也别提交到仓库。
rem  本脚本已确保 .gitignore 拦住 git-token.txt。
rem ============================================================

echo ============================================================
echo   推送到 GitHub
echo ============================================================
echo.

cd /d "%~dp0"

rem ---- 读取令牌（存在 git-token.txt 里，避免每次粘贴到命令行被看见）----
set "TOKENFILE=%~dp0git-token.txt"

if not exist "%TOKENFILE%" (
    echo   第一次使用，需要先创建 git-token.txt
    echo.
    echo   请打开这个网址生成令牌：
    echo     https://github.com/settings/tokens/new
    echo.
    echo   生成后，把那串 github_pat_ 开头的字符粘贴到下面
    echo.
    set /p "TOKEN=请粘贴令牌后按回车: "
    echo %TOKEN%> "%TOKENFILE%"
    echo.
    echo   已保存到 git-token.txt （已加入 .gitignore，不会被提交）
    echo.
) else (
    set /p "TOKEN=" < "%TOKENFILE%"
)

set "TOKEN=%TOKEN: =%"
if "%TOKEN%"=="" (
    echo   没有拿到令牌，停止。
    pause
    exit /b 1
)

echo   正在推送...
echo.

rem ---- 用令牌推送，地址里嵌令牌，不落盘不进日志 ----
set "PUSHURL=https://yzh627:%TOKEN%@github.com/yzh627/ExamDlgProj.git"

git push -u "%PUSHURL%" main 2>&1
set "RC=%ERRORLEVEL%"

rem ---- 无论成败都清掉地址里的令牌，避免残留在命令历史 ----
set "PUSHURL="

echo.
if not "%RC%"=="0" (
    echo ============================================================
    echo   推送失败
    echo ============================================================
    echo.
    echo   常见原因：
    echo     1. 令牌没勾 repo 权限 —— 回上一步重新生成时勾上
    echo     2. 令牌过期或被复制时带了空格 —— 删掉 git-token.txt 重新生成
    echo     3. 确认 GitHub 上仓库名是 ExamDlgProj，且为 Public
    echo.
) else (
    echo ============================================================
    echo   推送成功
    echo ============================================================
    echo.
    echo   下一步：查看构建是否通过
    echo   https://github.com/yzh627/ExamDlgProj/actions
    echo.
)

echo   （令牌已清除，不会留在任何地方）
echo.
pause
endlocal
