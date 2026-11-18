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
                       (default ${DEV_DIR}/GEANT4/geant4-v11.4.3-mf-ti-install/bin, patch set mf-ti)
.PARAMETER LogCsv      run log (default: <plan dir>/run_log.csv, appended)
.PARAMETER DryRun      print what would run; run and write nothing
.PARAMETER Limit       run at most this many (not skipped) histories
.PARAMETER Parallel    histories run at once (separate single-threaded processes; default 1).
                       Longest runs first. A failed run no longer stops the others; the
                       script exits 1 at the end when any run failed.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$Plan,
  [string]$History = (Join-Path $PSScriptRoot '..\..\build-patched\history.exe'),
  [string]$Geant4Bin = $(if ($env:DEV_DIR) { Join-Path $env:DEV_DIR 'GEANT4\geant4-v11.4.3-mf-ti-install\bin' }),
  [string]$LogCsv,
  [switch]$DryRun,
  [int]$Limit = 0,
  [int]$Parallel = 1
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
  if (-not $Geant4Bin) { throw 'DEV_DIR is not set: run ~/dotfiles/scripts/windows/setup-dev-env.ps1, or pass -Geant4Bin' }
  if (-not (Test-Path -LiteralPath $Geant4Bin)) { throw "Geant4 bin directory not found: $Geant4Bin" }
  $env:PATH = (Resolve-Path -LiteralPath $Geant4Bin).Path + [IO.Path]::PathSeparator + $env:PATH
  if (-not $env:GEANT4_DATA_DIR) { throw 'GEANT4_DATA_DIR is not set (directory with G4EMLOW..., G4PARTICLEXS...)' }
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null
  if (-not (Test-Path -LiteralPath $LogCsv)) {
    'id,startUtc,wallTime_s,exitCode,status,historyDone' | Set-Content -LiteralPath $LogCsv -Encoding utf8
  }
}

$total = @($planDoc.runs).Count
$skipped = 0
$todo = [Collections.Generic.List[object]]::new()
$i = 0
foreach ($run in $planDoc.runs) {
  $i++
  $outDir = [IO.Path]::GetFullPath((Join-Path $planDir $run.output))
  if (Test-RunPassed $outDir) { $skipped++; Write-Host "[$i/$total] $($run.id): done, skipped"; continue }
  if ($Limit -gt 0 -and $todo.Count -ge $Limit) { Write-Host "[$i/$total] $($run.id): limit reached, not run"; continue }
  $todo.Add([pscustomobject]@{
    index = $i; id = $run.id; nPackets = [int]$run.nPackets
    config = [IO.Path]::GetFullPath((Join-Path $planDir $run.config)); outDir = $outDir })
}
if ($DryRun) {
  foreach ($t in $todo) { Write-Host "[$($t.index)/$total] $($t.id): would run  history --config $($t.config) --output $($t.outDir)" }
  Write-Host "run_study: $total runs in the plan, $skipped already done, $($todo.Count) would run (parallel $Parallel)"
  exit 0
}
foreach ($t in $todo) {
  if ((Test-Path -LiteralPath $t.outDir) -and -not $t.outDir.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar)) {
    throw "refusing to remove an incomplete output outside the output root: $($t.outDir)"
  }
}
# Longest runs (most packets) first, so the parallel slots finish close together.
$ordered = @($todo | Sort-Object -Property @{ Expression = 'nPackets'; Descending = $true }, index)
$results = $ordered | ForEach-Object -ThrottleLimit ([Math]::Max(1, $Parallel)) -Parallel {
  $t = $_
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $History = $using:History; $logDir = $using:logDir; $LogCsv = $using:LogCsv; $total = $using:total
  if (Test-Path -LiteralPath $t.outDir) {
    Write-Host "[$($t.index)/$total] $($t.id): removing incomplete output"
    Remove-Item -LiteralPath $t.outDir -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $t.outDir) | Out-Null
  $log = Join-Path $logDir "$($t.id).log"
  $start = [DateTime]::UtcNow
  $watch = [Diagnostics.Stopwatch]::StartNew()
  & $History --config $t.config --output $t.outDir *> $log
  $exit = $LASTEXITCODE
  $watch.Stop()
  $line = (Select-String -LiteralPath $log -Pattern 'HISTORY_DONE' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $ledger = Join-Path $t.outDir 'ledger.json'
  $passed = $false
  if (($exit -eq 0) -and (Test-Path -LiteralPath $ledger)) {
    try { $passed = [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) } catch {}
  }
  $status = if ($passed) { 'ok' } else { 'FAILED' }
  $wall = $watch.Elapsed.TotalSeconds.ToString('F1', $inv)
  $fields = @($t.id, $start.ToString('s'), $wall, $exit, $status, ('"' + ([string]$line).Replace('"', '""') + '"'))
  $mutex = [Threading.Mutex]::new($false, 'Global\ptirt_run_study_log')
  [void]$mutex.WaitOne()
  try { ($fields -join ',') | Add-Content -LiteralPath $LogCsv -Encoding utf8 } finally { $mutex.ReleaseMutex() }
  Write-Host "[$($t.index)/$total] $($t.id): $status, exit $exit, $wall s"
  [pscustomobject]@{ id = $t.id; passed = $passed; exit = $exit; log = $log }
}
$failed = @($results | Where-Object { -not $_.passed })
Write-Host "run_study: $total runs in the plan, $skipped already done, $(@($results).Count) run (parallel $Parallel), $($failed.Count) failed"
if ($failed.Count -gt 0) {
  foreach ($f in $failed) { Write-Error "run $($f.id) failed (exit $($f.exit)); see $($f.log)" -ErrorAction Continue }
  exit 1
}
exit 0
