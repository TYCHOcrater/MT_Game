@echo off
title MT_Game - Unlock Files
color 0E
echo ============================================
echo    Unlock Files After Editing
echo ============================================
echo.
echo This releases your locks so your teammate can edit those files.
echo Only unlock files AFTER you've saved and pushed your changes!
echo.

:menu
echo What do you want to do?
echo   1. Show files I have locked
echo   2. Unlock a specific file
echo   3. Unlock ALL my files
echo   4. Quit
echo.
set /p CHOICE="Enter 1, 2, 3, or 4: "

if "%CHOICE%"=="1" goto :show
if "%CHOICE%"=="2" goto :unlock_one
if "%CHOICE%"=="3" goto :unlock_all
if "%CHOICE%"=="4" exit /b 0
echo Invalid choice, try again.
echo.
goto :menu

:show
echo.
echo === Currently locked files ===
git lfs locks
echo.
goto :menu

:unlock_one
echo.
echo Currently locked files:
git lfs locks
echo.
set /p FILEPATH="Type the path of the file to unlock: "
if "%FILEPATH%"=="" goto :menu
git lfs unlock "%FILEPATH%"
echo.
goto :menu

:unlock_all
echo.
echo Are you sure you want to unlock ALL your locked files?
echo Make sure you've already saved and uploaded your work!
set /p CONFIRM="Type YES to confirm: "
if /i not "%CONFIRM%"=="YES" (
    echo Cancelled.
    echo.
    goto :menu
)
echo.
echo Unlocking all your files...
for /f "tokens=1" %%i in ('git lfs locks ^| findstr /v "^ID"') do (
    git lfs unlock --id=%%i
)
echo.
echo Done!
echo.
goto :menu