@echo off
setlocal
set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe
"%MSBUILD%" "Builds\VisualStudio2022\DadeumStudio.sln" /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal /flp:logfile=build_log.txt;verbosity=minimal
echo EXIT_CODE=%ERRORLEVEL% >> build_log.txt
endlocal
