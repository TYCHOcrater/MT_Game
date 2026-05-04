@echo off
title MT_Game - Lock Files
color 0E
echo ============================================
echo    Lock Files Before Editing
echo ============================================
echo.
echo This locks files so your teammate can't edit them at the same time.
echo Always lock files BEFORE you start editing them in Unreal!
echo.
echo You can drag and drop files onto this script to lock them,
echo or type the path manually below.
echo.

REM If files were dragged onto the script, lock them all
if not "%~1"=="" goto :lock_dragged

REM Otherwise ask for input
:manual_input
echo Type the path to the file you want to lock.
echo Example: Content/Characters/Hero/SK_Hero.uasset
echo.
echo Or type "list" to see what's currently locked.
echo Or type "quit" to exit.
echo.
set /p FILEPATH="File path: "

if /i "%FILEPATH%"=="quit" exit /b 0
if /i "%FILEPATH%"=="list" (
    echo.
    echo === Currently locked files ===
    git lfs locks
    echo.
    goto :manual_input
)
if "%FILEPATH%"=="" goto :manual_input

git lfs lock "%FILEPATH%"
echo.
echo Lock another file? Type the path, or "quit" to exit.
goto :manual_input

:lock_dragged
echo Locking dragged files...
echo.
:loop
if "%~1"=="" goto :done
REM Convert absolute path to relative path from repo root
set "FULLPATH=%~1"
set "RELPATH=%FULLPATH:*\MT_Game\=%"
git lfs lock "%RELPATH%"
shift
goto :loop

:done
echo.
echo ============================================
echo    Done locking files
echo ============================================
echo.
echo You can now safely edit these files in Unreal.
echo Remember to run SAVE_WORK.bat when you're done!
pause