@echo off
setlocal
cd /d "%~dp0"

echo Compiling the C++ waitlist server...
g++ -std=c++17 -Wall -Wextra -pedantic server.cpp -o waitlist-server.exe -lws2_32
if errorlevel 1 (
    echo Compilation failed. Check that g++ is installed and available in PATH.
    pause
    exit /b 1
)

start "Restaurant Waitlist Server" waitlist-server.exe
timeout /t 1 /nobreak >nul
start "" http://127.0.0.1:8080/
endlocal