@echo off
rem Debug header generation
"%~dp0Build\Bin\Debug-windows-x86_64\UHT.exe" --root "%~dp0." --output "Build\Generated\UHT\Debug-windows-x86_64"

rem Exit code preservation and result inspection
set "UHTResult=%errorlevel%"
pause
exit /b %UHTResult%