<#
.SYNOPSIS
  Separated-sources check: one shared history vs the sum of independent single-source histories.

.DESCRIPTION
  Sources far apart cannot influence each other, so a combined history (all packets of a
  deterministic schedule injected into ONE IRT_syn chemistry history) must equal, within
  statistics, the sum of the same packets evolved alone. This script
    1. captures the packet library (or uses -LibraryDir),
    2. runs <Seeds> combined histories (chemistry seeds CombinedFirstSeed + i),
    3. runs <Seeds> single-source histories for EACH schedule row (seed SingleFirstSeed +
       1000 * row + i), activated at the same absolute time as in the combined history,
       on the same record grid, with the same packets and placements,
    4. runs `compare` (cells.csv, summary.json, summary.txt; see include/SeparatedCheck.hh).
  Only the chemistry differs between the runs. Chemistry is IRT_syn only, pure water, no bulk
  scavenger, unbounded domain.

  Declared inputs (all recorded in <OutDir>/run-info.json): schedule file (its SHA-256),
  capture macro and seeds, the seeds above, endpoint, grid settings, tolerance, resource caps.
  Resource caps: at most -MaxParallel history processes at once, each killed after
  -TimeoutMinutes; the capture is capped the same way. Any failed or timed-out run stops
  the check (no new run is started; the running ones finish or time out).

  The Geant4 DLL directory is found from Geant4_DIR in <BuildDir>/CMakeCache.txt (as the
  ctest ENVIRONMENT_MODIFICATION does) and prepended to PATH for the child processes;
  override with -Geant4BinDir.

.PARAMETER BuildDir
  Build directory holding capture, history, compare (and capture-check) executables.
.PARAMETER OutDir
  Result directory; must not exist or must be empty.
.PARAMETER EndpointNs
  End of every history, ns. Default 3e6 (3 ms): the design default with the shipped schedule
  (activations at 0, 0.5 ms and 1 ms), ages 1 ns to 3 ms after the first activation.

.EXAMPLE
  pwsh examples/poisson-track-irt/scripts/RunSeparatedCheck.ps1 -BuildDir <build> -OutDir <scratch>/separated-check
#>
#Requires -Version 7
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$BuildDir,
  [Parameter(Mandatory)][string]$OutDir,
  [string]$ScheduleFile,
  [string]$CaptureMacro,
  [string]$LibraryDir,
  [double]$EndpointNs = 3e6,
  [ValidateRange(2, 1000)][int]$Seeds = 16,
  [ValidateRange(1, 2000000000)][int]$CaptureFirstSeed = 1,
  [ValidateRange(1, 2000000000)][int]$CombinedFirstSeed = 101,
  [ValidateRange(1, 2000000000)][int]$SingleFirstSeed = 1001,
  [double]$Sigmas = 4,
  [double]$RelativeTolerance = 0.05,
  [double]$AbsoluteFloor = 0,
  [ValidateRange(1, 64)][int]$MaxParallel = 6,
  [ValidateRange(1, 1440)][int]$TimeoutMinutes = 30,
  [string]$Geant4BinDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$exampleDir = Split-Path -Parent $PSScriptRoot
if (-not $ScheduleFile) { $ScheduleFile = Join-Path $exampleDir 'config/deterministic-schedule.txt' }
if (-not $CaptureMacro) { $CaptureMacro = Join-Path $exampleDir 'macros/capture.mac' }

function Stop-Check([string]$Message, [int]$Code = 2) {
  Write-Host "SEPARATED_CHECK_ERROR $Message"
  exit $Code
}

# ---- Inputs ------------------------------------------------------------------
$exeSuffix = $IsWindows ? '.exe' : ''
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$exes = @{}
foreach ($name in 'capture', 'capture-check', 'history', 'compare') {
  $path = Join-Path $BuildDir "$name$exeSuffix"
  if (-not (Test-Path -LiteralPath $path)) { Stop-Check "$path not found; build the example first" }
  $exes[$name] = $path
}
foreach ($file in $ScheduleFile, $CaptureMacro) {
  if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { Stop-Check "input file not found: $file" }
}
$ScheduleFile = (Resolve-Path -LiteralPath $ScheduleFile).Path
$CaptureMacro = (Resolve-Path -LiteralPath $CaptureMacro).Path
$OutDir = [IO.Path]::GetFullPath($OutDir)
if ((Test-Path -LiteralPath $OutDir) -and (Get-ChildItem -LiteralPath $OutDir -Force | Select-Object -First 1)) {
  Stop-Check "OutDir must be empty (refusing to mix runs): $OutDir"
}
if (-not ($EndpointNs -gt 0)) { Stop-Check 'EndpointNs must be positive' }

