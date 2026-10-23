@echo off
"%~dp0TCSAssetManager.exe" --game "%~dp0." --manage
if errorlevel 1 pause
