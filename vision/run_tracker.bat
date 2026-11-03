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

echo Checking dependencies (OpenCV, MediaPipe, NumPy, PySerial)...
python -c "import cv2, mediapipe, serial, numpy; assert hasattr(mediapipe, 'solutions')" >nul 2>&1
if %errorlevel% neq 0 (
    echo Installing or verifying required packages from requirements.txt...
    python -m pip install -r "%~dp0requirements.txt"
)

echo.
echo Launching Vision Teleoperation (Auto-Detect: USB Serial + Wi-Fi UDP)...
echo.
echo Controls:
echo   [Q] Quit
echo   [M] Cycle connection mode (Auto / Serial / UDP / Offline)
echo   [W] Toggle wrist roll tracking
echo   [C] Tare calibration (rest position)
echo   [R] Reset calibration
echo   [H] Help modal
echo.

python "%~dp0hand_tracker.py" --mirror --mode auto --ip 192.168.4.1

pause
