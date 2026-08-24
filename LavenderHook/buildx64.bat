@echo off
set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set BUILD=%ROOT%\build64

echo === Configuring x64 ===
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

echo === Building x64 Release ===
cmake --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1

echo === Build succeeded: %BUILD%\Release\LavenderHook.dll ===
