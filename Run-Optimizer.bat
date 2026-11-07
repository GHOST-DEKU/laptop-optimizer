@echo off
:: ============================================================================
:: Laptop Optimizer // Cybercore Techwear Standalone Launcher
:: Automatically deflects Windows SmartScreen "Don't Run" Mark of the Web
:: ============================================================================
cd /d "%~dp0"

:: Strip NTFS Zone.Identifier (Mark of the Web) to prevent SmartScreen popups
powershell -NoProfile -ExecutionPolicy Bypass -Command "if (Test-Path 'LaptopOptimizer.exe') { Unblock-File -Path 'LaptopOptimizer.exe' -ErrorAction SilentlyContinue }"

:: Launch native executable
start "" "%~dp0LaptopOptimizer.exe"
exit
