@echo off
setlocal

set "MSBUILD="
set "MSBUILD_VS18=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
set "MSBUILD_VS2022=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"

if exist "%MSBUILD_VS18%" set "MSBUILD=%MSBUILD_VS18%"
if not defined MSBUILD if exist "%MSBUILD_VS2022%" set "MSBUILD=%MSBUILD_VS2022%"

if not defined MSBUILD (
  echo MSBuild not found.
  echo EXIT_CODE=1 >> build_log.txt
  endlocal
  exit /b 1
)

set "SOLUTION=Builds\VisualStudio2026\DadeumStudio.sln"
if not exist "%SOLUTION%" set "SOLUTION=Builds\VisualStudio2022\DadeumStudio.sln"

"%MSBUILD%" "%SOLUTION%" /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal /flp:logfile=build_log.txt;verbosity=minimal
echo EXIT_CODE=%ERRORLEVEL% >> build_log.txt
endlocal