<#
.SYNOPSIS
  Overlap sweep: two sources that start close together, stock vs patched.

.DESCRIPTION
  The stock stepper gives a fresh source no contact trial against the molecules already in the
  history, so two tracks that overlap can miss the reactions between their molecules at the second
  activation. This sweep measures that. For each separation d (um):
    schedule  packet 0 at t = 0 at the origin, packet 1 at SecondArrivalNs offset d along x,
    clusters  Clusters libraries of 2 packets (cluster k: first physics seed FirstCaptureSeed +
              CaptureSeedStep * k), the same in every d,
    seeds     Seeds histories per cluster, chemistry seed
              FirstChemistrySeed + SeedPerSeparation * (separation index) + ChemistrySeedStep * k + s,
              the same in both builds, endpoint EndpointNs,
  in both builds. It asserts the chemistry-start digests of the packets match between the builds
  (and stops if not), runs compare-builds per d, and reports per d the new-old contact pairs of
  each build (mean +- SE per history) and the verdict counts. The verdicts and contact counts are
  reported as seen, including a null result; nothing is enforced.

  The Geant4 DLL directory of each build comes from Geant4_DIR in its CMakeCache.txt; the builds
  run one after the other.

.PARAMETER StockBuildDir
  Build directory configured with PTIRT_PATCHED_GEANT4=OFF.
.PARAMETER PatchedBuildDir
  Build directory configured with PTIRT_PATCHED_GEANT4=ON.
.PARAMETER OutDir
  Result directory; must not exist or must be empty.
.PARAMETER SeparationsUm
  Separations d, um. Default 0.005 0.05 0.5 5 100.

.EXAMPLE
  pwsh examples/poisson-track-irt/scripts/RunOverlapSweep.ps1 -StockBuildDir <stock> -PatchedBuildDir <patched> -OutDir <scratch>/overlap
#>
#Requires -Version 7
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$StockBuildDir,
  [Parameter(Mandatory)][string]$PatchedBuildDir,
  [Parameter(Mandatory)][string]$OutDir,
  [string]$CaptureMacro,
  [double[]]$SeparationsUm = @(0.005, 0.05, 0.5, 5, 100),
  [double]$SecondArrivalNs = 10,
  [ValidateRange(2, 31)][int]$Clusters = 8,
  [ValidateRange(1, 1073741823)][int]$FirstCaptureSeed = 3001,
  [ValidateRange(2, 1000000)][int]$CaptureSeedStep = 100,
  [ValidateRange(1, 100)][int]$Seeds = 8,
  [ValidateRange(1, 2000000000)][int]$FirstChemistrySeed = 20000,
  [ValidateRange(1, 1000000)][int]$ChemistrySeedStep = 100,
  [ValidateRange(1, 10000000)][int]$SeedPerSeparation = 10000,
  [double]$EndpointNs = 1000,
  [ValidateRange(1, 64)][int]$MaxParallel = 12,
  [ValidateRange(1, 1440)][int]$TimeoutMinutes = 30
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'ValidationCommon.ps1')

$exampleDir = Split-Path -Parent $PSScriptRoot
if (-not $CaptureMacro) { $CaptureMacro = Join-Path $exampleDir 'macros/capture.mac' }
$packetsPerCluster = 2

