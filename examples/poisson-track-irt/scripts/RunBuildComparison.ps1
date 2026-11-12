<#
.SYNOPSIS
  Stock-vs-patched comparison: the same physical packets and seeds in both builds.

.DESCRIPTION
  For each build (stock, then patched) this script
    1. captures Clusters packet libraries of PacketsPerCluster packets (cluster k: first physics
       seed FirstCaptureSeed + CaptureSeedStep * k),
    2. asserts that the chemistryStartDigest (and the physics seed) of every packet is equal in the
       two builds, and stops when it is not (the comparison would not be of the same packets),
    3. runs Seeds deterministic histories per cluster (deterministic schedule, chemistry seed
       FirstChemistrySeed + ChemistrySeedStep * k + s, the same in both builds) to EndpointNs,
    4. runs compare-builds (clustered paired comparison, see include/BuildComparison.hh),
    5. summarises the verdict counts, the bias flags and the new-old contact totals of each build
       (from the ledgers).
  The verdicts are reported, not enforced: the exit status is 0 when everything ran and the digests
  matched, whatever the verdicts. A failed or timed-out run, or a digest mismatch, stops the script.

  The Geant4 DLL directory of each build comes from Geant4_DIR in its CMakeCache.txt. The builds run
  one after the other (their DLLs differ), each with at most -MaxParallel processes at once, each
  killed after -TimeoutMinutes.

.PARAMETER StockBuildDir
  Build directory configured with PTIRT_PATCHED_GEANT4=OFF.
.PARAMETER PatchedBuildDir
  Build directory configured with PTIRT_PATCHED_GEANT4=ON.
.PARAMETER OutDir
  Result directory; must not exist or must be empty.

.EXAMPLE
  pwsh examples/poisson-track-irt/scripts/RunBuildComparison.ps1 -StockBuildDir <stock> -PatchedBuildDir <patched> -OutDir <scratch>/comparison
#>
#Requires -Version 7
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$StockBuildDir,
  [Parameter(Mandatory)][string]$PatchedBuildDir,
  [Parameter(Mandatory)][string]$OutDir,
  [string]$ScheduleFile,
  [string]$CaptureMacro,
  [ValidateRange(2, 31)][int]$Clusters = 8,
  [ValidateRange(1, 1000)][int]$PacketsPerCluster = 3,
  [ValidateRange(1, 1073741823)][int]$FirstCaptureSeed = 2001,
  [ValidateRange(1, 1000000)][int]$CaptureSeedStep = 100,
  [ValidateRange(1, 1000)][int]$Seeds = 16,
  [ValidateRange(1, 2000000000)][int]$FirstChemistrySeed = 10000,
  [ValidateRange(1, 1000000)][int]$ChemistrySeedStep = 100,
  [double]$EndpointNs = 3e6,
  [ValidateRange(1, 64)][int]$MaxParallel = 8,
  [ValidateRange(1, 1440)][int]$TimeoutMinutes = 30
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'ValidationCommon.ps1')

$exampleDir = Split-Path -Parent $PSScriptRoot
if (-not $ScheduleFile) { $ScheduleFile = Join-Path $exampleDir 'config/deterministic-schedule.txt' }
if (-not $CaptureMacro) { $CaptureMacro = Join-Path $exampleDir 'macros/capture.mac' }

# ---- Inputs ------------------------------------------------------------------
foreach ($file in $ScheduleFile, $CaptureMacro) {
  if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { Stop-Check "input file not found: $file" }
}
$ScheduleFile = (Resolve-Path -LiteralPath $ScheduleFile).Path
$CaptureMacro = (Resolve-Path -LiteralPath $CaptureMacro).Path
if ($CaptureSeedStep -lt $PacketsPerCluster) { Stop-Check 'CaptureSeedStep must be at least PacketsPerCluster (clusters would share physics seeds)' }
if ($ChemistrySeedStep -lt $Seeds) { Stop-Check 'ChemistrySeedStep must be at least Seeds (clusters would share chemistry seeds)' }
if ($FirstCaptureSeed + $CaptureSeedStep * ($Clusters - 1) + $PacketsPerCluster - 1 -gt 1073741823) {
  Stop-Check 'a capture seed exceeds 1073741823 (stock build ceiling)'
}
if ($FirstChemistrySeed + $ChemistrySeedStep * ($Clusters - 1) + $Seeds - 1 -gt 2147483647) { Stop-Check 'a chemistry seed exceeds 2147483647' }
if (-not ($EndpointNs -gt 0)) { Stop-Check 'EndpointNs must be positive' }
$OutDir = [IO.Path]::GetFullPath($OutDir)
if ((Test-Path -LiteralPath $OutDir) -and (Get-ChildItem -LiteralPath $OutDir -Force | Select-Object -First 1)) {
  Stop-Check "OutDir must be empty (refusing to mix runs): $OutDir"
}

