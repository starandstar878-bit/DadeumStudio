@echo off
setlocal

set "APP=Builds\VisualStudio2026\x64\Debug\App\DadeumStudio.exe"
if not exist "%APP%" set "APP=Builds\VisualStudio2022\x64\Debug\App\DadeumStudio.exe"

if not exist "%APP%" (
  echo DadeumStudio debug app not found. Run build_check.bat first.
  endlocal
  exit /b 1
)

"%APP%" --teul-phase7-benchmark-gate %*
set "EXIT_CODE=%ERRORLEVEL%"
endlocal & exit /b %EXIT_CODE%
