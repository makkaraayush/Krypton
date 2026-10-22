@echo off
setlocal
echo ========================================================
echo             KRYPTON - ONE-CLICK GITHUB SYNC             
echo ========================================================
echo.

git status
echo.
set /p msg="Enter commit message (press Enter for 'update'): "
if "%msg%"=="" set msg=update

echo.
echo Staging changes...
git add -A

echo Committing...
git commit -m "%msg%"

echo Pushing to origin main...
git push origin main

echo.
echo ========================================================
echo                  SYNC COMPLETE!                         
echo ========================================================
pause
