@echo off
setlocal
if not exist "%~dp0LEGOStarWarsSaga.exe" (
    echo LEGOStarWarsSaga.exe was not found. Put this script in the game folder.
    pause
    exit /b 1
)
"%~dp0TCSAssetManager.exe" --game "%~dp0."
if errorlevel 1 (
    echo Mod preparation failed. The game was not launched.
    pause
    exit /b 1
)
start "" /d "%~dp0." "%~dp0LEGOStarWarsSaga.exe"
