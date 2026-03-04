param(
  [string]$BaselineDir = "Builds/UISnapshots/baseline",
  [string]$CandidateDir = "Builds/UISnapshots/candidate",
  [double]$PerImageThreshold = 0.015,
  [double]$AggregateThreshold = 0.010,
  [int]$ChannelTolerance = 14
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function Get-DiffRatio {
  param(
    [Parameter(Mandatory = $true)][string]$LeftPath,
    [Parameter(Mandatory = $true)][string]$RightPath,
    [Parameter(Mandatory = $true)][int]$Tolerance
  )

  $left = [System.Drawing.Bitmap]::new($LeftPath)
  $right = [System.Drawing.Bitmap]::new($RightPath)
  try {
    if ($left.Width -ne $right.Width -or $left.Height -ne $right.Height) {
      return 1.0
    }

    $total = [double]($left.Width * $left.Height)
    if ($total -le 0) {
      return 0.0
    }

    $changed = 0.0
    for ($y = 0; $y -lt $left.Height; $y++) {
      for ($x = 0; $x -lt $left.Width; $x++) {
        $a = $left.GetPixel($x, $y)
        $b = $right.GetPixel($x, $y)

        if ([Math]::Abs($a.R - $b.R) -gt $Tolerance -or
            [Math]::Abs($a.G - $b.G) -gt $Tolerance -or
            [Math]::Abs($a.B - $b.B) -gt $Tolerance -or
            [Math]::Abs($a.A - $b.A) -gt $Tolerance) {
          $changed += 1.0
        }
      }
    }

    return ($changed / $total)
  }
  finally {
    $left.Dispose()
    $right.Dispose()
  }
}

if (!(Test-Path $BaselineDir)) {
  Write-Error "Baseline directory not found: $BaselineDir"
}
if (!(Test-Path $CandidateDir)) {
  Write-Error "Candidate directory not found: $CandidateDir"
}

$baselineRoot = Resolve-Path $BaselineDir
$candidateRoot = Resolve-Path $CandidateDir
$baselineFiles = Get-ChildItem -Path $baselineRoot -Recurse -File -Filter *.png

if ($baselineFiles.Count -eq 0) {
  Write-Error "No baseline PNG files found in: $baselineRoot"
}

$failed = 0
$compared = 0
$sumRatio = 0.0
$reportLines = @()

foreach ($file in $baselineFiles) {
  $relative = $file.FullName.Substring($baselineRoot.Path.Length).TrimStart('\\', '/')
  $candidatePath = Join-Path $candidateRoot $relative

  if (!(Test-Path $candidatePath)) {
    $failed++
    $reportLines += "MISSING  $relative"
    continue
  }

  $ratio = Get-DiffRatio -LeftPath $file.FullName -RightPath $candidatePath -Tolerance $ChannelTolerance
  $compared++
  $sumRatio += $ratio

  if ($ratio -gt $PerImageThreshold) {
    $failed++
    $reportLines += "FAIL     $relative  diff=$('{0:N4}' -f $ratio)"
  } else {
    $reportLines += "PASS     $relative  diff=$('{0:N4}' -f $ratio)"
  }
}

$aggregate = if ($compared -gt 0) { $sumRatio / $compared } else { 1.0 }
$aggregateFailed = $aggregate -gt $AggregateThreshold
if ($aggregateFailed) {
  $failed++
}

$summary = @()
$summary += "Compared: $compared"
$summary += "AggregateDiff: $('{0:N4}' -f $aggregate)"
$summary += "PerImageThreshold: $PerImageThreshold"
$summary += "AggregateThreshold: $AggregateThreshold"
$summary += "Failures: $failed"

$reportPath = Join-Path (Split-Path -Parent $candidateRoot) "ui_quality_gate_ci_report.txt"
($summary + "" + $reportLines) | Set-Content -Path $reportPath -Encoding UTF8

$summary | ForEach-Object { Write-Host $_ }
Write-Host "Report: $reportPath"

if ($failed -gt 0) {
  exit 1
}
exit 0