# ---- Inputs ------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $CaptureMacro -PathType Leaf)) { Stop-Check "input file not found: $CaptureMacro" }
$CaptureMacro = (Resolve-Path -LiteralPath $CaptureMacro).Path
if ($SeparationsUm.Count -lt 1 -or @($SeparationsUm | Where-Object { -not ($_ -gt 0) }).Count) { Stop-Check 'SeparationsUm must be positive numbers' }
if ($ChemistrySeedStep -lt $Seeds) { Stop-Check 'ChemistrySeedStep must be at least Seeds' }
if ($SeedPerSeparation -lt $ChemistrySeedStep * $Clusters) { Stop-Check 'SeedPerSeparation must be at least ChemistrySeedStep * Clusters (separations would share seeds)' }
if ($FirstCaptureSeed + $CaptureSeedStep * ($Clusters - 1) + $packetsPerCluster - 1 -gt 1073741823) { Stop-Check 'a capture seed exceeds 1073741823 (stock build ceiling)' }
if ($FirstChemistrySeed + $SeedPerSeparation * $SeparationsUm.Count -gt 2147483647) { Stop-Check 'a chemistry seed exceeds 2147483647' }
if (-not ($SecondArrivalNs -gt 0 -and $SecondArrivalNs -lt $EndpointNs)) { Stop-Check 'SecondArrivalNs must be positive and before EndpointNs' }
$OutDir = [IO.Path]::GetFullPath($OutDir)
if ((Test-Path -LiteralPath $OutDir) -and (Get-ChildItem -LiteralPath $OutDir -Force | Select-Object -First 1)) {
  Stop-Check "OutDir must be empty (refusing to mix runs): $OutDir"
}
$inv = [cultureinfo]::InvariantCulture
function Format-Round([double]$Value) { return $Value.ToString('R', $inv) }

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
# One schedule per separation (time_ns packet_index dx_um dy_um dz_um).
$separations = for ($i = 0; $i -lt $SeparationsUm.Count; $i++) {
  $file = Join-Path $inputsDir ('schedule-d{0}.txt' -f $i)
  Set-Content -LiteralPath $file -Value @(
    "# d = $(Format-Round $SeparationsUm[$i]) um: packet 1 arrives $(Format-Round $SecondArrivalNs) ns after packet 0, offset d along x"
    '# time_ns  packet_index  dx_um  dy_um  dz_um'
    '0 0 0 0 0'
    "$(Format-Round $SecondArrivalNs) 1 $(Format-Round $SeparationsUm[$i]) 0 0")
  [pscustomobject]@{ index = $i; d = $SeparationsUm[$i]; label = 'd{0}' -f $i; schedule = $file }
}
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
  $logs = Join-Path $OutDir "$($build.name)/logs"
  $jobs = for ($k = 0; $k -lt $Clusters; $k++) {
    $name = $clusterNames[$k]
    @{ Name = "capture-$name"; Kind = 'capture'; Build = $build.name; Exe = $build.exes['capture']
       Arguments = @('--macro', $CaptureMacro, '--output', (Join-Path $OutDir "$($build.name)/libraries/$name"),
                     '--count', $packetsPerCluster, '--first-seed', ($FirstCaptureSeed + $CaptureSeedStep * $k))
       StdOut = Join-Path $logs "capture-$name.out.txt"; StdErr = Join-Path $logs "capture-$name.err.txt"
       DoneMarker = 'CAPTURE_DONE' }
  }
  Write-Host "[$($build.name)] capturing $Clusters libraries of $packetsPerCluster packets (first seeds $FirstCaptureSeed + $CaptureSeedStep k)"
  foreach ($record in (Invoke-Pool @($jobs) $MaxParallel $TimeoutMinutes)) { $runRecords.Add($record) }
  foreach ($name in $clusterNames) {
    $check = & $build.exes['capture-check'] (Join-Path $OutDir "$($build.name)/libraries/$name") --expect-count $packetsPerCluster
    if ($LASTEXITCODE -ne 0) { Stop-Check "capture-check failed on $($build.name)/$name : $($check | Select-Object -Last 1)" 1 }
  }
}

# ---- 2. Digest equality (abort on any difference) ------------------------------
$digestRows = @()
foreach ($name in $clusterNames) {
  $digestRows += Assert-DigestsEqual $name (Join-Path $OutDir "stock/libraries/$name") (Join-Path $OutDir "patched/libraries/$name") $packetsPerCluster
}
$digestRows | Export-Csv -LiteralPath (Join-Path $OutDir 'digests.csv') -NoTypeInformation
Write-Host "DIGEST_EQUAL $($digestRows.Count) packets ($Clusters clusters x $packetsPerCluster): chemistryStartDigest and physics seed match in both builds"

