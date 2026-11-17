@echo off
setlocal
pushd "%~dp0"

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup\SetupDebugger.ps1" %*
set "Result=%errorlevel%"

popd
if %Result% neq 0 echo Debugger setup failed. See the error above.
if not defined CI pause
exit /b %Result%
