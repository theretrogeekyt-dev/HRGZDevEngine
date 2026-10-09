@echo off
rem HRGZDevEngine Studio Launcher for Windows

where node >nul 2>nul
if %errorlevel% neq 0 (
    echo ==========================================================
    echo  Error: Node.js is required to run HRGZDevEngine Studio.
    echo  Please install Node.js (https://nodejs.org) and try again.
    echo ==========================================================
    pause
    exit /b 1
)

echo Starting HRGZDevEngine Studio...
node "%~dp0studio\server.js" %*
