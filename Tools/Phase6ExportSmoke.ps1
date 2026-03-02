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

$outDir = Join-Path $repoRoot ("Builds/GyeolExport/Phase6Smoke_" + [DateTimeOffset]::Now.ToUnixTimeMilliseconds())
Write-Host "[Phase6Smoke] Exporting to: $outDir"
& $exePath "--phase6-export-smoke" "--output-dir=$outDir"
if (($LASTEXITCODE -is [int]) -and $LASTEXITCODE -ne 0)
{
    throw "Smoke export mode failed (exit code: $LASTEXITCODE)"
}

$headerPath = Join-Path $outDir "GyeolPhase6SmokeComponent.h"
$sourcePath = Join-Path $outDir "GyeolPhase6SmokeComponent.cpp"
$manifestPath = Join-Path $outDir "export-manifest.json"
$runtimePath = Join-Path $outDir "export-runtime.json"
for ($retry = 0; $retry -lt 20; $retry++)
{
    if ((Test-Path $headerPath) -and (Test-Path $sourcePath) -and (Test-Path $manifestPath) -and (Test-Path $runtimePath))
    {
        break
    }

    Start-Sleep -Milliseconds 100
}

foreach ($path in @($headerPath, $sourcePath, $manifestPath, $runtimePath))
{
    if (-not (Test-Path $path))
    {
        throw "Missing smoke output file: $path"
    }
}

$sourceText = Get-Content $sourcePath -Raw
foreach ($token in @("initializeRuntimeBridge()", "dispatchRuntimeEvent(", "applyPropertyBindings()", "applyRuntimeAction("))
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
Write-Host "[Phase6Smoke] Compiling generated component TU..."
cmd /c $compileCommand
if ($LASTEXITCODE -ne 0)
{
    throw "Generated component compile check failed (exit code: $LASTEXITCODE)"
}

Write-Host "[Phase6Smoke] PASS"
Write-Host "[Phase6Smoke] Output: $outDir"
