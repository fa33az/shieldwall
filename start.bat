:: ShieldWall DDoS Protection Suite
:: Author: fa33az

@echo off
title ShieldWall - All-In-One Launcher
echo ===================================================
echo   ShieldWall DDoS Protection Suite Launcher
echo ===================================================
echo.

:: Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python tidak terdeteksi! Pastikan Python sudah terinstall dan ditambahkan ke PATH.
    pause
    exit /b
)

:: Start Python Proxy in a minimized window
echo [ShieldWall] Menyalakan Python Query Proxy di port 7777...
start "ShieldWall Proxy" /min python proxy/query_proxy.py

:: Start SA-MP server
echo [ShieldWall] Menyalakan SA-MP Server di port 7778...
if exist samp-server.exe (
    samp-server.exe
) else (
    echo.
    echo [Peringatan] samp-server.exe tidak ditemukan di folder ini.
    echo Pastikan meletakkan file start.bat ini di folder utama SA-MP Server Anda.
    echo.
)

:: Auto clean up proxy when server stops
echo [ShieldWall] SA-MP Server dimatikan. Mematikan Proxy...
taskkill /FI "WINDOWTITLE eq ShieldWall Proxy*" /T /F >nul 2>&1
echo [ShieldWall] Selesai.
pause
