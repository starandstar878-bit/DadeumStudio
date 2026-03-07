param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exePath = Join-Path $repoRoot "Builds/VisualStudio2026/$Platform/$Configuration/App/DadeumStudio.exe"
if (-not (Test-Path $exePath))
{
    throw "Executable not found: $exePath"
}

$outDir = Join-Path $repoRoot ("Builds/TeulExport/Phase5Smoke_" + [DateTimeOffset]::Now.ToUnixTimeMilliseconds())
Write-Host "[TeulPhase5Smoke] Exporting to: $outDir"
& $exePath "--teul-phase5-export-smoke" "--output-dir=$outDir"
if (($LASTEXITCODE -is [int]) -and $LASTEXITCODE -ne 0)
{
    throw "Teul Phase5 smoke mode failed (exit code: $LASTEXITCODE)"
}

$sourcePath = Join-Path $outDir "TeulPhase5SmokeRuntime.cpp"
for ($retry = 0; $retry -lt 100; $retry++)
{
    if ([System.IO.File]::Exists($sourcePath))
    {
        break
    }

    Start-Sleep -Milliseconds 200
}
if (-not [System.IO.File]::Exists($sourcePath))
{
    throw "Missing smoke output file: $sourcePath"
}

$sourceText = Get-Content $sourcePath -Raw
foreach ($token in @("createParameterLayout()", "embeddedGraphJson()", "embeddedRuntimeJson()", "setParamById(", "nodeSchedule() const noexcept"))
{
    if (-not $sourceText.Contains($token))
    {
        throw "Generated source missing token: $token"
    }
}

$vsDevCmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $vsDevCmd))
{
    throw "VsDevCmd not found: $vsDevCmd"
}

$compileCommand = "`"$vsDevCmd`" -host_arch=x64 -arch=x64 && cl /nologo /c /std:c++17 /EHsc /MDd /I `"$repoRoot\JuceLibraryCode`" /I `"C:\JUCE\modules`" /I `"$repoRoot\Source`" /D JUCE_PROJUCER_VERSION=0x8000c /D JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 /D JUCE_STANDALONE_APPLICATION=1 `"$sourcePath`""
Write-Host "[TeulPhase5Smoke] Compiling generated runtime TU..."
cmd /c $compileCommand
if ($LASTEXITCODE -ne 0)
{
    throw "Generated runtime compile check failed (exit code: $LASTEXITCODE)"
}

Write-Host "[TeulPhase5Smoke] PASS"
Write-Host "[TeulPhase5Smoke] Output: $outDir"