# Schedule rows: time_ns packet_index dx dy dz (comment lines start with '#').
$rows = @(Get-Content -LiteralPath $ScheduleFile | ForEach-Object { $_.Trim() } |
  Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object { , ($_ -split '\s+') })
if ($rows.Count -lt 2) { Stop-Check "the schedule needs at least 2 rows: $ScheduleFile" }
foreach ($row in $rows) {
  if ($row.Count -ne 5) { Stop-Check "malformed schedule row '$($row -join ' ')' (expected 5 fields)" }
}
$activationText = @($rows | ForEach-Object { $_[0] })
$packetCount = 1 + ($rows | ForEach-Object { [int]$_[1] } | Measure-Object -Maximum).Maximum

# All seeds must be distinct (capture physics seeds included), so no two runs share a random stream.
$allSeeds = [System.Collections.Generic.List[long]]::new()
for ($i = 0; $i -lt $packetCount; $i++) { $allSeeds.Add([long]$CaptureFirstSeed + $i) }
for ($i = 0; $i -lt $Seeds; $i++) { $allSeeds.Add($CombinedFirstSeed + $i) }
for ($k = 0; $k -lt $rows.Count; $k++) {
  for ($i = 0; $i -lt $Seeds; $i++) { $allSeeds.Add([long]$SingleFirstSeed + 1000L * $k + $i) }
}
if (($allSeeds | Sort-Object -Unique).Count -ne $allSeeds.Count) {
  Stop-Check 'the seeds are not all distinct; change CaptureFirstSeed / CombinedFirstSeed / SingleFirstSeed'
}
if (($allSeeds | Measure-Object -Maximum).Maximum -gt 2147483647) {
  Stop-Check 'a chemistry seed exceeds 2147483647; lower SingleFirstSeed'
}

# Geant4 DLLs first on PATH, as the ctest ENVIRONMENT_MODIFICATION does.
if (-not $Geant4BinDir) {
  $cache = Join-Path $BuildDir 'CMakeCache.txt'
  $line = Select-String -LiteralPath $cache -Pattern '^Geant4_DIR:[A-Z]+=(.+)$' | Select-Object -First 1
  if (-not $line) { Stop-Check "Geant4_DIR not found in $cache; pass -Geant4BinDir" }
  $Geant4BinDir = [IO.Path]::GetFullPath((Join-Path $line.Matches[0].Groups[1].Value.Trim() '../../../bin'))
}
if (-not (Test-Path -LiteralPath $Geant4BinDir -PathType Container)) { Stop-Check "Geant4 bin directory not found: $Geant4BinDir" }
$env:PATH = $Geant4BinDir + [IO.Path]::PathSeparator + $env:PATH

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$runsDir = Join-Path $OutDir 'histories'
$inputsDir = Join-Path $OutDir 'inputs'
New-Item -ItemType Directory -Force -Path $runsDir, $inputsDir | Out-Null
Copy-Item -LiteralPath $ScheduleFile -Destination (Join-Path $inputsDir 'schedule.txt')
$scheduleCopy = Join-Path $inputsDir 'schedule.txt'

$clock = [System.Diagnostics.Stopwatch]::StartNew()
$runRecords = [System.Collections.Generic.List[object]]::new()

function Format-Path([string]$Path) { return $Path -replace '\\', '/' }

