@echo off
REM Native Windows build using MinGW-w64 (e.g. from MSYS2: pacman -S mingw-w64-x86_64-gcc)
setlocal
set ROOT=%~dp0..
if not exist "%ROOT%\build-win" mkdir "%ROOT%\build-win"
gcc -std=c99 -O2 -Wall -Wextra -static -static-libgcc ^
    "%ROOT%\src\htserve.c" -o "%ROOT%\build-win\htserve.exe" -lws2_32
if errorlevel 1 exit /b 1
echo built: %ROOT%\build-win\htserve.exe
