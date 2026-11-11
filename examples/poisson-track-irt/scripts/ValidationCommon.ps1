<#
.SYNOPSIS
  Helpers shared by RunBuildComparison.ps1 and RunOverlapSweep.ps1 (dot-source, do not run).

.DESCRIPTION
  Process pool with a per-run timeout, the Geant4 DLL directory of a build (from Geant4_DIR in
  its CMakeCache.txt, never hardcoded), packet-digest assertions between the stock and the
  patched capture, and the new-old contact figures of a ledger.json.

  Stock and patched histories need different Geant4 DLLs, so the callers run one build per
  phase: Set-BuildPath puts the DLL directory of that build first on PATH, and the child
  processes started afterwards inherit it.
#>

function Stop-Check([string]$Message, [int]$Code = 2) {
  Write-Host "VALIDATION_ERROR $Message"
  exit $Code
}

function Format-Path([string]$Path) { return $Path -replace '\\', '/' }

# Absolute path of the executables of a build directory; stops when one is missing.
function Get-BuildExes([string]$BuildDir, [string[]]$Names) {
  $suffix = $IsWindows ? '.exe' : ''
  $exes = @{}
  foreach ($name in $Names) {
    $path = Join-Path $BuildDir "$name$suffix"
    if (-not (Test-Path -LiteralPath $path)) { Stop-Check "$path not found; build the example first" }
    $exes[$name] = $path
  }
  return $exes
}

# The Geant4 binary directory of a build: Geant4_DIR/../../../bin (as the ctest
# ENVIRONMENT_MODIFICATION does). Stops when Geant4_DIR is missing.
function Get-Geant4BinDir([string]$BuildDir) {
  $cache = Join-Path $BuildDir 'CMakeCache.txt'
  $line = Select-String -LiteralPath $cache -Pattern '^Geant4_DIR:[A-Z]+=(.+)$' | Select-Object -First 1
  if (-not $line) { Stop-Check "Geant4_DIR not found in $cache" }
  $bin = [IO.Path]::GetFullPath((Join-Path $line.Matches[0].Groups[1].Value.Trim() '../../../bin'))
  if (-not (Test-Path -LiteralPath $bin -PathType Container)) { Stop-Check "Geant4 bin directory not found: $bin" }
  return $bin
}

# The build mode a build directory was configured with (PTIRT_PATCHED_GEANT4 in its cache).
function Get-BuildModeOfCache([string]$BuildDir) {
  $line = Select-String -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt') `
    -Pattern '^PTIRT_PATCHED_GEANT4:[A-Z]+=(.+)$' | Select-Object -First 1
  if (-not $line) { Stop-Check "PTIRT_PATCHED_GEANT4 not found in the CMakeCache.txt of $BuildDir" }
  return ($line.Matches[0].Groups[1].Value.Trim() -eq 'ON') ? 'patched' : 'stock'
}

# Geant4 DLLs of the build first on PATH; $BasePath is the PATH the script started with.
function Set-BuildPath([string]$Geant4BinDir, [string]$BasePath) {
  $env:PATH = $Geant4BinDir + [IO.Path]::PathSeparator + $BasePath
}

