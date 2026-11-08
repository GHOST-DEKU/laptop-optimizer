@echo off
:: ============================================================================
:: Laptop Optimizer // Standalone Launcher
:: Removes the "downloaded from the internet" mark from the .exe, then starts it
:: as administrator (Windows will show a UAC prompt).
:: ============================================================================
cd /d "%~dp0"

:: Use the real Windows PowerShell by full path, not whatever "powershell" is first on PATH.
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
:: The path goes in through an environment variable so odd characters in the folder name cannot break the command.
set "LO_EXE=%~dp0LaptopOptimizer.exe"

if not exist "%LO_EXE%" (
    echo LaptopOptimizer.exe was not found next to this file.
    pause
    exit /b 1
)

"%PS%" -NoProfile -Command "Unblock-File -LiteralPath $env:LO_EXE -ErrorAction SilentlyContinue"

:: Start as administrator. If you decline the UAC prompt, the app still opens
:: and shows a banner saying admin-only tweaks will be skipped.
"%PS%" -NoProfile -Command "Start-Process -FilePath $env:LO_EXE -Verb RunAs"
if errorlevel 1 (
    start "" "%LO_EXE%"
)
exit /b 0