# ---- 3. Histories ----------------------------------------------------------------
$historyWatch = [System.Diagnostics.Stopwatch]::StartNew()
foreach ($build in $builds) {
  Set-BuildPath $build.binDir $basePath
  $logs = Join-Path $OutDir "$($build.name)/logs"
  $jobs = @()
  foreach ($sep in $separations) {
    $histDir = Join-Path $OutDir "$($build.name)/histories/$($sep.label)"
    for ($k = 0; $k -lt $Clusters; $k++) {
      for ($s = 0; $s -lt $Seeds; $s++) {
        $name = '{0}-s{1:D2}' -f $clusterNames[$k], $s
        $seed = $FirstChemistrySeed + $SeedPerSeparation * $sep.index + $ChemistrySeedStep * $k + $s
        $config = Join-Path $OutDir "$($build.name)/configs/$($sep.label)-$name.cfg"
        Set-Content -LiteralPath $config -Value @(
          'mode = deterministic'
          "libraryDir = $(Format-Path (Join-Path $OutDir "$($build.name)/libraries/$($clusterNames[$k])"))"
          "scheduleFile = $(Format-Path $sep.schedule)"
          "endpointNs = $(Format-Round $EndpointNs)"
          "chemistrySeed = $seed")
        $jobs += @{ Name = "$($sep.label)-$name"; Kind = 'history'; Build = $build.name; Exe = $build.exes['history']
          Arguments = @('--config', $config, '--output', (Join-Path $histDir $name))
          StdOut = Join-Path $logs "$($sep.label)-$name.out.txt"; StdErr = Join-Path $logs "$($sep.label)-$name.err.txt"
          DoneMarker = 'HISTORY_DONE' }
      }
    }
  }
  Write-Host "[$($build.name)] running $($jobs.Count) histories ($($separations.Count) separations x $Clusters clusters x $Seeds seeds), at most $MaxParallel at once, endpoint $EndpointNs ns"
  foreach ($record in (Invoke-Pool $jobs $MaxParallel $TimeoutMinutes)) { $runRecords.Add($record) }
}
$env:PATH = $basePath
$historySeconds = [math]::Round($historyWatch.Elapsed.TotalSeconds, 1)
Write-Host "All histories finished in $historySeconds s"

