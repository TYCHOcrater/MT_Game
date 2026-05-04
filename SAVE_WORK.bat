@echo off
setlocal enabledelayedexpansion
title MT_Game - Save Work
color 0A
echo ============================================
echo    Save and Upload Your Work
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
    echo Make sure you're running SAVE_WORK.bat from inside the MT_Game project folder
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
echo Saving while Unreal is open can corrupt files.
echo.
pause

echo.
echo ============================================
echo    Checking what you changed...
echo ============================================
git status --short
echo.

REM Check if there's anything to commit
git diff --quiet
set UNCACHED=%errorlevel%
git diff --quiet --cached
set CACHED=%errorlevel%

REM Check for untracked files
for /f %%i in ('git ls-files --others --exclude-standard ^| find /c /v ""') do set UNTRACKED_COUNT=%%i

if %UNCACHED% equ 0 if %CACHED% equ 0 if %UNTRACKED_COUNT% equ 0 (
    echo You haven't changed anything since the last save.
    echo Nothing to upload!
    pause
    exit /b 0
)

set /p CONFIRM="Does this look right? (Y to continue, N to cancel): "
if /i not "%CONFIRM%"=="Y" (
    echo Cancelled.
    pause
    exit /b 0
)

echo.
set /p MESSAGE="Briefly describe what you did (e.g., 'Added new tree models'): "
if "%MESSAGE%"=="" set MESSAGE=Updated assets

echo.
echo ============================================
echo    [1/4] Staging your changes...
echo ============================================
git add .
if %errorlevel% neq 0 (
    echo Failed to stage changes. Take a screenshot and send to your teammate.
    pause
    exit /b 1
)
echo Done.
echo.

echo ============================================
echo    [2/4] Committing your changes...
echo ============================================
git commit -m "%MESSAGE%"
if %errorlevel% neq 0 (
    echo Failed to commit. Take a screenshot and send to your teammate.
    pause
    exit /b 1
)
echo.

echo ============================================
echo    [3/4] Checking for teammate's updates...
echo ============================================
echo Pulling any changes your teammate pushed while you were working...
git pull --rebase
if %errorlevel% neq 0 (
    echo.
    echo ============================================
    echo    MERGE CONFLICT
    echo ============================================
    echo Your teammate changed some of the same files you did.
    echo This needs manual resolution.
    echo.
    echo To undo and try again later:
    echo    git rebase --abort
    echo.
    echo Take a screenshot and ask your teammate for help.
    pause
    exit /b 1
)
echo.

echo ============================================
echo    [4/4] Uploading to GitHub...
echo ============================================
echo This may take a while if you changed large files.
echo.
git push
if %errorlevel% neq 0 (
    echo.
    echo ============================================
    echo    UPLOAD FAILED
    echo ============================================
    echo Possible reasons:
    echo   - Internet connection dropped
    echo   - GitHub authentication expired ^(try again, sign in if prompted^)
    echo   - LFS storage/bandwidth limit hit ^(ask your teammate^)
    echo.
    echo Take a screenshot of the error above and send to your teammate.
    pause
    exit /b 1
)

echo.
echo ============================================
echo    SUCCESS! Your work is saved to GitHub.
echo ============================================
echo.
echo Don't forget to run UNLOCK_FILES.bat to release any files you locked,
echo so your teammate can edit them later.
echo.
pause