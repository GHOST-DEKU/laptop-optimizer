@echo off
:: ============================================================================
:: Laptop Optimizer // Cybercore Techwear Standalone Launcher
:: Automatically deflects Windows SmartScreen "Don't Run" & triggers UAC
:: ============================================================================
cd /d "%~dp0"

:: Strip NTFS Zone.Identifier (Mark of the Web) to prevent SmartScreen popups
powershell -NoProfile -ExecutionPolicy Bypass -Command "if (Test-Path 'LaptopOptimizer.exe') { Unblock-File -Path 'LaptopOptimizer.exe' -ErrorAction SilentlyContinue }"

:: Launch native executable with administrator privileges
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~dp0LaptopOptimizer.exe' -Verb RunAs"
if %ERRORLEVEL% NEQ 0 (
    start "" "%~dp0LaptopOptimizer.exe"
)
exit
