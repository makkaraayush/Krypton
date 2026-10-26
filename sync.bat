@echo off
setlocal
echo ========================================================
echo             KRYPTON - GITHUB SYNC UTILITY               
echo ========================================================
echo.

git status -s
echo.
set /p msg="Commit message (press Enter for 'quick sync'): "
if "%msg%"=="" set msg=quick sync

echo.
echo [1/3] Staging changes...
git add -A

echo [2/3] Creating commit...
git commit -m "%msg%"

echo [3/3] Pushing to GitHub (origin main)...
git push origin main

echo.
echo ========================================================
echo                  SYNC FINISHED!                         
echo ========================================================
pause
