param(
 [Parameter(Mandatory=$true)][string]$BuildDir,
 [Parameter(Mandatory=$true)][string]$FinderBuildDir,
 [Parameter(Mandatory=$true)][string]$InstallPrefix,
 [Parameter(Mandatory=$true)][string]$WaterLibrary,
 [Parameter(Mandatory=$true)][string]$OxygenLibrary,
 [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir) { throw 'Use a fresh evidence directory.' }
$BuildDir=(Resolve-Path $BuildDir).Path; $FinderBuildDir=(Resolve-Path $FinderBuildDir).Path
$InstallPrefix=(Resolve-Path $InstallPrefix).Path; $WaterLibrary=(Resolve-Path $WaterLibrary).Path; $OxygenLibrary=(Resolve-Path $OxygenLibrary).Path
New-Item -ItemType Directory $OutDir | Out-Null; $OutDir=(Resolve-Path $OutDir).Path
$env:PATH="$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$launcher="$FinderBuildDir/JobLauncher.exe"
$files=@(Get-ChildItem "$InstallPrefix/bin/G4*.dll") + @(Get-Item "$BuildDir/FreshSchedule.exe","$BuildDir/TrackCapture4um.exe","$BuildDir/BeamOnHistory.exe",$launcher)
$files+=@(Get-ChildItem "$WaterLibrary/library*.packet","$OxygenLibrary/library*.packet")
$files+=@(Get-Item "$PSScriptRoot/MesoFixture4um.in","$PSScriptRoot/MesoFixture4umOxygen.in","$PSScriptRoot/BeamOnProtocol.md")
$files | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
$conditions=@(
 @{Name='water'; Mean='0.0004583859867103157'; SE='0.0000856204856481622'; Macro="$PSScriptRoot/MesoFixture4um.in"; Bank=$WaterLibrary},
 @{Name='oxygen'; Mean='0.000556467579215713'; SE='0.00016694983712935'; Macro="$PSScriptRoot/MesoFixture4umOxygen.in"; Bank=$OxygenLibrary}
)
$rows=[System.Collections.Generic.List[object]]::new()
function Run-Capped($name,$exe,[string[]]$arguments,$marker,[bool]$requiresKernel) {
 $prefix="$OutDir/$name"; $arguments | Set-Content "$prefix.args"
 & $launcher 512 60 $prefix $exe @arguments
 $launchExit=$LASTEXITCODE
 $meta=if(Test-Path "$prefix.meta"){Get-Content "$prefix.meta" -Raw}else{''}
 $log=if(Test-Path "$prefix.log"){Get-Content "$prefix.log" -Raw}else{''}
 $err=if(Test-Path "$prefix.err"){Get-Content "$prefix.err" -Raw}else{''}
 $provenance=!$requiresKernel
 if($requiresKernel -and (Test-Path "$prefix.modules")) {
  $modules=@(Get-Content "$prefix.modules" | Where-Object {[IO.Path]::GetFileName($_) -like 'G4*.dll'})
  $provenance=@($modules | Where-Object {[IO.Path]::GetFileName($_) -eq 'G4processes_core.dll'}).Count -gt 0
  foreach($module in $modules) {if(!([IO.Path]::GetDirectoryName($module).Equals("$InstallPrefix\bin",[StringComparison]::OrdinalIgnoreCase))){$provenance=$false}}
 }
 $pass=$launchExit -eq 0 -and $meta -match '(?m)^reason complete\r?$' -and $meta -match '(?m)^exit_code 0\r?$' -and $meta -match '(?m)^cap_notification 0\r?$' -and $log -match $marker -and ($log+$err) -notmatch 'EEEE|FatalException' -and $provenance
 $outcome=if($pass){'pass'}elseif($err -match 'source cap exceeded; history censored'){'censored_source_cap'}elseif($meta -match '(?m)^reason (job_memory_limit|system_floor|system_floor_prelaunch|timeout)\r?$'){'censored_resource'}else{'failed_validation'}
 $script:rows.Add([pscustomobject]@{name=$name;pass=$pass;outcome=$outcome;provenance=$provenance})
 $script:rows | Export-Csv "$OutDir/results.csv" -NoTypeInformation
 Write-Host "$name $outcome"
 return $pass
}
for($oxygen=0;$oxygen -lt $conditions.Count;++$oxygen) {
 $condition=$conditions[$oxygen]; $enabled=@{100=$true;1000=$true}
 for($realization=0;$realization -lt 4;++$realization) {
  if(!$enabled[100] -and !$enabled[1000]) { break }
  $name="$($condition.Name)-realization$realization"; $seed=400001+10000*$oxygen+$realization
  $schedules=@{}; $valid=$true
  foreach($rate in @(100,1000)) {
   if(!$enabled[$rate]) { continue }
   $schedule="$OutDir/$name-rate$rate.schedule"; $schedules[$rate]=$schedule
   $ok=Run-Capped "$name-schedule$rate" "$BuildDir/FreshSchedule.exe" @($condition.Mean,"$rate","$seed",$schedule) 'FRESH_SCHEDULE_PASS' $false
   if(!$ok) { $valid=$false; $enabled[$rate]=$false }
  }
  if(!$valid) { continue }
  $activeRates=@(100,1000 | Where-Object {$enabled[$_]})
  if(!$activeRates.Count) { continue }
  $sourceRows=@(Get-Content $schedules[$activeRates[0]] | Where-Object {$_ -ne ''})
  if($activeRates.Count -eq 2) {
   $other=@(Get-Content $schedules[$activeRates[1]] | Where-Object {$_ -ne ''})
   if($sourceRows.Count -ne $other.Count) { throw 'paired Poisson counts differ' }
   for($i=0;$i -lt $sourceRows.Count;++$i) {
    $a=$sourceRows[$i] -split ' '; $b=$other[$i] -split ' '
    if($a[1] -ne "$i" -or $b[1] -ne "$i" -or [math]::Abs([double]$a[0]-10*[double]$b[0]) -gt 1e-6) { throw 'paired Poisson schedule mismatch' }
   }
  }
  $capturePaths=@(); $captureFailed=$false
  for($k=0;$k -lt $sourceRows.Count;++$k) {
   $offset=10000*$oxygen+1000*$realization+101*$k; $capturePrefix="$OutDir/$name-capture$k"
   $ok=Run-Capped "$name-capture$k-job" "$BuildDir/TrackCapture4um.exe" @($condition.Macro,'capture',$capturePrefix,"$(501000+$offset)","$(601000+$offset)","$(701000+$offset)") 'PACKET_VALIDATED' $true
   if(!$ok -or !(Test-Path "$capturePrefix.packet")) { $captureFailed=$true; break }
   $capturePaths+=('"'+"$capturePrefix.packet".Replace('\','/')+'"')
  }
  if($captureFailed) { if($realization -eq 0){foreach($rate in $activeRates){$enabled[$rate]=$false}}; continue }
  # A zero-arrival history needs a compatible library to initialize geometry but imports none.
  if(!$capturePaths.Count) { $capturePaths+=('"'+(Resolve-Path "$($condition.Bank)/library0.packet").Path.Replace('\','/')+'"') }
  $manifest="$OutDir/$name.manifest"; Set-Content $manifest $capturePaths
  foreach($rate in $activeRates) {
   $exposure=2e9/$rate; $endpoint=$exposure+2; $historySeed=801000+10000*$oxygen+1000*$realization
   $metadata="$OutDir/$name-rate$rate.calibration"
   @("mass_kg 6.4e-14","mean_MeV $($condition.Mean)","mean_se_MeV $($condition.SE)","dose_rate_Gy_s $rate","nominal_dose_Gy 0.002","arrival_seed $seed") | Set-Content $metadata
   $ok=Run-Capped "$name-rate$rate-history" "$BuildDir/BeamOnHistory.exe" @($condition.Macro,$manifest,"$OutDir/$name-rate$rate.history","$exposure","$endpoint","$historySeed","$seed",'1','fixed',$schedules[$rate],'8') 'BEAM_ON_HISTORY_PASS' $true
   if(!$ok -and $realization -eq 0) { $enabled[$rate]=$false }
  }
 }
}
$files | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-after.csv" -NoTypeInformation
@(Get-ChildItem "$OutDir/*-capture*.packet") | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/captured-hashes.csv" -NoTypeInformation
if(Compare-Object (Import-Csv "$OutDir/hashes-before.csv") (Import-Csv "$OutDir/hashes-after.csv") -Property Path,Hash) { throw 'Runtime changed during fresh controls.' }
if(@($rows | Where-Object {!$_.pass}).Count) { throw 'Fresh control failed or was censored; see results.csv.' }
Write-Output 'FRESH_BEAM_ON_CONTROLS_COMPLETE'