function Start-PoolProcess([hashtable]$Job) {
  $info = @{ Job = $Job; Watch = [System.Diagnostics.Stopwatch]::StartNew(); TimedOut = $false }
  $quoted = @($Job.Arguments | ForEach-Object { '"{0}"' -f $_ })
  $info.Process = Start-Process -FilePath $Job.Exe -ArgumentList $quoted -PassThru -NoNewWindow `
    -RedirectStandardOutput $Job.StdOut -RedirectStandardError $Job.StdErr
  $null = $info.Process.Handle  # keep the handle so ExitCode stays readable
  return $info
}

# Runs the jobs (hashtables: Name, Kind, Build, Exe, Arguments, StdOut, StdErr, DoneMarker), at most
# $Parallel at once, each killed after $TimeoutMinutes. The first failed or timed-out run stops
# the check: no new run is started, the running ones finish or time out. Returns one record per run.
function Invoke-Pool([object[]]$Jobs, [int]$Parallel, [int]$TimeoutMinutes) {
  $records = [System.Collections.Generic.List[object]]::new()
  $pending = [System.Collections.Generic.Queue[hashtable]]::new()
  foreach ($job in $Jobs) { $pending.Enqueue($job) }
  $running = [System.Collections.Generic.List[hashtable]]::new()
  $failure = $null
  $deadline = [TimeSpan]::FromMinutes($TimeoutMinutes)
  while (($pending.Count -gt 0 -and -not $failure) -or $running.Count -gt 0) {
    while (-not $failure -and $pending.Count -gt 0 -and $running.Count -lt $Parallel) {
      $running.Add((Start-PoolProcess $pending.Dequeue()))
    }
    foreach ($info in @($running)) {
      # Sticky: Kill() does not wait, so the process may only report exited on a later poll.
      if (-not $info.TimedOut -and -not $info.Process.HasExited -and $info.Watch.Elapsed -gt $deadline) {
        $info.TimedOut = $true
        $info.Process.Kill($true)
      }
      if ($info.Process.HasExited) {
        $info.Process.WaitForExit()
        $info.Watch.Stop()
        $job = $info.Job
        $stdout = Get-Content -LiteralPath $job.StdOut -Raw -ErrorAction SilentlyContinue
        $done = $stdout -and $stdout.Contains($job.DoneMarker)
        $ok = (-not $info.TimedOut) -and $info.Process.ExitCode -eq 0 -and $done
        $records.Add([pscustomobject]@{
            build = $job.Build; kind = $job.Kind; name = $job.Name
            exitCode = $info.Process.ExitCode; timedOut = $info.TimedOut
            seconds = [math]::Round($info.Watch.Elapsed.TotalSeconds, 2); ok = $ok })
        if (-not $ok -and -not $failure) {
          $failure = "run $($job.Build)/$($job.Name) failed (exit $($info.Process.ExitCode), timedOut=$($info.TimedOut)); see $($job.StdErr)"
        }
        $running.Remove($info) | Out-Null
      }
    }
    if ($running.Count -gt 0) { Start-Sleep -Milliseconds 500 }
  }
  if ($failure) { Stop-Check $failure 1 }
  return $records
}

# The packets of a capture.json: index, physics seed, chemistry-start digest, molecule count.
function Read-CapturePackets([string]$LibraryDir) {
  $file = Join-Path $LibraryDir 'capture.json'
  if (-not (Test-Path -LiteralPath $file)) { Stop-Check "capture.json not found in $LibraryDir" }
  $json = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
  return @($json.packets | ForEach-Object {
      [pscustomobject]@{ packetIndex = [int]$_.packetIndex; physicsSeed = [long]$_.physicsSeed
        chemistryStartDigest = [string]$_.chemistryStartDigest; moleculeCount = [int]$_.moleculeCount } })
}

# Stock and patched libraries of one cluster must hold the same physical packets: same physics
# seed and same chemistry-start digest, packet by packet. Returns the digest rows; stops at
# the first difference (the comparison would not be of the same packets).
function Assert-DigestsEqual([string]$Cluster, [string]$StockDir, [string]$PatchedDir, [int]$Expected) {
  $stock = Read-CapturePackets $StockDir
  $patched = Read-CapturePackets $PatchedDir
  if ($stock.Count -ne $Expected -or $patched.Count -ne $Expected) {
    Stop-Check "cluster ${Cluster}: expected $Expected packets, stock $($stock.Count), patched $($patched.Count)" 1
  }
  $rows = for ($i = 0; $i -lt $Expected; $i++) {
    if ($stock[$i].physicsSeed -ne $patched[$i].physicsSeed -or
        $stock[$i].chemistryStartDigest -ne $patched[$i].chemistryStartDigest) {
      Stop-Check ("DIGEST_MISMATCH cluster $Cluster packet ${i}: seed $($stock[$i].physicsSeed)/$($patched[$i].physicsSeed), " +
        "chemistryStartDigest $($stock[$i].chemistryStartDigest) (stock) vs $($patched[$i].chemistryStartDigest) (patched)") 1
    }
    [pscustomobject]@{ cluster = $Cluster; packetIndex = $i; physicsSeed = $stock[$i].physicsSeed
      chemistryStartDigest = $stock[$i].chemistryStartDigest
      stockMoleculeCount = $stock[$i].moleculeCount; patchedMoleculeCount = $patched[$i].moleculeCount; equal = $true }
  }
  return @($rows)
}

# New-old contact figures and the stock gate counters of one ledger.json.
function Read-LedgerContacts([string]$LedgerFile) {
  $ledger = Get-Content -LiteralPath $LedgerFile -Raw | ConvertFrom-Json
  $contacts = $ledger.newOldContacts
  $perPair = @{}
  foreach ($member in $contacts.perPair.PSObject.Properties) { $perPair[$member.Name] = [int]$member.Value }
  $second = $ledger.firstPacketSecondTrial
  return [pscustomobject]@{
    buildMode = [string]$ledger.buildMode
    allPassed = [bool]$ledger.checks.allPassed
    pairs = [int]$contacts.pairs
    activationPairs = @($contacts.perActivation | ForEach-Object { [int]$_.pairs })
    newMoleculesInContact = [int]$contacts.newMoleculesInContact
    perPair = $perPair
    trialledByKernel = [bool]$contacts.trialledByKernel
    startZeroStepReactions = if ($second) { [int]$second.startZeroStepReactions } else { $null }
    secondTrialPairs = if ($second) { [int]$second.pairs } else { $null }
    secondTrialExpectedReactions = if ($second) { [double]$second.expectedReactions } else { $null }
  }
}

# Mean, standard error (sample sd / sqrt(n)) and n of a list of numbers.
function Get-MeanSe([double[]]$Values) {
  $n = $Values.Count
  if ($n -eq 0) { return [pscustomobject]@{ n = 0; mean = [double]::NaN; se = [double]::NaN; sum = 0 } }
  $sum = ($Values | Measure-Object -Sum).Sum
  $mean = $sum / $n
  $se = 0.0
  if ($n -gt 1) {
    $sq = 0.0
    foreach ($v in $Values) { $sq += ($v - $mean) * ($v - $mean) }
    $se = [math]::Sqrt($sq / ($n - 1)) / [math]::Sqrt($n)
  }
  return [pscustomobject]@{ n = $n; mean = $mean; se = $se; sum = $sum }
}

function Format-Number([double]$Value, [string]$Format = 'G6') {
  return $Value.ToString($Format, [cultureinfo]::InvariantCulture)
}

# Runs compare-builds and returns its output lines, exit code and the parsed verdict counts.
function Invoke-CompareBuilds([string]$Exe, [string]$StockDir, [string]$PatchedDir, [string]$CsvFile) {
  $lines = @(& $Exe --stock $StockDir --patched $PatchedDir --out $CsvFile 2>&1 | ForEach-Object { "$_" })
  $exit = $LASTEXITCODE
  $counts = @{ EQUIVALENT = 0; DIFFERENT = 0; INCONCLUSIVE = 0; EMPTY = 0; biasFlags = 0; biasFlagsEquivalent = 0 }
  foreach ($line in $lines) {
    if ($line -match '^verdicts EQUIVALENT=(\d+) DIFFERENT=(\d+) INCONCLUSIVE=(\d+) EMPTY=(\d+)') {
      $counts.EQUIVALENT = [int]$Matches[1]; $counts.DIFFERENT = [int]$Matches[2]
      $counts.INCONCLUSIVE = [int]$Matches[3]; $counts.EMPTY = [int]$Matches[4]
    }
    if ($line -match '^biasFlags=(\d+) biasFlagsEquivalent=(\d+)') {
      $counts.biasFlags = [int]$Matches[1]; $counts.biasFlagsEquivalent = [int]$Matches[2]
    }
  }
  return [pscustomobject]@{ exitCode = $exit; lines = $lines; counts = $counts }
}
