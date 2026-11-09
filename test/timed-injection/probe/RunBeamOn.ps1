param(
  [Parameter(Mandatory=$true)][string]$BuildDir,
  [Parameter(Mandatory=$true)][string]$InstallPrefix,
  [Parameter(Mandatory=$true)][string]$FinderBuildDir,
  [Parameter(Mandatory=$true)][string]$WaterLibrary,
  [Parameter(Mandatory=$true)][string]$OxygenLibrary,
  [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference = 'Stop'
if (!$env:DEV_DIR -or !$env:G4_ROOT) { throw 'Run setup-dev-env.ps1 and restart the shell.' }
if (Test-Path $OutDir) { throw 'Use a fresh evidence directory.' }
$BuildDir = (Resolve-Path $BuildDir).Path
$InstallPrefix = (Resolve-Path $InstallPrefix).Path
$FinderBuildDir = (Resolve-Path $FinderBuildDir).Path
$WaterLibrary = (Resolve-Path $WaterLibrary).Path
$OxygenLibrary = (Resolve-Path $OxygenLibrary).Path
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$env:PATH = "$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$exe = "$BuildDir/BeamOnHistory.exe"
$launcher = "$FinderBuildDir/JobLauncher.exe"
$runtimeFiles = @(Get-ChildItem "$InstallPrefix/bin/G4*.dll") + @(Get-Item $exe, $launcher)
$runtimeFiles += @(Get-ChildItem "$WaterLibrary/library*.packet", "$OxygenLibrary/library*.packet")
$runtimeFiles += @(Get-Item "$PSScriptRoot/MesoFixture4um.in", "$PSScriptRoot/MesoFixture4umOxygen.in", "$PSScriptRoot/BeamOnProtocol.md")
$runtimeFiles | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
$conditions = @(
  @{ Name='water'; Library=$WaterLibrary; Macro="$PSScriptRoot/MesoFixture4um.in"; PerCluster=4 },
  @{ Name='oxygen'; Library=$OxygenLibrary; Macro="$PSScriptRoot/MesoFixture4umOxygen.in"; PerCluster=2 }
)
foreach ($condition in $conditions) {
  for ($cluster=0; $cluster -lt 4; ++$cluster) {
    $paths = for ($i=0; $i -lt $condition.PerCluster; ++$i) {
      $path = (Resolve-Path "$($condition.Library)/library$($cluster * $condition.PerCluster + $i).packet").Path
      '"' + $path.Replace('\','/') + '"'
    }
    Set-Content "$OutDir/$($condition.Name)-cluster$cluster.manifest" $paths
  }
  $paths = for ($i=0; $i -lt 8; ++$i) {
    $path = (Resolve-Path "$($condition.Library)/library$i.packet").Path
    '"' + $path.Replace('\','/') + '"'
  }
  Set-Content "$OutDir/$($condition.Name)-cost.manifest" $paths
  for ($pair=0; $pair -lt 2; ++$pair) {
    $paths = for ($i=0; $i -lt 2; ++$i) {
      $path = (Resolve-Path "$($condition.Library)/library$($pair * 2 + $i).packet").Path
      '"' + $path.Replace('\','/') + '"'
    }
    Set-Content "$OutDir/$($condition.Name)-pair$pair.manifest" $paths
  }
}
$rows = [System.Collections.Generic.List[object]]::new()
function Invoke-History($name, $condition, $manifest, $exposure, $endpoint, $chemistrySeed, $arrivalSeed, $selectionSeed, $mode, $argument) {
  $prefix = "$OutDir/$name"
  @("macro=$($condition.Macro)", "manifest=$manifest", "exposure_ps=$exposure", "endpoint_ps=$endpoint", "chemistry_seed=$chemistrySeed", "arrival_seed=$arrivalSeed", "selection_seed=$selectionSeed", "mode=$mode", "argument=$argument", 'max_sources=8') | Set-Content "$prefix.args"
  & $launcher 512 60 $prefix $exe $condition.Macro $manifest "$prefix.history" "$exposure" "$endpoint" "$chemistrySeed" "$arrivalSeed" "$selectionSeed" $mode "$argument" 8
  $launchExit = $LASTEXITCODE
  $meta = if (Test-Path "$prefix.meta") { Get-Content "$prefix.meta" -Raw } else { '' }
  $log = if (Test-Path "$prefix.log") { Get-Content "$prefix.log" -Raw } else { '' }
  $err = if (Test-Path "$prefix.err") { Get-Content "$prefix.err" -Raw } else { '' }
  $modules = if (Test-Path "$prefix.modules") { @(Get-Content "$prefix.modules" | Where-Object { [IO.Path]::GetFileName($_) -like 'G4*.dll' }) } else { @() }
  $provenance = @($modules | Where-Object { [IO.Path]::GetFileName($_) -eq 'G4processes_core.dll' }).Count -gt 0
  foreach ($module in $modules) {
    if (!([IO.Path]::GetDirectoryName($module).Equals("$InstallPrefix\bin", [StringComparison]::OrdinalIgnoreCase))) { $provenance = $false }
  }
  $pass = $launchExit -eq 0 -and $meta -match '(?m)^reason complete\r?$' -and $meta -match '(?m)^exit_code 0\r?$' -and $meta -match '(?m)^cap_notification 0\r?$' -and $log -match 'BEAM_ON_HISTORY_PASS' -and ($log + $err) -notmatch 'EEEE|FatalException' -and $provenance
  $outcome = if($pass) {'pass'} elseif($err -match 'source cap exceeded; history censored') {'censored_source_cap'} elseif($meta -match '(?m)^reason (job_memory_limit|system_floor|system_floor_prelaunch|timeout)\r?$') {'censored_resource'} else {'failed_validation'}
  $script:rows.Add([pscustomobject]@{name=$name; pass=$pass; outcome=$outcome; launcherExit=$launchExit; provenance=$provenance})
  $script:rows | Export-Csv "$OutDir/results.csv" -NoTypeInformation
  Write-Host "$name pass=$pass"
  return $pass
}

# Compressed mechanics and costs; every packet is distinct within a history.
foreach ($condition in $conditions) {
  foreach ($n in @(2,4,8)) {
    $schedule = "$OutDir/cost-$n.schedule"
    if (!(Test-Path $schedule)) {
      $lines = for ($i=0; $i -lt $n; ++$i) { "$(0.25*$i) $i" }
      Set-Content $schedule $lines
    }
    foreach ($seed in @(93001,93002,93003)) {
      $null = Invoke-History "$($condition.Name)-cost$n-$seed" $condition "$OutDir/$($condition.Name)-cost.manifest" 10 12 $seed 1 2 fixed $schedule
    }
  }
}

# Fixed distinct-capture pairs at illustrative physical gaps, reported descriptively.
foreach ($condition in $conditions) {
  foreach ($gap in @(1000000,10000000)) {
    $schedule="$OutDir/distinct-gap$gap.schedule"
    if (!(Test-Path $schedule)) { Set-Content $schedule @('0 0',"$gap 1") }
    $smoke=Invoke-History "$($condition.Name)-distinct-gap$gap-p0-r0" $condition "$OutDir/$($condition.Name)-pair0.manifest" ($gap+1) ($gap+3) 95001 1 2 fixed $schedule
    if (!$smoke) { Write-Output "CENSORED_DISTINCT_CONTROL $($condition.Name) $gap"; continue }
    for ($pair=0; $pair -lt 2; ++$pair) {
      for ($replica=0; $replica -lt 2; ++$replica) {
        if ($pair -eq 0 -and $replica -eq 0) { continue }
        $null=Invoke-History "$($condition.Name)-distinct-gap$gap-p$pair-r$replica" $condition "$OutDir/$($condition.Name)-pair$pair.manifest" ($gap+1) ($gap+3) (95001+$replica) 1 2 fixed $schedule
      }
    }
  }
}

# Fixed-nominal-dose physical arms. A failed smoke censors the entire arm.
for ($oxygen=0; $oxygen -lt $conditions.Count; ++$oxygen) {
  $condition = $conditions[$oxygen]
  foreach ($doseRate in @(1,10,100,1000)) {
    $exposure = 2e9 / $doseRate; $endpoint = $exposure + 2
    $smokeName = "$($condition.Name)-rate$doseRate-c0-r0"
    $base = 10000*$oxygen
    $smoke = Invoke-History $smokeName $condition "$OutDir/$($condition.Name)-cluster0.manifest" $exposure $endpoint (300001+$base) (100001+$base) (200001+$base) poisson $doseRate
    if (!$smoke) { Write-Output "CENSORED_ARM $($condition.Name) $doseRate"; continue }
    for ($cluster=0; $cluster -lt 4; ++$cluster) {
      for ($replica=0; $replica -lt 2; ++$replica) {
        if ($cluster -eq 0 -and $replica -eq 0) { continue }
        $offset = $base + 10*$cluster + $replica
        $null = Invoke-History "$($condition.Name)-rate$doseRate-c$cluster-r$replica" $condition "$OutDir/$($condition.Name)-cluster$cluster.manifest" $exposure $endpoint (300001+$offset) (100001+$offset) (200001+$offset) poisson $doseRate
      }
    }
  }
}
$runtimeFiles | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-after.csv" -NoTypeInformation
if (Compare-Object (Import-Csv "$OutDir/hashes-before.csv") (Import-Csv "$OutDir/hashes-after.csv") -Property Path,Hash) { throw 'Runtime changed during histories.' }
if (@($rows | Where-Object { !$_.pass }).Count) { throw 'One or more histories failed or were censored; see results.csv and individual metadata.' }
Write-Output 'BEAM_ON_PROTOCOL_COMPLETE'