# Schedule rows: time_ns packet_index dx dy dz. The schedule must use exactly the packets of a cluster.
$rows = @(Get-Content -LiteralPath $ScheduleFile | ForEach-Object { $_.Trim() } |
  Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object { , ($_ -split '\s+') })
if ($rows.Count -lt 1) { Stop-Check "the schedule has no row: $ScheduleFile" }
$maxPacket = ($rows | ForEach-Object { [int]$_[1] } | Measure-Object -Maximum).Maximum
if ($maxPacket -ge $PacketsPerCluster) { Stop-Check "the schedule uses packet $maxPacket but a cluster holds $PacketsPerCluster packets" }

$builds = @(
  [pscustomobject]@{ name = 'stock'; dir = (Resolve-Path -LiteralPath $StockBuildDir).Path; expectMode = 'stock' }
  [pscustomobject]@{ name = 'patched'; dir = (Resolve-Path -LiteralPath $PatchedBuildDir).Path; expectMode = 'patched' }
)
foreach ($build in $builds) {
  $mode = Get-BuildModeOfCache $build.dir
  if ($mode -ne $build.expectMode) { Stop-Check "$($build.dir) is configured as '$mode', expected '$($build.expectMode)'" }
  $build | Add-Member -NotePropertyName exes -NotePropertyValue (Get-BuildExes $build.dir @('capture', 'capture-check', 'history', 'compare-builds'))
  $build | Add-Member -NotePropertyName binDir -NotePropertyValue (Get-Geant4BinDir $build.dir)
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$inputsDir = Join-Path $OutDir 'inputs'
New-Item -ItemType Directory -Force -Path $inputsDir | Out-Null
Copy-Item -LiteralPath $ScheduleFile -Destination (Join-Path $inputsDir 'schedule.txt')
$scheduleCopy = Join-Path $inputsDir 'schedule.txt'
foreach ($build in $builds) {
  foreach ($sub in 'libraries', 'histories', 'configs', 'logs') {
    New-Item -ItemType Directory -Force -Path (Join-Path $OutDir "$($build.name)/$sub") | Out-Null
  }
}
$basePath = $env:PATH
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$runRecords = [System.Collections.Generic.List[object]]::new()
$clusterNames = @(0..($Clusters - 1) | ForEach-Object { 'c{0:D2}' -f $_ })

# ---- 1. Capture the libraries of both builds ----------------------------------
foreach ($build in $builds) {
  Set-BuildPath $build.binDir $basePath
  $jobs = for ($k = 0; $k -lt $Clusters; $k++) {
    $name = $clusterNames[$k]
    $logs = Join-Path $OutDir "$($build.name)/logs"
    @{ Name = "capture-$name"; Kind = 'capture'; Build = $build.name; Exe = $build.exes['capture']
       Arguments = @('--macro', $CaptureMacro, '--output', (Join-Path $OutDir "$($build.name)/libraries/$name"),
                     '--count', $PacketsPerCluster, '--first-seed', ($FirstCaptureSeed + $CaptureSeedStep * $k))
       StdOut = Join-Path $logs "capture-$name.out.txt"; StdErr = Join-Path $logs "capture-$name.err.txt"
       DoneMarker = 'CAPTURE_DONE' }
  }
  Write-Host "[$($build.name)] capturing $Clusters libraries of $PacketsPerCluster packets (first seeds $FirstCaptureSeed + $CaptureSeedStep k)"
  foreach ($record in (Invoke-Pool @($jobs) $MaxParallel $TimeoutMinutes)) { $runRecords.Add($record) }
  foreach ($name in $clusterNames) {
    $check = & $build.exes['capture-check'] (Join-Path $OutDir "$($build.name)/libraries/$name") --expect-count $PacketsPerCluster
    if ($LASTEXITCODE -ne 0) { Stop-Check "capture-check failed on $($build.name)/$name : $($check | Select-Object -Last 1)" 1 }
  }
}

# ---- 2. Digest equality (abort on any difference) ------------------------------
$digestRows = @()
foreach ($name in $clusterNames) {
  $digestRows += Assert-DigestsEqual $name (Join-Path $OutDir "stock/libraries/$name") (Join-Path $OutDir "patched/libraries/$name") $PacketsPerCluster
}
$digestRows | Export-Csv -LiteralPath (Join-Path $OutDir 'digests.csv') -NoTypeInformation
Write-Host "DIGEST_EQUAL $($digestRows.Count) packets ($Clusters clusters x $PacketsPerCluster): chemistryStartDigest and physics seed match in both builds"

# ---- 3. Histories --------------------------------------------------------------
$historyWatch = [System.Diagnostics.Stopwatch]::StartNew()
foreach ($build in $builds) {
  Set-BuildPath $build.binDir $basePath
  $jobs = @()
  for ($k = 0; $k -lt $Clusters; $k++) {
    for ($s = 0; $s -lt $Seeds; $s++) {
      $name = '{0}-s{1:D2}' -f $clusterNames[$k], $s
      $seed = $FirstChemistrySeed + $ChemistrySeedStep * $k + $s
      $config = Join-Path $OutDir "$($build.name)/configs/$name.cfg"
      Set-Content -LiteralPath $config -Value @(
        'mode = deterministic'
        "libraryDir = $(Format-Path (Join-Path $OutDir "$($build.name)/libraries/$($clusterNames[$k])"))"
        "scheduleFile = $(Format-Path $scheduleCopy)"
        "endpointNs = $($EndpointNs.ToString('R', [cultureinfo]::InvariantCulture))"
        "chemistrySeed = $seed")
      $logs = Join-Path $OutDir "$($build.name)/logs"
      $jobs += @{ Name = $name; Kind = 'history'; Build = $build.name; Exe = $build.exes['history']
        Arguments = @('--config', $config, '--output', (Join-Path $OutDir "$($build.name)/histories/$name"))
        StdOut = Join-Path $logs "$name.out.txt"; StdErr = Join-Path $logs "$name.err.txt"; DoneMarker = 'HISTORY_DONE' }
    }
  }
  Write-Host "[$($build.name)] running $($jobs.Count) histories ($Clusters clusters x $Seeds seeds), at most $MaxParallel at once, endpoint $EndpointNs ns"
  foreach ($record in (Invoke-Pool $jobs $MaxParallel $TimeoutMinutes)) { $runRecords.Add($record) }
}
$env:PATH = $basePath
$historySeconds = [math]::Round($historyWatch.Elapsed.TotalSeconds, 1)
Write-Host "All histories finished in $historySeconds s"

# ---- 4. compare-builds -----------------------------------------------------------
$csv = Join-Path $OutDir 'comparison.csv'
$compare = Invoke-CompareBuilds $builds[0].exes['compare-builds'] (Join-Path $OutDir 'stock/histories') (Join-Path $OutDir 'patched/histories') $csv
$compare.lines | Set-Content -LiteralPath (Join-Path $OutDir 'compare.stdout.txt')
$compare.lines | ForEach-Object { Write-Host $_ }
if ($compare.exitCode -ne 0) { Stop-Check "compare-builds exited with $($compare.exitCode)" 1 }

# ---- 5. Contacts and summary -----------------------------------------------------
$contactRows = [System.Collections.Generic.List[object]]::new()
$perPairTotals = @{ stock = @{}; patched = @{} }
$activationTotals = @{ stock = @{}; patched = @{} }
foreach ($build in $builds) {
  for ($k = 0; $k -lt $Clusters; $k++) {
    for ($s = 0; $s -lt $Seeds; $s++) {
      $name = '{0}-s{1:D2}' -f $clusterNames[$k], $s
      $ledger = Read-LedgerContacts (Join-Path $OutDir "$($build.name)/histories/$name/ledger.json")
      if ($ledger.buildMode -ne $build.name) { Stop-Check "$($build.name)/$name ledger says buildMode '$($ledger.buildMode)'" 1 }
      if (-not $ledger.allPassed) { Stop-Check "$($build.name)/$name ledger has checks.allPassed false" 1 }
      $contactRows.Add([pscustomobject]@{ build = $build.name; cluster = $k; seed = $s; pairs = $ledger.pairs
          newMoleculesInContact = $ledger.newMoleculesInContact
          startZeroStepReactions = $ledger.startZeroStepReactions })
      foreach ($key in $ledger.perPair.Keys) {
        $perPairTotals[$build.name][$key] = ($perPairTotals[$build.name][$key] ?? 0) + $ledger.perPair[$key]
      }
      for ($a = 0; $a -lt $ledger.activationPairs.Count; $a++) {
        $activationTotals[$build.name][$a] = ($activationTotals[$build.name][$a] ?? 0) + $ledger.activationPairs[$a]
      }
    }
  }
}
$contactRows | Export-Csv -LiteralPath (Join-Path $OutDir 'contacts.csv') -NoTypeInformation

$comparison = @(Import-Csv -LiteralPath $csv)
$invariant = [cultureinfo]::InvariantCulture
$contactSummary = [ordered]@{}
foreach ($build in $builds) {
  $mine = @($contactRows | Where-Object { $_.build -eq $build.name })
  $stats = Get-MeanSe ([double[]]@($mine | ForEach-Object { [double]$_.pairs }))
  $contactSummary[$build.name] = [ordered]@{
    histories = $stats.n
    pairsTotal = [int]$stats.sum
    pairsMeanPerHistory = $stats.mean
    pairsSePerHistory = $stats.se
    newMoleculesInContactTotal = [int](($mine | Measure-Object newMoleculesInContact -Sum).Sum)
    historiesWithAnyContact = @($mine | Where-Object { $_.pairs -gt 0 }).Count
    pairsByActivationIndex = [ordered]@{ }
    pairsByReactionPair = $perPairTotals[$build.name]
  }
}
foreach ($build in $builds) {
  foreach ($a in ($activationTotals[$build.name].Keys | Sort-Object)) {
    $contactSummary[$build.name].pairsByActivationIndex["$a"] = $activationTotals[$build.name][$a]
  }
}
$stockStartZero = @($contactRows | Where-Object { $_.build -eq 'stock' } | ForEach-Object { $_.startZeroStepReactions })

$differentRows = @($comparison | Where-Object { $_.verdict -eq 'DIFFERENT' })
$biasRows = @($comparison | Where-Object { $_.biasFlag -eq '1' })
$bySpecies = @($comparison | Where-Object { $_.verdict -ne 'EMPTY' } | Group-Object species | ForEach-Object {
    [ordered]@{ species = $_.Name; rows = $_.Count
      different = @($_.Group | Where-Object { $_.verdict -eq 'DIFFERENT' }).Count
      inconclusive = @($_.Group | Where-Object { $_.verdict -eq 'INCONCLUSIVE' }).Count
      biasFlags = @($_.Group | Where-Object { $_.biasFlag -eq '1' }).Count } })
$worst = @($comparison | Where-Object { $_.verdict -ne 'EMPTY' } | ForEach-Object {
    [pscustomobject]@{ timeNs = [double]$_.timeNs; species = $_.species; verdict = $_.verdict; meanDiff = [double]$_.meanDiff
      ciLow = [double]$_.ciLow; ciHigh = [double]$_.ciHigh; margin = [double]$_.margin; patchedMean = [double]$_.patchedMean
      ratio = [math]::Max([math]::Abs([double]$_.ciLow), [math]::Abs([double]$_.ciHigh)) / [double]$_.margin }
  } | Sort-Object ratio -Descending | Select-Object -First 10)

$summary = [ordered]@{
  verdicts = [ordered]@{ EQUIVALENT = $compare.counts.EQUIVALENT; DIFFERENT = $compare.counts.DIFFERENT
    INCONCLUSIVE = $compare.counts.INCONCLUSIVE; EMPTY = $compare.counts.EMPTY; rows = $comparison.Count }
  biasFlags = $compare.counts.biasFlags
  biasFlagsOnEquivalentRows = $compare.counts.biasFlagsEquivalent
  digestsEqual = $true
  digestPackets = $digestRows.Count
  contacts = $contactSummary
  stockStartZeroStepReactionsTotal = [int](($stockStartZero | Measure-Object -Sum).Sum)
  perSpecies = $bySpecies
  worstCellsByCiOverMargin = $worst
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'summary.json')

$text = [System.Collections.Generic.List[string]]::new()
$text.Add("Stock vs patched comparison: $Clusters clusters x $PacketsPerCluster packets, $Seeds seeds per cluster, endpoint $EndpointNs ns")
$text.Add("DIGEST_EQUAL $($digestRows.Count) packets")
$text.Add("Rows (record time x species): $($comparison.Count)")
$text.Add("Verdicts: EQUIVALENT $($compare.counts.EQUIVALENT), DIFFERENT $($compare.counts.DIFFERENT), INCONCLUSIVE $($compare.counts.INCONCLUSIVE), EMPTY $($compare.counts.EMPTY)")
$text.Add("Bias flags (CI excludes 0): $($compare.counts.biasFlags), of which $($compare.counts.biasFlagsEquivalent) on EQUIVALENT rows")
foreach ($build in $builds) {
  $c = $contactSummary[$build.name]
  $text.Add(("New-old contact pairs, {0}: total {1} in {2} histories ({3} with any), per history {4} +- {5} (SE)" -f
      $build.name, $c.pairsTotal, $c.histories, $c.historiesWithAnyContact, (Format-Number $c.pairsMeanPerHistory), (Format-Number $c.pairsSePerHistory)))
}
$text.Add("Stock startZeroStepReactions total: $($summary.stockStartZeroStepReactionsTotal)")
$text.Add('Worst cells (max |CI end| / margin):')
foreach ($w in $worst) {
  $text.Add(("  t={0} ns {1} {2} meanDiff {3} CI [{4}, {5}] margin {6} patchedMean {7}" -f
      (Format-Number $w.timeNs "G10"), $w.species, $w.verdict, (Format-Number $w.meanDiff), (Format-Number $w.ciLow), (Format-Number $w.ciHigh),
      (Format-Number $w.margin), (Format-Number $w.patchedMean)))
}
$text | Set-Content -LiteralPath (Join-Path $OutDir 'summary.txt')
$text | ForEach-Object { Write-Host $_ }

# ---- 6. Record -----------------------------------------------------------------
$runRecords | Export-Csv -LiteralPath (Join-Path $OutDir 'runs.csv') -NoTypeInformation
$historyRuns = @($runRecords | Where-Object { $_.kind -eq 'history' })
$info = [ordered]@{
  format = 'ptirt-build-comparison-run'
  stockBuildDir = $builds[0].dir; patchedBuildDir = $builds[1].dir
  geant4BinDirs = [ordered]@{ stock = $builds[0].binDir; patched = $builds[1].binDir }
  scheduleFile = $ScheduleFile
  scheduleSha256 = (Get-FileHash -LiteralPath $ScheduleFile -Algorithm SHA256).Hash
  captureMacro = $CaptureMacro
  clusters = $Clusters; packetsPerCluster = $PacketsPerCluster
  captureFirstSeeds = @(0..($Clusters - 1) | ForEach-Object { $FirstCaptureSeed + $CaptureSeedStep * $_ })
  seedsPerCluster = $Seeds
  chemistrySeedRule = "$FirstChemistrySeed + $ChemistrySeedStep * cluster + seed"
  endpointNs = $EndpointNs
  margin = 'max(0.05 * patchedMean, 1 molecule); t(0.975, clusters-1) over cluster means (include/BuildComparison.hh)'
  resourceCaps = [ordered]@{ maxParallel = $MaxParallel; timeoutMinutesPerRun = $TimeoutMinutes }
  runtimes = [ordered]@{
    historiesWallSeconds = $historySeconds
    historyRunCount = $historyRuns.Count
    historyRunSecondsSum = [math]::Round(($historyRuns | Measure-Object seconds -Sum).Sum, 1)
    historyRunSecondsMax = ($historyRuns | Measure-Object seconds -Maximum).Maximum
    totalWallSeconds = [math]::Round($clock.Elapsed.TotalSeconds, 1) }
  machine = [ordered]@{ logicalProcessors = [Environment]::ProcessorCount; os = [Environment]::OSVersion.VersionString }
}
$info | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'run-info.json')
Write-Host ("Runtimes: histories {0} s wall ({1} runs, sum {2} s, max {3} s), total {4} s; results in {5}" -f
  $historySeconds, $historyRuns.Count, $info.runtimes.historyRunSecondsSum, $info.runtimes.historyRunSecondsMax,
  $info.runtimes.totalWallSeconds, $OutDir)
exit 0
