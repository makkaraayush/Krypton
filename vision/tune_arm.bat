@echo off
title Krypton - Kinematics Tuner & Calibration Hub
echo ========================================================
echo       KRYPTON BIONIC ARM - KINEMATIC POSITION TUNER     
echo ========================================================
echo.

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not installed or not in PATH!
    pause
    exit /b 1
)

python "%~dp0tuner.py"

pause

