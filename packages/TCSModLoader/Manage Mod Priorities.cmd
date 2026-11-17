@echo off
setlocal
if not exist "%~dp0TCSAssetManager.exe" (
    echo TCSAssetManager.exe is missing. Extract the complete loader ZIP into the game folder.
    pause
    exit /b 1
)
"%~dp0TCSAssetManager.exe" --game "%~dp0." --manage
if errorlevel 1 pause