# ---- 4. Per separation: compare-builds and contacts ------------------------------
$contactRows = [System.Collections.Generic.List[object]]::new()
$perSeparation = @()
$text = [System.Collections.Generic.List[string]]::new()
$text.Add("Overlap sweep: packet 0 at 0 ns, packet 1 at $(Format-Round $SecondArrivalNs) ns offset d along x; $Clusters clusters x $Seeds seeds per build; endpoint $(Format-Round $EndpointNs) ns")
$text.Add("DIGEST_EQUAL $($digestRows.Count) packets")
foreach ($sep in $separations) {
  $csv = Join-Path $OutDir "comparison-$($sep.label).csv"
  $compare = Invoke-CompareBuilds $builds[0].exes['compare-builds'] (Join-Path $OutDir "stock/histories/$($sep.label)") (Join-Path $OutDir "patched/histories/$($sep.label)") $csv
  $compare.lines | Set-Content -LiteralPath (Join-Path $OutDir "compare-$($sep.label).stdout.txt")
  if ($compare.exitCode -ne 0) { Stop-Check "compare-builds exited with $($compare.exitCode) for d = $($sep.d) um: $($compare.lines -join ' | ')" 1 }

  $entry = [ordered]@{ index = $sep.index; dUm = $sep.d; verdicts = $compare.counts }
  foreach ($build in $builds) {
    $pairs = [System.Collections.Generic.List[double]]::new()
    $moleculesInContact = 0
    $perPairTotals = @{}
    for ($k = 0; $k -lt $Clusters; $k++) {
      for ($s = 0; $s -lt $Seeds; $s++) {
        $name = '{0}-s{1:D2}' -f $clusterNames[$k], $s
        $ledger = Read-LedgerContacts (Join-Path $OutDir "$($build.name)/histories/$($sep.label)/$name/ledger.json")
        if ($ledger.buildMode -ne $build.name) { Stop-Check "$($build.name)/$($sep.label)/$name ledger says buildMode '$($ledger.buildMode)'" 1 }
        if (-not $ledger.allPassed) { Stop-Check "$($build.name)/$($sep.label)/$name ledger has checks.allPassed false" 1 }
        $pairs.Add($ledger.pairs)
        $moleculesInContact += $ledger.newMoleculesInContact
        foreach ($key in $ledger.perPair.Keys) { $perPairTotals[$key] = ($perPairTotals[$key] ?? 0) + $ledger.perPair[$key] }
        $contactRows.Add([pscustomobject]@{ separation = $sep.index; dUm = $sep.d; build = $build.name; cluster = $k; seed = $s
            pairs = $ledger.pairs; newMoleculesInContact = $ledger.newMoleculesInContact })
      }
    }
    $stats = Get-MeanSe ([double[]]$pairs.ToArray())
    $entry[$build.name] = [ordered]@{
      histories = $stats.n; pairsTotal = [int]$stats.sum; pairsMean = $stats.mean; pairsSe = $stats.se
      historiesWithAnyContact = @($pairs | Where-Object { $_ -gt 0 }).Count
      newMoleculesInContactTotal = $moleculesInContact
      pairsByReactionPair = $perPairTotals }
  }
  $perSeparation += [pscustomobject]$entry
  $c = $compare.counts
  $s0 = $entry['stock']; $p0 = $entry['patched']
  $line = ("d = {0} um: stock contact pairs {1} +- {2} per history (total {3}, {4}/{5} histories with any); patched {6} +- {7} (total {8}); " +
      "verdicts EQUIVALENT {9} DIFFERENT {10} INCONCLUSIVE {11} EMPTY {12}, bias flags {13}") -f
      (Format-Number $sep.d), (Format-Number $s0.pairsMean), (Format-Number $s0.pairsSe), $s0.pairsTotal, $s0.historiesWithAnyContact, $s0.histories,
      (Format-Number $p0.pairsMean), (Format-Number $p0.pairsSe), $p0.pairsTotal,
      $c.EQUIVALENT, $c.DIFFERENT, $c.INCONCLUSIVE, $c.EMPTY, $c.biasFlags
  $text.Add($line)
}
$contactRows | Export-Csv -LiteralPath (Join-Path $OutDir 'contacts.csv') -NoTypeInformation
$perSeparation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutDir 'summary.json')
$text | Set-Content -LiteralPath (Join-Path $OutDir 'summary.txt')
$text | ForEach-Object { Write-Host $_ }

# ---- 5. Record -----------------------------------------------------------------
$runRecords | Export-Csv -LiteralPath (Join-Path $OutDir 'runs.csv') -NoTypeInformation
$historyRuns = @($runRecords | Where-Object { $_.kind -eq 'history' })
$info = [ordered]@{
  format = 'ptirt-overlap-sweep-run'
  stockBuildDir = $builds[0].dir; patchedBuildDir = $builds[1].dir
  geant4BinDirs = [ordered]@{ stock = $builds[0].binDir; patched = $builds[1].binDir }
  captureMacro = $CaptureMacro
  separationsUm = $SeparationsUm
  secondArrivalNs = $SecondArrivalNs
  clusters = $Clusters; packetsPerCluster = $packetsPerCluster
  captureFirstSeeds = @(0..($Clusters - 1) | ForEach-Object { $FirstCaptureSeed + $CaptureSeedStep * $_ })
  seedsPerCluster = $Seeds
  chemistrySeedRule = "$FirstChemistrySeed + $SeedPerSeparation * separationIndex + $ChemistrySeedStep * cluster + seed"
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
