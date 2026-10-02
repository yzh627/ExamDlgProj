@echo off
chcp 936 >nul 2>nul
setlocal
cd /d "%~dp0"
echo.
echo   正在重新编译：64位主程序 + 32位主程序 + 安装程序（约 1 分钟）
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\rebuild.ps1"
echo.
pause