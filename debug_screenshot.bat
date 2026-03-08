@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0Tools\CaptureDebugWindow.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"

endlocal & exit /b %EXIT_CODE%
