@echo off
chcp 936 >nul 2>nul
setlocal
echo.
echo   正在刷新 Windows 图标缓存（几秒钟）...
ie4uinit.exe -show >nul 2>nul
echo   完成。
echo.
echo   如果文件图标还是旧的：把文件复制到别的文件夹再看一眼，或者注销/重启一次电脑。
echo   （这是因为 Windows 会缓存图标，程序本身没问题）
echo.
pause