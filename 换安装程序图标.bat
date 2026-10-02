@echo off
chcp 936 >nul 2>nul
setlocal
cd /d "%~dp0"
if "%~1"=="" (
  echo.
  echo   用法：把一张图片（png / jpg / bmp 都可以）拖到本文件上，松手即可。
  echo   作用：只换安装程序（Setup）的图标
  echo.
  echo   建议用正方形图片、边长 256 像素以上。原图标会自动备份。
  echo.
  pause
  exit /b
)
echo.
echo   正在把 "%~1" 转成图标...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\png2ico.ps1" "%~1" -Target installer
if errorlevel 1 (
  echo   转换失败，请确认图片能正常打开。
  pause
  exit /b 1
)
echo.
echo   图标换好了，需要重新编译才会生效。
set /p ans=   现在自动重新编译吗？(Y/N) 
if /i "%ans%"=="Y" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\rebuild.ps1"
  echo.
  echo   完成！发布包里的文件也已经同步好了。
  pause
) else (
  echo.
  echo   好的。以后双击"一键重建.bat"再编译即可。
  pause
)