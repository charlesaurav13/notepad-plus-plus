@echo off
:: One-click launcher — double-click this file on Windows to build and run Notepad++ AI Edition
:: Downloads all prerequisites automatically (Git, MSYS2, MinGW-w64)

echo.
echo  ==========================================
echo   Notepad++ AI Edition - Auto Build Script
echo  ==========================================
echo.

:: Check if PowerShell is available
where powershell >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: PowerShell not found. Please install it from:
    echo https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows
    pause
    exit /b 1
)

:: Run the PowerShell script with elevated execution policy
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-and-run.ps1"
