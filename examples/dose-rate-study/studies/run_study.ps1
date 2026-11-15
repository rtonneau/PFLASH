#requires -Version 7
<#
.SYNOPSIS
  Run the doseCutoff histories of a plan.json written by make_configs.py (resumable, parallel).

.DESCRIPTION
  Each run is `history --config <cfg> --output <dir>`, a separate single-threaded process.
  A run whose output holds a ledger.json with checks.allPassed true is skipped; an incomplete
  output directory (under the plan's output root only) is removed and rerun.
  While a run is alive its peak working set is sampled every -PollS seconds. A run still alive
  after -WallTimeCapS seconds is killed (status 'truncated'); its last HISTORY_PROGRESS line
  (simulated time, activated sources, live molecules) is kept in the log so analysis can tell how
  far it got. Each run appends one row to run_log.csv:
    id,rate_Gy_s,startUtc,wallTime_s,peakWorkingSet_MB,exitCode,status,lastProgress,historyDone
  Console output of each run goes to <outputRoot>/logs/<id>.log (stderr to <id>.err).

.PARAMETER Plan          plan.json
.PARAMETER History       history executable (default ../build-ninja/history.exe)
.PARAMETER Geant4Bin     bin directory of the PATCHED Geant4 install, put first on PATH
.PARAMETER LogCsv        run log (default <plan dir>/run_log.csv, appended)
.PARAMETER Parallel      histories at once (default 1); slowest rates (lowest dose rate) first
.PARAMETER WallTimeCapS  per-history wall-time cap in s (default: plan study.wallTimeCapS, else 0 = none)
.PARAMETER PollS         memory sampling interval in s (default 2)
.PARAMETER Only          run only these run ids
.PARAMETER DryRun        print what would run
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$Plan,
  [string]$History = (Join-Path $PSScriptRoot '..\build-ninja\history.exe'),
  [string]$Geant4Bin = (Join-Path $PSScriptRoot '..\..\..\.scratch\tests\2026-11-08__beam-on-irt-beam-off-meso\install\bin'),
  [string]$LogCsv,
  [int]$Parallel = 1,
  [double]$WallTimeCapS = -1,
  [double]$PollS = 2,
  [string[]]$Only,
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$planPath = (Resolve-Path -LiteralPath $Plan).Path
$planDir = Split-Path -Parent $planPath
$planDoc = Get-Content -LiteralPath $planPath -Raw -Encoding utf8 | ConvertFrom-Json
if (-not $LogCsv) { $LogCsv = Join-Path $planDir 'run_log.csv' }
if ($WallTimeCapS -lt 0) {
  $WallTimeCapS = if ($planDoc.study.wallTimeCapS) { [double]$planDoc.study.wallTimeCapS } else { 0 }
}
$outputRoot = [IO.Path]::GetFullPath((Join-Path $planDir $planDoc.outputRoot))
$logDir = Join-Path $outputRoot 'logs'

function Test-RunPassed([string]$outDir) {
  $ledger = Join-Path $outDir 'ledger.json'
  if (-not (Test-Path -LiteralPath $ledger)) { return $false }
  try { return [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) }
  catch { return $false }
}

$todo = [Collections.Generic.List[object]]::new()
foreach ($run in $planDoc.runs) {
  if ($Only -and ($Only -notcontains $run.id)) { continue }
  $outDir = [IO.Path]::GetFullPath((Join-Path $planDir $run.output))
  if (Test-RunPassed $outDir) { Write-Host "$($run.id): done, skipped"; continue }
  if ((Test-Path -LiteralPath $outDir) -and -not $outDir.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar)) {
    throw "refusing to remove an incomplete output outside the output root: $outDir"
  }
  $todo.Add([pscustomobject]@{ id = $run.id; rate = [double]$run.rate_Gy_s
      config = [IO.Path]::GetFullPath((Join-Path $planDir $run.config)); outDir = $outDir })
}
if ($DryRun) {
  foreach ($t in $todo) { Write-Host "$($t.id): would run  history --config $($t.config) --output $($t.outDir)" }
  Write-Host "run_study: $($todo.Count) would run (parallel $Parallel, cap $WallTimeCapS s)"
  exit 0
}
if (-not (Test-Path -LiteralPath $History)) { throw "history executable not found: $History" }
$History = (Resolve-Path -LiteralPath $History).Path
if (-not (Test-Path -LiteralPath $Geant4Bin)) { throw "Geant4 bin directory not found: $Geant4Bin" }
$env:PATH = (Resolve-Path -LiteralPath $Geant4Bin).Path + [IO.Path]::PathSeparator + $env:PATH
if (-not $env:GEANT4_DATA_DIR) { throw 'GEANT4_DATA_DIR is not set' }
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (-not (Test-Path -LiteralPath $LogCsv)) {
  'id,rate_Gy_s,startUtc,wallTime_s,peakWorkingSet_MB,exitCode,status,lastProgress,historyDone' |
    Set-Content -LiteralPath $LogCsv -Encoding utf8
}

