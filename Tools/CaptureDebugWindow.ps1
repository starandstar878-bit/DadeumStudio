param(
  [string]$Path,
  [string]$ExePath,
  [ValidateSet("Debug", "Release", "Verify")][string]$Configuration = "Debug",
  [ValidateSet("x64", "Win32")][string]$Platform = "x64",
  [int]$StartupTimeoutSeconds = 20,
  [int]$RenderDelayMilliseconds = 1200,
  [switch]$KeepOpen
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
    $outputDirectory = Join-Path $RepoRoot "Builds\\DebugScreenshots"
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
    $fullPath = [System.IO.Path]::GetFullPath(
        [Environment]::ExpandEnvironmentVariables($RequestedPath))
    if (-not (Test-Path -LiteralPath $fullPath)) {
      throw "Requested executable not found: $fullPath"
    }
    return $fullPath
  }

  $candidates = @(
    (Join-Path $RepoRoot "Builds\\VisualStudio2026\\$PlatformName\\$ConfigurationName\\App\\DadeumStudio.exe"),
    (Join-Path $RepoRoot "Builds\\VisualStudio2022\\$PlatformName\\$ConfigurationName\\App\\DadeumStudio.exe"),
    (Join-Path $RepoRoot "Builds\\VisualStudio2026\\Builds\\VisualStudio2026\\$PlatformName\\$ConfigurationName\\Bin\\DadeumStudio.exe")
  )

  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) {
      return $candidate
    }
  }

  throw ("Debug executable not found. Expected one of:`n" + ($candidates -join "`n"))
}

function Get-LatestVisibleProcess {
  param([string]$ProcessName)

  $matches = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
      Where-Object { $_.MainWindowHandle -ne 0 })
  if ($matches.Count -eq 0) {
    return $null
  }

  return $matches | Sort-Object StartTime -Descending | Select-Object -First 1
}

function Wait-ForMainWindowHandle {
  param(
    [System.Diagnostics.Process]$PrimaryProcess,
    [string]$ProcessName,
    [int]$TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    if ($PrimaryProcess -ne $null) {
      try {
        $PrimaryProcess.Refresh()
      } catch {
      }

      if (-not $PrimaryProcess.HasExited -and $PrimaryProcess.MainWindowHandle -ne 0) {
        return [IntPtr]$PrimaryProcess.MainWindowHandle
      }
    }

    $fallbackProcess = Get-LatestVisibleProcess -ProcessName $ProcessName
    if ($fallbackProcess -ne $null) {
      try {
        $fallbackProcess.Refresh()
      } catch {
      }

      if ($fallbackProcess.MainWindowHandle -ne 0) {
        return [IntPtr]$fallbackProcess.MainWindowHandle
      }
    }

    Start-Sleep -Milliseconds 250
  }

  throw "Timed out waiting for a visible window from process '$ProcessName'."
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

  [DllImport("user32.dll")]
  public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, int nFlags);
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
      $deviceContext = $graphics.GetHdc()
      try {
        $printed = [NativeMethods]::PrintWindow($WindowHandle, $deviceContext, 2)
      } finally {
        $graphics.ReleaseHdc($deviceContext)
      }
    } finally {
      $graphics.Dispose()
    }

    if (-not $printed) {
      Prepare-WindowForScreenCapture -WindowHandle $WindowHandle

      $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
      try {
        $source = New-Object System.Drawing.Point($rect.Left, $rect.Top)
        $target = [System.Drawing.Point]::Empty
        $size = New-Object System.Drawing.Size($width, $height)
        $graphics.CopyFromScreen($source, $target, $size)
      } finally {
        $graphics.Dispose()
      }
    }

    return $bitmap
  } catch {
    $bitmap.Dispose()
    throw
  }
}

$repoRoot = Get-RepoRoot
$resolvedExePath = Resolve-ExePath -RequestedPath $ExePath -RepoRoot $repoRoot `
    -PlatformName $Platform -ConfigurationName $Configuration
$resolvedOutputPath = Resolve-OutputPath -RequestedPath $Path -RepoRoot $repoRoot
$processName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedExePath)
$startedProcess = $null

try {
  $startedProcess = Start-Process -FilePath $resolvedExePath `
      -WorkingDirectory (Split-Path -Parent $resolvedExePath) -PassThru
  $windowHandle = Wait-ForMainWindowHandle -PrimaryProcess $startedProcess `
      -ProcessName $processName -TimeoutSeconds $StartupTimeoutSeconds

  if ($RenderDelayMilliseconds -gt 0) {
    Start-Sleep -Milliseconds $RenderDelayMilliseconds
  }

  $bitmap = Capture-WindowBitmap -WindowHandle $windowHandle
  try {
    $bitmap.Save($resolvedOutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally {
    $bitmap.Dispose()
  }

  Write-Output "EXE=$resolvedExePath"
  Write-Output "HANDLE=$([Int64]$windowHandle)"
  Write-Output "SCREENSHOT=$resolvedOutputPath"
} finally {
  if ($startedProcess -ne $null -and -not $KeepOpen) {
    try {
      if (-not $startedProcess.HasExited) {
        $startedProcess.CloseMainWindow() | Out-Null
        $startedProcess.WaitForExit(5000) | Out-Null
      }
    } catch {
    }
  }
}
