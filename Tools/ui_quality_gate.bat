@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0ui_quality_gate.ps1" %*
exit /b %errorlevel%