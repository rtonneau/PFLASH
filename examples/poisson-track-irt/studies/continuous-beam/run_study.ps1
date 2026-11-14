#requires -Version 7
<#
.SYNOPSIS
  Run every history of a plan.json written by make_schedules.py (resumable).

.DESCRIPTION
  For each run of the plan: skip it when its output already holds a ledger.json with
  checks.allPassed true; otherwise (removing an incomplete output directory under the
  plan's output root) run `history --config <cfg> --output <dir>`, log wall time, exit
  status and the HISTORY_DONE line to a CSV, and stop at the first failed run.

.PARAMETER Plan        plan.json
.PARAMETER History     history executable (default: ../../build-patched/history.exe)
.PARAMETER Geant4Bin   bin directory of the PATCHED Geant4 install, put first on PATH
.PARAMETER LogCsv      run log (default: <plan dir>/run_log.csv, appended)
.PARAMETER DryRun      print what would run; run and write nothing
.PARAMETER Limit       run at most this many (not skipped) histories
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$Plan,
  [string]$History = (Join-Path $PSScriptRoot '..\..\build-patched\history.exe'),
  [string]$Geant4Bin = (Join-Path $PSScriptRoot '..\..\..\..\.scratch\tests\2026-11-08__beam-on-irt-beam-off-meso\install\bin'),
  [string]$LogCsv,
  [switch]$DryRun,
  [int]$Limit = 0
)

$ErrorActionPreference = 'Stop'
$planPath = (Resolve-Path -LiteralPath $Plan).Path
$planDir = Split-Path -Parent $planPath
$planDoc = Get-Content -LiteralPath $planPath -Raw -Encoding utf8 | ConvertFrom-Json
if (-not $LogCsv) { $LogCsv = Join-Path $planDir 'run_log.csv' }
$outputRoot = [IO.Path]::GetFullPath((Join-Path $planDir $planDoc.outputRoot))
$logDir = Join-Path $outputRoot 'logs'

function Test-RunPassed([string]$outDir) {
  $ledger = Join-Path $outDir 'ledger.json'
  if (-not (Test-Path -LiteralPath $ledger)) { return $false }
  try { return [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) }
  catch { return $false }
}

if (-not $DryRun) {
  if (-not (Test-Path -LiteralPath $History)) { throw "history executable not found: $History" }
  $History = (Resolve-Path -LiteralPath $History).Path
  if (-not (Test-Path -LiteralPath $Geant4Bin)) { throw "Geant4 bin directory not found: $Geant4Bin" }
  $env:PATH = (Resolve-Path -LiteralPath $Geant4Bin).Path + [IO.Path]::PathSeparator + $env:PATH
  if (-not $env:GEANT4_DATA_DIR) { throw 'GEANT4_DATA_DIR is not set (directory with G4EMLOW..., G4PARTICLEXS...)' }
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null
  if (-not (Test-Path -LiteralPath $LogCsv)) {
    'id,startUtc,wallTime_s,exitCode,status,historyDone' | Set-Content -LiteralPath $LogCsv -Encoding utf8
  }
}

$total = @($planDoc.runs).Count
$done = 0; $skipped = 0; $ran = 0
foreach ($run in $planDoc.runs) {
  $done++
  $config = [IO.Path]::GetFullPath((Join-Path $planDir $run.config))
  $outDir = [IO.Path]::GetFullPath((Join-Path $planDir $run.output))
  if (Test-RunPassed $outDir) { $skipped++; Write-Host "[$done/$total] $($run.id): done, skipped"; continue }
  if ($Limit -gt 0 -and $ran -ge $Limit) { Write-Host "[$done/$total] $($run.id): limit reached, not run"; continue }
  if ($DryRun) {
    Write-Host "[$done/$total] $($run.id): would run  history --config $config --output $outDir"
    $ran++
    continue
  }
  if (Test-Path -LiteralPath $outDir) {
    if (-not $outDir.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar)) {
      throw "refusing to remove an incomplete output outside the output root: $outDir"
    }
    Write-Host "[$done/$total] $($run.id): removing incomplete output"
    Remove-Item -LiteralPath $outDir -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outDir) | Out-Null
  $log = Join-Path $logDir "$($run.id).log"
  $start = [DateTime]::UtcNow
  $watch = [Diagnostics.Stopwatch]::StartNew()
  & $History --config $config --output $outDir *> $log
  $exit = $LASTEXITCODE
  $watch.Stop()
  $ran++
  $line = (Select-String -LiteralPath $log -Pattern 'HISTORY_DONE' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $passed = ($exit -eq 0) -and (Test-RunPassed $outDir)
  $status = if ($passed) { 'ok' } else { 'FAILED' }
  $fields = @($run.id, $start.ToString('s'), $watch.Elapsed.TotalSeconds.ToString('F1', [Globalization.CultureInfo]::InvariantCulture), $exit, $status,
              ('"' + ([string]$line).Replace('"', '""') + '"'))
  ($fields -join ',') | Add-Content -LiteralPath $LogCsv -Encoding utf8
  Write-Host ("[$done/$total] $($run.id): $status, exit $exit, $($watch.Elapsed.TotalSeconds.ToString("F1", [Globalization.CultureInfo]::InvariantCulture)) s")
  if (-not $passed) {
    Write-Error "run $($run.id) failed (exit $exit, checks passed: $(Test-RunPassed $outDir)); see $log"
    exit 1
  }
}
Write-Host "run_study: $total runs in the plan, $skipped already done, $ran $(if ($DryRun) { 'would run' } else { 'run' })"
exit 0