# Starts one process with output redirected to files (arguments quoted: Start-Process
# joins them with spaces) and returns its record for Invoke-Pool.
function Start-HistoryProcess([hashtable]$Job) {
  $info = @{
    Job      = $Job
    Watch    = [System.Diagnostics.Stopwatch]::StartNew()
    TimedOut = $false
  }
  $quoted = @($Job.Arguments | ForEach-Object { '"{0}"' -f $_ })
  $info.Process = Start-Process -FilePath $Job.Exe -ArgumentList $quoted -PassThru -NoNewWindow `
    -RedirectStandardOutput $Job.StdOut -RedirectStandardError $Job.StdErr
  $null = $info.Process.Handle  # keep the handle so ExitCode stays readable
  return $info
}

function Invoke-Pool([object[]]$Jobs, [int]$Parallel) {
  $pending = [System.Collections.Generic.Queue[hashtable]]::new()
  foreach ($job in $Jobs) { $pending.Enqueue($job) }
  $running = [System.Collections.Generic.List[hashtable]]::new()
  $failure = $null
  $deadline = [TimeSpan]::FromMinutes($TimeoutMinutes)
  while (($pending.Count -gt 0 -and -not $failure) -or $running.Count -gt 0) {
    while (-not $failure -and $pending.Count -gt 0 -and $running.Count -lt $Parallel) {
      $running.Add((Start-HistoryProcess $pending.Dequeue()))
    }
    foreach ($info in @($running)) {
      # Sticky: Kill() does not wait, so the process may only report exited on a later poll.
      if (-not $info.TimedOut -and -not $info.Process.HasExited -and $info.Watch.Elapsed -gt $deadline) {
        $info.TimedOut = $true
        $info.Process.Kill($true)
      }
      $timedOut = $info.TimedOut
      if ($info.Process.HasExited) {
        $info.Process.WaitForExit()
        $info.Watch.Stop()
        $job = $info.Job
        $stdout = Get-Content -LiteralPath $job.StdOut -Raw -ErrorAction SilentlyContinue
        $done = $stdout -and $stdout.Contains($job.DoneMarker)
        $ok = (-not $timedOut) -and $info.Process.ExitCode -eq 0 -and $done
        $runRecords.Add([pscustomobject]@{
            name = $job.Name; kind = $job.Kind; source = $job.Source; seed = $job.Seed
            exitCode = $info.Process.ExitCode; timedOut = $timedOut
            seconds = [math]::Round($info.Watch.Elapsed.TotalSeconds, 2); ok = $ok })
        if (-not $ok -and -not $failure) {
          $failure = "run $($job.Name) failed (exit $($info.Process.ExitCode), timedOut=$timedOut); see $($job.StdErr)"
        }
        $running.Remove($info) | Out-Null
      }
    }
    if ($running.Count -gt 0) { Start-Sleep -Milliseconds 500 }
  }
  if ($failure) { Stop-Check $failure 1 }
}

# ---- 1. Packet library --------------------------------------------------------
$captureSeconds = 0
if ($LibraryDir) {
  $LibraryDir = (Resolve-Path -LiteralPath $LibraryDir).Path
  Write-Host "Using the packet library $LibraryDir"
} else {
  $LibraryDir = Join-Path $OutDir 'library'
  Write-Host "Capturing $packetCount packets (first seed $CaptureFirstSeed) into $LibraryDir"
  $captureJob = @{
    Name = 'capture'; Kind = 'capture'; Source = -1; Seed = $CaptureFirstSeed; Exe = $exes['capture']
    Arguments = @('--macro', $CaptureMacro, '--output', $LibraryDir, '--count', $packetCount, '--first-seed', $CaptureFirstSeed)
    StdOut = Join-Path $runsDir 'capture.out.txt'; StdErr = Join-Path $runsDir 'capture.err.txt'; DoneMarker = 'CAPTURE_DONE' }
  Invoke-Pool @($captureJob) 1
  $captureSeconds = $runRecords[$runRecords.Count - 1].seconds
}
$check = & $exes['capture-check'] $LibraryDir --expect-count $packetCount
if ($LASTEXITCODE -ne 0) { Stop-Check "capture-check failed on $LibraryDir : $($check | Select-Object -Last 1)" 1 }
Write-Host ($check | Select-Object -Last 1)

# ---- 2. Histories -------------------------------------------------------------
function New-HistoryJob([string]$Name, [string]$Kind, [int]$Source, [int]$Seed) {
  $dir = Join-Path $runsDir $Name
  $lines = @("mode = $Kind", "libraryDir = $(Format-Path $LibraryDir)", "scheduleFile = $(Format-Path $scheduleCopy)")
  if ($Kind -eq 'single') { $lines += "singleSource = $Source" }
  $lines += @("endpointNs = $($EndpointNs.ToString('R', [cultureinfo]::InvariantCulture))", "chemistrySeed = $Seed")
  $config = Join-Path $runsDir "$Name.cfg"
  Set-Content -LiteralPath $config -Value $lines
  return @{
    Name = $Name; Kind = $Kind; Source = $Source; Seed = $Seed; Exe = $exes['history']
    Arguments = @('--config', $config, '--output', $dir)
    StdOut = Join-Path $runsDir "$Name.out.txt"; StdErr = Join-Path $runsDir "$Name.err.txt"
    DoneMarker = 'HISTORY_DONE'; OutputDir = $dir }
}
$jobs = @()
for ($i = 0; $i -lt $Seeds; $i++) {
  $jobs += New-HistoryJob ('combined-s{0:D2}' -f $i) 'deterministic' -1 ($CombinedFirstSeed + $i)
}
for ($k = 0; $k -lt $rows.Count; $k++) {
  for ($i = 0; $i -lt $Seeds; $i++) {
    $jobs += New-HistoryJob ('single-k{0}-s{1:D2}' -f $k, $i) 'single' $k ($SingleFirstSeed + 1000 * $k + $i)
  }
}
Write-Host "Running $($jobs.Count) histories ($Seeds combined, $Seeds x $($rows.Count) single), at most $MaxParallel at once, $TimeoutMinutes min each, endpoint $EndpointNs ns"
$historyWatch = [System.Diagnostics.Stopwatch]::StartNew()
Invoke-Pool $jobs $MaxParallel
$historySeconds = [math]::Round($historyWatch.Elapsed.TotalSeconds, 1)
Write-Host "All histories finished in $historySeconds s"

# ---- 3. Compare ---------------------------------------------------------------
$manifest = Join-Path $OutDir 'inputs/species-files.txt'
$manifestLines = @('# kind  activationNs  species.csv   (relative to this file)')
foreach ($job in $jobs) {
  $relative = 'histories/{0}/species.csv' -f $job.Name
  if ($job.Kind -eq 'single') {
    $manifestLines += "single $($activationText[$job.Source]) ../$relative"
  } else {
    $manifestLines += "combined - ../$relative"
  }
}
Set-Content -LiteralPath $manifest -Value $manifestLines

$compareDir = Join-Path $OutDir 'compare'
$compareWatch = [System.Diagnostics.Stopwatch]::StartNew()
& $exes['compare'] --inputs $manifest --output $compareDir --sigmas $Sigmas --relative $RelativeTolerance --floor $AbsoluteFloor |
  Tee-Object -FilePath (Join-Path $OutDir 'compare.stdout.txt')
$compareExit = $LASTEXITCODE
$compareSeconds = [math]::Round($compareWatch.Elapsed.TotalSeconds, 2)

# ---- Controls (the exit status stays that of the check above, except a failed negative control) ----
# Margin: the same data with the statistical term only (no relative term, no floor).
$marginDir = Join-Path $OutDir 'compare-statistical-only'
$marginOut = & $exes['compare'] --inputs $manifest --output $marginDir --sigmas $Sigmas --relative 0 --floor 0
$marginExit = $LASTEXITCODE
$marginLine = ($marginOut | Select-String -Pattern '^SEPARATED_CHECK_(PASS|FAIL)' | Select-Object -Last 1).Line -replace '^SEPARATED_CHECK_', 'result '
Write-Host "Margin (statistical term only, $Sigmas sigma): $marginLine"
# Negative controls: leave one source out of the expectation, first the last one (cells after
# its activation), then, with three or more sources, the middle one (cells between its
# activation and the next). A shared history holding every track cannot match the sum without
# one of them, so each control must report SEPARATED_CHECK_FAIL; if one passes, the check has
# no power there. Exit 1 alone is not enough: compare also exits 1 on an output error.
function Invoke-NegativeControl([int]$Dropped, [string]$Label) {
  $activation = $activationText[$Dropped]
  $controlManifest = Join-Path $OutDir "inputs/species-files-negative-control-$Label.txt"
  Set-Content -LiteralPath $controlManifest -Value @($manifestLines | Where-Object { -not $_.StartsWith("single $activation ") })
  $controlDir = Join-Path $OutDir "compare-negative-control-$Label"
  $out = & $exes['compare'] --inputs $controlManifest --output $controlDir --sigmas $Sigmas --relative $RelativeTolerance --floor $AbsoluteFloor
  $exitCode = $LASTEXITCODE
  $line = ($out | Select-String -Pattern '^SEPARATED_CHECK_(PASS|FAIL)' | Select-Object -Last 1)
  $text = if ($line) { $line.Line } else { '' }
  $detected = $exitCode -eq 1 -and $text.StartsWith('SEPARATED_CHECK_FAIL ')
  Write-Host "Negative control (source $Dropped, t = $activation ns, left out of the expectation): $($text -replace '^SEPARATED_CHECK_', 'result ')"
  return [pscustomobject]@{ source = $Dropped; label = $Label; exitCode = $exitCode; detected = $detected; line = $text }
}
$controls = @(Invoke-NegativeControl ($rows.Count - 1) 'last')
if ($rows.Count -ge 3) { $controls += Invoke-NegativeControl ($rows.Count - 2) 'middle' }
$controlOk = -not ($controls | Where-Object { -not $_.detected })
$controlExit = ($controls | ForEach-Object { $_.exitCode }) -join ','
Write-Host ($controlOk ? 'NEGATIVE_CONTROL_OK the check fails when a source is missing from the expectation' :
    'NEGATIVE_CONTROL_FAILED the check did not detect a missing source')


# ---- 4. Record ----------------------------------------------------------------
$runRecords | ForEach-Object {
  [pscustomobject]@{ name = $_.name; kind = $_.kind; source = $_.source; seed = $_.seed; exitCode = $_.exitCode
    timedOut = $_.timedOut; seconds = $_.seconds.ToString('0.00', [cultureinfo]::InvariantCulture); ok = $_.ok }
} | Export-Csv -LiteralPath (Join-Path $OutDir 'runs.csv') -NoTypeInformation
$historyRuns = @($runRecords | Where-Object { $_.kind -ne 'capture' })
$info = [ordered]@{
  format = 'ptirt-separated-check-run'
  buildDir = $BuildDir
  geant4BinDir = $Geant4BinDir
  scheduleFile = $ScheduleFile
  scheduleSha256 = (Get-FileHash -LiteralPath $ScheduleFile -Algorithm SHA256).Hash
  scheduleRows = $rows.Count
  activationsNs = $activationText
  captureMacro = $CaptureMacro
  libraryDir = $LibraryDir
  libraryCaptured = -not $PSBoundParameters.ContainsKey('LibraryDir')
  packetCount = [int]$packetCount
  captureFirstSeed = $CaptureFirstSeed
  endpointNs = $EndpointNs
  seedsPerEnsemble = $Seeds
  combinedSeeds = @($jobs | Where-Object { $_.Kind -eq 'deterministic' } | ForEach-Object { $_.Seed })
  singleSeedsBySource = @(for ($k = 0; $k -lt $rows.Count; $k++) {
      , @($jobs | Where-Object { $_.Source -eq $k } | ForEach-Object { $_.Seed }) })
  tolerance = [ordered]@{ sigmas = $Sigmas; relative = $RelativeTolerance; absoluteFloorMolecules = $AbsoluteFloor }
  resourceCaps = [ordered]@{ maxParallel = $MaxParallel; timeoutMinutesPerRun = $TimeoutMinutes }
  runtimes = [ordered]@{
    captureSeconds = $captureSeconds
    historiesWallSeconds = $historySeconds
    historyRunCount = $historyRuns.Count
    historyRunSecondsSum = [math]::Round(($historyRuns | Measure-Object seconds -Sum).Sum, 1)
    historyRunSecondsMax = ($historyRuns | Measure-Object seconds -Maximum).Maximum
    compareSeconds = $compareSeconds
    totalWallSeconds = [math]::Round($clock.Elapsed.TotalSeconds, 1) }
  compareExitCode = $compareExit
  controls = [ordered]@{
    statisticalTermOnlyExitCode = $marginExit
    negativeControlExitCode = $controlExit
    negativeControlDetected = $controlOk
    negativeControls = @($controls) }
  machine = [ordered]@{ logicalProcessors = [Environment]::ProcessorCount; os = [Environment]::OSVersion.VersionString }
}
$info | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'run-info.json')
Write-Host ("Runtimes: histories {0} s wall ({1} runs, sum {2} s, max {3} s), total {4} s; results in {5}" -f
  $historySeconds, $historyRuns.Count, $info.runtimes.historyRunSecondsSum, $info.runtimes.historyRunSecondsMax,
  $info.runtimes.totalWallSeconds, $OutDir)
if ($compareExit -eq 0 -and -not $controlOk) { exit 1 }
exit $compareExit
