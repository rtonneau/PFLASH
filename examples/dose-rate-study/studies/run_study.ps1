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

  With -Study, each history is a Study run of the /study skill instead:
    study.ps1 run <study> <runSet> --id <id> --seed ... --param ... -- history.exe --config <cfg> --output {output}
  Its output goes to ${STUDIES_ROOT}/<project>/<study>/runs/<runSet>/<id>/output (not the plan's
  run.output), beside the run's stamp.json and stdout.log. Resume reads the ledger there; a failed
  attempt is rerun with --rerun (the wrapper keeps it as <id>.prev-<time>). The wrapper's own
  console goes to <run set>.logs/ beside the run set. Analyze with
  analyze.py --plan <plan> --runs-dir <run set dir>.

.PARAMETER Plan         plan.json
.PARAMETER History       history executable (default ../build-ninja/history.exe)
.PARAMETER Geant4Bin     bin directory of the PATCHED Geant4 install, put first on PATH
                         (default ${DEV_DIR}/GEANT4/geant4-v11.4.3-mf-ti-install/bin, patch set mf-ti)
.PARAMETER LogCsv        run log (default <plan dir>/run_log.csv, appended)
.PARAMETER Parallel      histories at once (default 1); slowest rates (lowest dose rate) first
.PARAMETER WallTimeCapS  per-history wall-time cap in s (default: plan study.wallTimeCapS, else 0 = none)
.PARAMETER PollS         memory sampling interval in s (default 2)
.PARAMETER Only          run only these run ids
.PARAMETER DryRun        print what would run
.PARAMETER Study         Study id or unique slug: launch each history through study.ps1 run
.PARAMETER RunSet        run set of the Study (default: the plan file's directory name)
.PARAMETER StudyScript   study.ps1 (default ~/.claude/skills/study/scripts/study.ps1)
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$Plan,
  [string]$History = (Join-Path $PSScriptRoot '..\build-ninja\history.exe'),
  [string]$Geant4Bin = $(if ($env:DEV_DIR) { Join-Path $env:DEV_DIR 'GEANT4\geant4-v11.4.3-mf-ti-install\bin' }),
  [string]$LogCsv,
  [int]$Parallel = 1,
  [double]$WallTimeCapS = -1,
  [double]$PollS = 2,
  [string[]]$Only,
  [switch]$DryRun,
  [string]$Study,
  [string]$RunSet,
  [string]$StudyScript = (Join-Path $HOME '.claude\skills\study\scripts\study.ps1')
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
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))

# -Study: find the run set directory under STUDIES_ROOT (study.ps1 status --json).
$runSetDir = $null
if ($Study) {
  if (-not $RunSet) { $RunSet = Split-Path -Leaf $planDir }
  if ($RunSet -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw "run set '$RunSet' must use letters, digits, '.', '_' or '-' (pass -RunSet)" }
  if (-not (Test-Path -LiteralPath $StudyScript)) { throw "study.ps1 not found: $StudyScript (install the /study skill with claude-sync-skills.ps1)" }
  $statusOut = & pwsh -NoProfile -File $StudyScript status $Study --json --project-root $repoRoot 2>&1
  if ($LASTEXITCODE -eq 0) {
    $info = ($statusOut | Out-String) | ConvertFrom-Json
    $Study = $info.study.id
    $dataPath = [string]$info.study.dataPath
    foreach ($v in 'STUDIES_ROOT', 'DEV_DIR', 'G4_ROOT') { $dataPath = $dataPath.Replace('${' + $v + '}', [string][Environment]::GetEnvironmentVariable($v)) }
    $runSetDir = [IO.Path]::GetFullPath((Join-Path (Join-Path $dataPath 'runs') $RunSet))
    $logDir = "$runSetDir.logs"
  } elseif ($DryRun) {
    Write-Warning "study '$Study' not resolved, listing every run: $(($statusOut | Out-String).Trim())"
  } else {
    throw "study '$Study' not resolved: $(($statusOut | Out-String).Trim())"
  }
}

