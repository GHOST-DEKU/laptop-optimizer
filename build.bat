@echo off
echo ========================================================
echo Building LaptopOptimizer.exe from source (GCC/MinGW-w64)
echo ========================================================

g++ -O2 -mwindows src\main.cpp -lgdiplus -lgdi32 -luser32 -ldwmapi -lcomctl32 -static -o LaptopOptimizer.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] LaptopOptimizer.exe compiled successfully!
    echo Output: %~dp0LaptopOptimizer.exe
) else (
    echo.
    echo [ERROR] Compilation failed. Ensure g++ (MinGW-w64) is in your PATH.
)
pause
