@echo off
setlocal enabledelayedexpansion
title MT_Game - Start Work Session
color 0B
echo ============================================
echo    Start Work Session - Pulling Latest
echo ============================================
echo.

REM ====== CONFIG ======
set EXPECTED_REPO=TYCHOcrater/MT_Game
REM ====================

REM Verify we're inside a Git repo
git rev-parse --is-inside-work-tree >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This folder is not a Git repository.
    echo.
    echo Make sure you're running START_WORK.bat from inside the MT_Game project folder
    echo ^(the one that contains the .uproject file^).
    pause
    exit /b 1
)

REM Verify the remote points to our repo
for /f "tokens=*" %%i in ('git config --get remote.origin.url 2^>nul') do set REMOTE_URL=%%i
echo Remote: %REMOTE_URL%
echo %REMOTE_URL% | findstr /i "%EXPECTED_REPO%" >nul
if %errorlevel% neq 0 (
    echo.
    echo WARNING: This repo's remote doesn't match the expected MT_Game repository.
    echo Expected to find "%EXPECTED_REPO%" in the URL.
    echo.
    echo If this is a different/test repo, that's fine. Otherwise, stop and ask your teammate.
    echo.
    set /p CONTINUE="Continue anyway? (Y/N): "
    if /i not "!CONTINUE!"=="Y" (
        echo Cancelled.
        pause
        exit /b 0
    )
)
echo.

echo IMPORTANT: Make sure Unreal Engine is CLOSED before continuing!
echo Pulling updates while Unreal is open can corrupt files.
echo.
pause

echo.
echo ============================================
echo    Checking for unsaved local changes...
echo ============================================
git diff --quiet
set UNCACHED=%errorlevel%
git diff --quiet --cached
set CACHED=%errorlevel%
for /f %%i in ('git ls-files --others --exclude-standard ^| find /c /v ""') do set UNTRACKED_COUNT=%%i

if %UNCACHED% neq 0 goto :has_changes
if %CACHED% neq 0 goto :has_changes
if %UNTRACKED_COUNT% gtr 0 goto :has_changes
goto :no_changes

:has_changes
echo.
echo You have unsaved changes from a previous session:
echo.
git status --short
echo.
echo You should run SAVE_WORK.bat first to upload these changes
echo before pulling new updates from your teammate.
echo.
echo If you continue now, Git might refuse to pull or create conflicts.
echo.
set /p CONTINUE="Continue anyway? (Y/N): "
if /i not "!CONTINUE!"=="Y" (
    echo Cancelled. Run SAVE_WORK.bat to save your work first.
    pause
    exit /b 0
)

:no_changes
echo.
echo ============================================
echo    Pulling latest changes from GitHub...
echo ============================================
git pull
if %errorlevel% neq 0 (
    echo.
    echo ============================================
    echo    PULL FAILED
    echo ============================================
    echo Possible reasons:
    echo   - Local changes conflict with teammate's changes
    echo   - Internet connection dropped
    echo   - GitHub authentication expired
    echo.
    echo DO NOT open Unreal Engine until this is fixed!
    echo Take a screenshot of the error above and ask your teammate.
    pause
    exit /b 1
)

echo.
echo ============================================
echo    READY TO WORK!
echo ============================================
echo.
echo You have the latest version of the project.
echo You can now open the .uproject file in Unreal Engine.
echo.
echo REMINDER: Before you edit any .uasset or .umap file,
echo run LOCK_FILES.bat to prevent conflicts!
echo.
pause