# study.ps1 run arguments of one run; the wrapper replaces {output} by the run's output/.
function Get-StudyArgs($t) {
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $a = @('-NoProfile', '-File', $StudyScript, 'run', $Study, $RunSet, '--id', $t.id, '--project-root', $repoRoot, '--quiet')
  if ($t.rerun) { $a += '--rerun' }
  foreach ($k in $t.seeds.Keys) { $a += @('--seed', "$k=$($t.seeds[$k])") }
  $a += @('--param', "rate_Gy_s=$($t.rate.ToString('R', $inv))")
  if ($null -ne $t.firstPacket) { $a += @('--param', "firstPacket=$($t.firstPacket)") }
  $a += @('--', $History, '--config', $t.config, '--output', '{output}')
  return , $a
}

function Test-RunPassed([string]$outDir) {
  $ledger = Join-Path $outDir 'ledger.json'
  if (-not (Test-Path -LiteralPath $ledger)) { return $false }
  try { return [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) }
  catch { return $false }
}

$todo = [Collections.Generic.List[object]]::new()
foreach ($run in $planDoc.runs) {
  if ($Only -and ($Only -notcontains $run.id)) { continue }
  $rerun = $false
  if ($Study) {
    # The wrapper owns the run directory; an earlier failed attempt is set aside by --rerun.
    $outDir = if ($runSetDir) { Join-Path (Join-Path $runSetDir $run.id) 'output' } else { $null }
    if ($outDir -and (Test-RunPassed $outDir)) { Write-Host "$($run.id): done, skipped"; continue }
    $rerun = [bool]($runSetDir -and (Test-Path -LiteralPath (Join-Path $runSetDir $run.id)))
  } else {
    $outDir = [IO.Path]::GetFullPath((Join-Path $planDir $run.output))
    if (Test-RunPassed $outDir) { Write-Host "$($run.id): done, skipped"; continue }
    if ((Test-Path -LiteralPath $outDir) -and -not $outDir.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar)) {
      throw "refusing to remove an incomplete output outside the output root: $outDir"
    }
  }
  $seeds = [ordered]@{}
  foreach ($k in 'chemistrySeed', 'sourceSeed') { if ($run.PSObject.Properties[$k]) { $seeds[$k] = $run.$k } }
  $todo.Add([pscustomobject]@{ id = $run.id; rate = [double]$run.rate_Gy_s
      config = [IO.Path]::GetFullPath((Join-Path $planDir $run.config)); outDir = $outDir
      rerun = $rerun; seeds = $seeds; firstPacket = $run.firstPacket; studyArgs = $null })
}
if ($DryRun) {
  foreach ($t in $todo) {
    if ($Study) {
      $shown = (Get-StudyArgs $t | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
      Write-Host "$($t.id): would run  pwsh $shown"
    } else {
      Write-Host "$($t.id): would run  history --config $($t.config) --output $($t.outDir)"
    }
  }
  Write-Host "run_study: $($todo.Count) would run (parallel $Parallel, cap $WallTimeCapS s)"
  exit 0
}
if (-not (Test-Path -LiteralPath $History)) { throw "history executable not found: $History" }
$History = (Resolve-Path -LiteralPath $History).Path
if ($Study) { foreach ($t in $todo) { $t.studyArgs = Get-StudyArgs $t } }
if (-not $Geant4Bin) { throw 'DEV_DIR is not set: run ~/dotfiles/scripts/windows/setup-dev-env.ps1, or pass -Geant4Bin' }
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
  $cap = $using:WallTimeCapS; $poll = $using:PollS; $study = $using:Study; $runSetDir = $using:runSetDir
  $log = Join-Path $logDir "$($t.id).log"
  $err = Join-Path $logDir "$($t.id).err"
  $start = [DateTime]::UtcNow
  $watch = [Diagnostics.Stopwatch]::StartNew()
  if ($study) {
    # The wrapper creates the run directory and tees history's console to <run>/stdout.log.
    $quoted = @($t.studyArgs | ForEach-Object { if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ } })
    $p = Start-Process -FilePath 'pwsh' -ArgumentList $quoted `
         -RedirectStandardOutput $log -RedirectStandardError $err -NoNewWindow -PassThru
  } else {
    if (Test-Path -LiteralPath $t.outDir) { Remove-Item -LiteralPath $t.outDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $t.outDir | Out-Null
    $p = Start-Process -FilePath $History -ArgumentList @('--config', "`"$($t.config)`"", '--output', "`"$($t.outDir)`"") `
         -RedirectStandardOutput $log -RedirectStandardError $err -NoNewWindow -PassThru
  }
  $handle = $p.Handle   # keep the handle so ExitCode is readable after exit
  $peak = 0L
  $truncated = $false
  while (-not $p.HasExited) {
    try { $p.Refresh(); if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 } } catch {}
    if ($cap -gt 0 -and $watch.Elapsed.TotalSeconds -gt $cap) {
      $truncated = $true
      if ($study) {
        # Kill history (the wrapper's child) so the wrapper still completes the stamp;
        # kill the whole tree only if the wrapper does not exit on its own.
        Get-CimInstance Win32_Process -Filter "ParentProcessId = $($p.Id)" -ErrorAction SilentlyContinue |
          ForEach-Object { try { (Get-Process -Id $_.ProcessId -ErrorAction Stop).Kill($true) } catch {} }
        if (-not $p.WaitForExit(60000)) { try { $p.Kill($true) } catch {} }
      } else {
        try { $p.Kill() } catch {}
      }
      [void]$p.WaitForExit(30000)
      break
    }
    [void]$p.WaitForExit([int]($poll * 1000))
  }
  $p.WaitForExit()
  $watch.Stop()
  $exit = $p.ExitCode
  if ($study) {
    # history's own log and peak (the sampled peak above is the wrapper's pwsh; a run too
    # short for the wrapper to sample leaves the peak empty).
    $runDir = Join-Path $runSetDir $t.id
    $t.outDir = Join-Path $runDir 'output'
    $log = Join-Path $runDir 'stdout.log'
    $peak = -1L
    try {
      $stamp = Get-Content -LiteralPath (Join-Path $runDir 'stamp.json') -Raw -Encoding utf8 | ConvertFrom-Json
      if ($stamp.peakWorkingSet_MB) { $peak = [long]([double]$stamp.peakWorkingSet_MB * 1MB) }
    } catch {}
  }
  $done =(Select-String -LiteralPath $log -Pattern 'HISTORY_DONE' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $prog = (Select-String -LiteralPath $log -Pattern 'HISTORY_PROGRESS' -ErrorAction SilentlyContinue | Select-Object -Last 1).Line
  $passed = $false
  $ledger = Join-Path $t.outDir 'ledger.json'
  if (-not $truncated -and $exit -eq 0 -and (Test-Path -LiteralPath $ledger)) {
    try { $passed = [bool]((Get-Content -LiteralPath $ledger -Raw -Encoding utf8 | ConvertFrom-Json).checks.allPassed) } catch {}
  }
  $status = if ($truncated) { 'truncated' } elseif ($passed) { 'ok' } else { 'FAILED' }
  $wall = $watch.Elapsed.TotalSeconds.ToString('F1', $inv)
  $peakMb = if ($peak -ge 0) { ($peak / 1MB).ToString('F1', $inv) } else { '' }
  $q = { param($s) '"' + ([string]$s).Replace('"', '""') + '"' }
  $fields = @($t.id, $t.rate.ToString('R', $inv), $start.ToString('s'), $wall, $peakMb, $exit, $status, (& $q $prog), (& $q $done))
  $mutex = [Threading.Mutex]::new($false, 'Global\drs_run_study_log')
  [void]$mutex.WaitOne()
  try { ($fields -join ',') | Add-Content -LiteralPath $LogCsv -Encoding utf8 } finally { $mutex.ReleaseMutex() }
  Write-Host "$($t.id): $status, exit $exit, $wall s, peak $(if ($peakMb) { "$peakMb MB" } else { 'n/a' })"
  [void]$handle
  [pscustomobject]@{ id = $t.id; status = $status; exit = $exit; log = $log }
}
$failed = @($results | Where-Object { $_.status -eq 'FAILED' })
$trunc = @($results | Where-Object { $_.status -eq 'truncated' })
Write-Host "run_study: $(@($results).Count) run (parallel $Parallel), $($trunc.Count) truncated, $($failed.Count) failed"
if ($failed.Count -gt 0) { exit 1 }
exit 0
