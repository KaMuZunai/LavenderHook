@echo off
set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set BUILD=%ROOT%\build86

echo === Configuring x86 ===
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1

echo === Building x86 Release ===
cmake --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1

echo === Build succeeded: %BUILD%\Release\LavenderHook.dll ===
