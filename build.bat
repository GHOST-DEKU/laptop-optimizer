@echo off
echo ========================================================
echo Building LaptopOptimizer.exe from source (GCC/MinGW-w64)
echo Theme: Concept 2 - Cybercore // Dark Techwear
echo ========================================================

echo Compiling embedded Windows resource and icons...
windres -i src\resource.rc -o src\resource.res -O coff

echo Compiling native Win32/GDI+ desktop executable...
g++ -O2 -mwindows -static -Isrc src\main.cpp src\resource.res -o LaptopOptimizer.exe -lgdiplus -lgdi32 -luser32 -ldwmapi -lcomctl32 -lole32 -lshlwapi

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] LaptopOptimizer.exe compiled successfully!
    echo Standalone binary: %~dp0LaptopOptimizer.exe
) else (
    echo.
    echo [ERROR] Compilation failed. Ensure g++ and windres (MinGW-w64) are in your PATH.
)
pause
