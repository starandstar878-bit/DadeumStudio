# Gyeol UI Release Checklist

## UI Quality Gate
- Capture baseline snapshots in-app: `UI menu -> Capture UI Baseline`.
- Capture candidate snapshots in-app (or by running the app build under target changes).
- Run pixel-diff gate: `tools\\ui_quality_gate.bat`.
- Block release if gate exits non-zero or if `Builds/UISnapshots/ui_quality_gate_ci_report.txt` reports failures.

## Required Viewports
- `wide` (>=1600)
- `desktop` (1200-1599)
- `tablet` (900-1199)
- `narrow` (<900)

## Accessibility Checks
- Run in-app: `UI menu -> Run Accessibility Audit`.
- Ensure no critical contrast/focus/hit-target failures before release.