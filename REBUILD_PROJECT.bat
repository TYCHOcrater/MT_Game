@echo off
setlocal enabledelayedexpansion
title MT_Game - Rebuild Project
color 0D
echo ============================================
echo    Rebuild Project
echo ============================================
echo.
echo Run this if:
echo   - Unreal won't open and shows "missing modules"
echo   - Unreal crashes immediately when launching
echo   - You just pulled a big update with code changes
echo.
echo This forces a fresh recompile of the project.
echo It usually takes 5-15 minutes.
echo.

REM ====== CONFIG ======
set EXPECTED_REPO=TYCHOcrater/MT_Game
set UPROJECT_NAME=MultiplayerTest.uproject
REM ====================

REM Verify we're inside a Git repo
git rev-parse --is-inside-work-tree >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This folder is not a Git repository.
    echo Make sure you're running this from inside the MT_Game project folder.
    pause
    exit /b 1
)

REM Verify the .uproject file exists
if not exist "%UPROJECT_NAME%" (
    echo ERROR: Could not find %UPROJECT_NAME% in this folder.
    echo Make sure REBUILD_PROJECT.bat is in the same folder as the .uproject file.
    pause
    exit /b 1
)

REM Verify the remote points to our repo
for /f "tokens=*" %%i in ('git config --get remote.origin.url 2^>nul') do set REMOTE_URL=%%i
echo Remote: %REMOTE_URL%
echo %REMOTE_URL% | findstr /i "%EXPECTED_REPO%" >nul
if %errorlevel% neq 0 (
    echo.
    echo WARNING: Remote doesn't match the expected repo. Continuing anyway...
)
echo.

echo IMPORTANT: Make sure Unreal Engine is CLOSED before continuing!
echo If Unreal is open, this rebuild will fail.
echo.
pause

echo.
echo ============================================
echo    [1/3] Finding Unreal Engine...
echo ============================================
set UE_PATH=
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.7" /v "InstalledDirectory" 2^>nul') do set UE_PATH=%%b
if "%UE_PATH%"=="" (
    for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.7" /v "InstalledDirectory" 2^>nul') do set UE_PATH=%%b
)

if "%UE_PATH%"=="" (
    echo ERROR: Could not find Unreal Engine 5.7 install location.
    echo.
    echo Make sure Unreal Engine 5.7 is installed via Epic Games Launcher.
    echo If it's installed in a non-standard location, you may need to rebuild manually:
    echo   1. Right-click %UPROJECT_NAME%
    echo   2. Click "Generate Visual Studio project files"
    echo   3. Open the .sln file in Visual Studio
    echo   4. Build the project ^(Ctrl+Shift+B^)
    pause
    exit /b 1
)

echo Found Unreal Engine at: %UE_PATH%
echo.

echo ============================================
echo    [2/3] Cleaning old build files...
echo ============================================
echo Deleting Binaries, Intermediate, and DerivedDataCache to force a fresh build...
echo.

if exist "Binaries" (
    echo Removing Binaries folder...
    rmdir /s /q "Binaries"
)
if exist "Intermediate" (
    echo Removing Intermediate folder...
    rmdir /s /q "Intermediate"
)
if exist "DerivedDataCache" (
    echo Removing DerivedDataCache folder...
    rmdir /s /q "DerivedDataCache"
)

REM Also clean plugin binaries if any
if exist "Plugins" (
    for /d %%P in (Plugins\*) do (
        if exist "%%P\Binaries" (
            echo Removing %%P\Binaries...
            rmdir /s /q "%%P\Binaries"
        )
        if exist "%%P\Intermediate" (
            echo Removing %%P\Intermediate...
            rmdir /s /q "%%P\Intermediate"
        )
    )
)

echo Done cleaning.
echo.

echo ============================================
echo    [3/3] Rebuilding the project...
echo ============================================
echo.
echo Generating project files...
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%CD%\%UPROJECT_NAME%" -game -rocket -progress
if %errorlevel% neq 0 (
    echo.
    echo Failed to generate project files. Take a screenshot and send to your teammate.
    pause
    exit /b 1
)

echo.
echo Compiling C++ code ^(this is the slow part, 5-15 minutes^)...
echo.
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" MultiplayerTestEditor Win64 Development -project="%CD%\%UPROJECT_NAME%" -waitmutex
if %errorlevel% neq 0 (
    echo.
    echo ============================================
    echo    REBUILD FAILED
    echo ============================================
    echo The project failed to compile. This usually means:
    echo   - Visual Studio isn't installed correctly
    echo   - The C++ code has errors ^(your teammate may have pushed broken code^)
    echo   - Unreal Engine version mismatch
    echo.
    echo Take a screenshot of the error above and send to your teammate.
    pause
    exit /b 1
)

echo.
echo ============================================
echo    REBUILD SUCCESSFUL!
echo ============================================
echo.
echo You can now open %UPROJECT_NAME% in Unreal Engine.
echo.
pause