$ordered = @($todo | Sort-Object -Property rate)   # low dose rates are the long ones
$results = $ordered | ForEach-Object -ThrottleLimit ([Math]::Max(1, $Parallel)) -Parallel {
  $t = $_
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $History = $using:History; $logDir = $using:logDir; $LogCsv = $using:LogCsv
  $cap = $using:WallTimeCapS; $poll = $using:PollS
  if (Test-Path -LiteralPath $t.outDir) { Remove-Item -LiteralPath $t.outDir -Recurse -Force }
  New-Item -ItemType Directory -Force -Path $t.outDir | Out-Null
  $log = Join-Path $logDir "$($t.id).log"
  $err = Join-Path $logDir "$($t.id).err"
  $start = [DateTime]::UtcNow
  $watch = [Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath $History -ArgumentList @('--config', "`"$($t.config)`"", '--output', "`"$($t.outDir)`"") `
       -RedirectStandardOutput $log -RedirectStandardError $err -NoNewWindow -PassThru
  $handle = $p.Handle   # keep the handle so ExitCode is readable after exit
  $peak = 0L
  $truncated = $false
  while (-not $p.HasExited) {
    try { $p.Refresh(); if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 } } catch {}
    if ($cap -gt 0 -and $watch.Elapsed.TotalSeconds -gt $cap) {
      $truncated = $true
      try { $p.Kill() } catch {}
      [void]$p.WaitForExit(30000)
      break
    }
    [void]$p.WaitForExit([int]($poll * 1000))
  }
  $p.WaitForExit()
  $watch.Stop()
  $exit = $p.ExitCode
  $done = (Select-String -LiteralPath $log -Pattern 'HISTORY_DONE' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $prog = (Select-String -LiteralPath $log -Pattern 'HISTORY_PROGRESS' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $passed = $false
  $ledger = Join-Path $t.outDir 'ledger.json'
  if (-not $truncated -and $exit -eq 0 -and (Test-Path -LiteralPath $ledger)) {
    try { $passed = [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) } catch {}
  }
  $status = if ($truncated) { 'truncated' } elseif ($passed) { 'ok' } else { 'FAILED' }
  $wall = $watch.Elapsed.TotalSeconds.ToString('F1', $inv)
  $peakMb = ($peak / 1MB).ToString('F1', $inv)
  $q = { param($s) '"' + ([string]$s).Replace('"', '""') + '"' }
  $fields = @($t.id, $t.rate.ToString('R', $inv), $start.ToString('s'), $wall, $peakMb, $exit, $status, (& $q $prog), (& $q $done))
  $mutex = [Threading.Mutex]::new($false, 'Global\drs_run_study_log')
  [void]$mutex.WaitOne()
  try { ($fields -join ',') | Add-Content -LiteralPath $LogCsv -Encoding utf8 } finally { $mutex.ReleaseMutex() }
  Write-Host "$($t.id): $status, exit $exit, $wall s, peak $peakMb MB"
  [void]$handle
  [pscustomobject]@{ id = $t.id; status = $status; exit = $exit; log = $log }
}
$failed = @($results | Where-Object { $_.status -eq 'FAILED' })
$trunc = @($results | Where-Object { $_.status -eq 'truncated' })
Write-Host "run_study: $(@($results).Count) run (parallel $Parallel), $($trunc.Count) truncated, $($failed.Count) failed"
if ($failed.Count -gt 0) { exit 1 }
exit 0
