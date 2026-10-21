@echo off
title Krypton Bionic Arm - Vision Teleoperation
echo ========================================================
echo        KRYPTON BIONIC ARM - REAL-TIME HAND TRACKER       
echo ========================================================
echo.

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not installed or not in PATH!
    echo Please install Python 3.10+ from python.org.
    pause
    exit /b 1
)

echo Checking dependencies...
python -c "import cv2, mediapipe, serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo Installing required packages from requirements.txt...
    pip install -r "%~dp0requirements.txt"
)

echo.
echo Launching Vision Teleoperation (Default: UDP to 192.168.4.1:8888)...
echo Press 'q' in the camera window to quit.
echo Press 'm' to cycle modes (UDP / Serial / Offline).
echo.
python "%~dp0hand_tracker.py" --mirror --mode udp --ip 192.168.4.1

pause
