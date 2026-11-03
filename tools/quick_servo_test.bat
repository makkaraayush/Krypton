@echo off
title Krypton - Quick Servo Test Utility
echo ========================================================
echo          KRYPTON BIONIC ARM - SERVO BENCH TEST          
echo ========================================================
echo.

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not installed or not in PATH!
    pause
    exit /b 1
)

python "%~dp0servo_test.py" --mode auto

pause
