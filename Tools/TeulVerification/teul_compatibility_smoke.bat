@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
pushd "%REPO_ROOT%" >nul

set "APP=Builds\VisualStudio2026\x64\Debug\App\DadeumStudio.exe"
if not exist "%APP%" set "APP=Builds\VisualStudio2022\x64\Debug\App\DadeumStudio.exe"

if not exist "%APP%" (
  echo DadeumStudio debug app not found. Run build_check.bat first.
  popd >nul
  endlocal
  exit /b 1
)

"%APP%" --teul-phase8-compatibility-smoke %*
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul
endlocal & exit /b %EXIT_CODE%
