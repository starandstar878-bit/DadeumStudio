param(
  [string]$Path,
  [string]$ExePath,
  [ValidateSet("Debug", "Release", "Verify")][string]$Configuration = "Debug",
  [ValidateSet("x64", "Win32")][string]$Platform = "x64",
  [int]$StartupTimeoutSeconds = 20,
  [int]$RenderDelayMilliseconds = 500
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-Timestamp {
  Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
}

function Get-RepoRoot {
  Split-Path -Parent $PSScriptRoot
}

function Resolve-OutputPath {
  param(
    [string]$RequestedPath,
    [string]$RepoRoot
  )

  if ([string]::IsNullOrWhiteSpace($RequestedPath)) {
    $outputDirectory = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot "Builds\DebugScreenshots"))
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    return Join-Path $outputDirectory ("DadeumStudio-DebugShot-" + (Get-Timestamp) + ".png")
  }

  $expanded = [Environment]::ExpandEnvironmentVariables($RequestedPath)
  $fullPath = [System.IO.Path]::GetFullPath($expanded)
  $extension = [System.IO.Path]::GetExtension($fullPath)

  if ([string]::IsNullOrWhiteSpace($extension)) {
    $fullPath = "$fullPath.png"
  }

  $parent = Split-Path -Parent $fullPath
  if (-not [string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
  }

  return $fullPath
}

function Resolve-ExePath {
  param(
    [string]$RequestedPath,
    [string]$RepoRoot,
    [string]$PlatformName,
    [string]$ConfigurationName
  )

  if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
    $fullPath = [System.IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($RequestedPath))
    if (-not (Test-Path -LiteralPath $fullPath)) {
      throw "Requested executable not found: $fullPath"
    }
    return $fullPath
  }

  $vs2026Path = "Builds\VisualStudio2026\{0}\{1}\App\DadeumStudio.exe" -f $PlatformName, $ConfigurationName
  $vs2022Path = "Builds\VisualStudio2022\{0}\{1}\App\DadeumStudio.exe" -f $PlatformName, $ConfigurationName
  $verifyPath = "Builds\VisualStudio2026\Builds\VisualStudio2026\{0}\{1}\Bin\DadeumStudio.exe" -f $PlatformName, $ConfigurationName

  $candidates = @(
    [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $vs2026Path)),
    [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $vs2022Path)),
    [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $verifyPath))
  )

  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) {
      return $candidate
    }
  }

  throw ("Debug executable not found. Expected one of:`n" + ($candidates -join "`n"))
}

function Test-ProcessMatchesExePath {
  param(
    [System.Diagnostics.Process]$Process,
    [string]$ResolvedExePath
  )

  try {
    if (-not [string]::IsNullOrWhiteSpace($Process.Path)) {
      return [string]::Equals($Process.Path, $ResolvedExePath, [System.StringComparison]::OrdinalIgnoreCase)
    }
  } catch {
  }

  return $true
}

function Get-MatchingProcesses {
  param(
    [string]$ProcessName,
    [string]$ResolvedExePath
  )

  return @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
      Where-Object { Test-ProcessMatchesExePath -Process $_ -ResolvedExePath $ResolvedExePath } |
      Sort-Object StartTime -Descending)
}

function Wait-ForVisibleProcess {
  param(
    [string]$ProcessName,
    [string]$ResolvedExePath,
    [int]$TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    $matches = Get-MatchingProcesses -ProcessName $ProcessName -ResolvedExePath $ResolvedExePath
    foreach ($process in $matches) {
      try {
        $process.Refresh()
      } catch {
      }

      if (-not $process.HasExited -and $process.MainWindowHandle -ne 0) {
        return $process
      }
    }

    Start-Sleep -Milliseconds 250
  }

  throw "No visible debug window found for '$ProcessName'. Launch the app first and keep it visible."
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeMethods {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
  }

  [DllImport("user32.dll")]
  public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

  [DllImport("user32.dll")]
  public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

function Prepare-WindowForScreenCapture {
  param([IntPtr]$WindowHandle)

  [NativeMethods]::ShowWindowAsync($WindowHandle, 9) | Out-Null
  [NativeMethods]::SetForegroundWindow($WindowHandle) | Out-Null
  Start-Sleep -Milliseconds 250
}

function Capture-WindowBitmap {
  param([IntPtr]$WindowHandle)

  if ($WindowHandle -eq [IntPtr]::Zero) {
    throw "Window handle is invalid."
  }

  Prepare-WindowForScreenCapture -WindowHandle $WindowHandle

  $rect = New-Object NativeMethods+RECT
  if (-not [NativeMethods]::GetWindowRect($WindowHandle, [ref]$rect)) {
    throw "Failed to get window bounds."
  }

  $width = $rect.Right - $rect.Left
  $height = $rect.Bottom - $rect.Top
  if ($width -le 0 -or $height -le 0) {
    throw "Window bounds are invalid."
  }

  $bitmap = New-Object System.Drawing.Bitmap($width, $height)
  try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
      $source = New-Object System.Drawing.Point($rect.Left, $rect.Top)
      $target = [System.Drawing.Point]::Empty
      $size = New-Object System.Drawing.Size($width, $height)
      $graphics.CopyFromScreen($source, $target, $size)
    } finally {
      $graphics.Dispose()
    }

    return $bitmap
  } catch {
    $bitmap.Dispose()
    throw
  }
}

$repoRoot = Get-RepoRoot
$resolvedExePath = Resolve-ExePath -RequestedPath $ExePath -RepoRoot $repoRoot -PlatformName $Platform -ConfigurationName $Configuration
$resolvedOutputPath = Resolve-OutputPath -RequestedPath $Path -RepoRoot $repoRoot
$processName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedExePath)
$targetProcess = Wait-ForVisibleProcess -ProcessName $processName -ResolvedExePath $resolvedExePath -TimeoutSeconds $StartupTimeoutSeconds

try {
  try {
    $targetProcess.Refresh()
  } catch {
  }

  if ($targetProcess.MainWindowHandle -eq 0) {
    $targetProcess = Wait-ForVisibleProcess -ProcessName $processName -ResolvedExePath $resolvedExePath -TimeoutSeconds $StartupTimeoutSeconds
  }

  if ($RenderDelayMilliseconds -gt 0) {
    Start-Sleep -Milliseconds $RenderDelayMilliseconds
  }

  $windowHandle = [IntPtr]$targetProcess.MainWindowHandle
  $bitmap = Capture-WindowBitmap -WindowHandle $windowHandle
  try {
    $bitmap.Save($resolvedOutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally {
    $bitmap.Dispose()
  }

  Write-Output "EXE=$resolvedExePath"
  Write-Output "SOURCE=existing"
  Write-Output "PID=$($targetProcess.Id)"
  Write-Output "HANDLE=$([Int64]$windowHandle)"
  Write-Output "SCREENSHOT=$resolvedOutputPath"
} finally {
}