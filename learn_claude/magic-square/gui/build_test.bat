@echo off
REM Build and run the cube unit tests on Windows.
REM Works in cmd.exe / PowerShell as long as gcc is on PATH
REM (MSYS2 MinGW, TDM-GCC, or any other Windows gcc distribution).
REM
REM No raylib needed -- the test only exercises the cube data model.

setlocal

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc not found on PATH.
    echo Install MSYS2 / MinGW-w64 / TDM-GCC, or open an MSYS2 MinGW shell.
    exit /b 1
)

echo Compiling cube_test.exe ...
gcc -Wall -Wextra -O2 -std=c99 cube.c cube_test.c -o cube_test.exe
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
echo Running cube_test.exe ...
echo --------------------------------
cube_test.exe
set RC=%ERRORLEVEL%
echo --------------------------------
if %RC% EQU 0 (
    echo All tests passed.
) else (
    echo %RC% test^(s^) failed.
)
exit /b %RC%
