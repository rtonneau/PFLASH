param([string]$OutDir)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/CalibratedLedger.ps1"
. "$PSScriptRoot/CalibratedProcess.ps1"
if(Test-Path $OutDir){throw 'Fresh output required'}
New-Item -ItemType Directory $OutDir | Out-Null
$root=(Resolve-Path $OutDir).Path
$source=Get-Content "$PSScriptRoot/RunCalibratedBeamOn.ps1" -Raw
# Execute the production orchestration verbatim except the two external-call closures.
# Setup/provenance is tested separately; no production audit bypass is introduced.
$body=$source.Substring($source.IndexOf('$state=@{Root='))
$body=[regex]::Replace($body,'(?m)^\$state.Launch=.*$', '$state.Launch=$mockLaunch')
$body=[regex]::Replace($body,'(?m)^\$state.Audit=.*$', '$state.Audit={return 0}')
foreach($scenario in 'zero','terminal-lock','launch-error','prelaunch-lock'){
 $OutDir=Join-Path $root $scenario
 New-Item -ItemType Directory $OutDir | Out-Null
 foreach($condition in 'water','oxygen'){
  New-Item -ItemType Directory "$OutDir/$condition" | Out-Null
  "id`tsplit`tphysics`tposition`tprecursor`n$condition-capture0`t0`t1`t2`t3`n$condition-capture1`t1`t4`t5`t6" | Set-Content "$OutDir/$condition/captures.tsv"
  "realization`tarrivalSeed`tchemistrySeed`tselectionSeed`n0`t7`t8`t9" | Set-Content "$OutDir/$condition/pairs.tsv"
 }
 $inputs=@(Get-Item "$PSScriptRoot/CalibratedLedger.ps1");$frozen=$inputs
 $inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
 $inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/manifest-hashes.csv" -NoTypeInformation
 $InstallPrefix='mock';$BuildDir='NEVER_NATIVE';$culture=[Globalization.CultureInfo]::InvariantCulture
 $checker={ $script:LASTEXITCODE=0 }
 $calls=[Collections.Generic.List[string]]::new()
 $mockLaunch={param($seconds,$stem,$exe,$arguments)
  if($seconds -gt 60 -or $seconds -le 0){throw 'Invalid deadline'}
  if($calls.Contains($stem)){throw 'Duplicate native call'}
  $calls.Add($stem)
  if($scenario -eq 'launch-error'){throw 'injected launcher exception'}
  if($scenario -eq 'terminal-lock'){$script:held=[IO.File]::Open("$OutDir/processes.csv",'Open','Read','Read');return 0}
  if($exe -like '*AnalyzeCalibratedBeamOn.exe'){"energy`tcalibration`tmean`t1" | Set-Content "$($arguments[0])/calibration.summary.tsv"}
  if($exe -like '*FreshSchedule.exe'){[IO.File]::WriteAllText($arguments[3],'');[IO.File]::WriteAllText($arguments[4],'')}
  return 0
 }
 $runBody=$body
 if($scenario -eq 'prelaunch-lock'){$runBody=$body.Replace('$state.Clock.Start();', '$script:held=[IO.File]::Open("$OutDir/processes.csv",''Open'',''Read'',''Read''); $state.Clock.Start();')}
 $failed=$false
 try { & ([scriptblock]::Create($runBody)) *> "$OutDir/driver.log" } catch {$failed=$true; $_ | Out-String | Set-Content "$OutDir/exception.txt"} finally {if($script:held){$script:held.Dispose();$script:held=$null}}
 $status=Get-Content "$OutDir/study-status.txt" -Raw
 if($scenario -eq 'zero'){
  if($failed -or $calls.Count -ne 14){throw "Zero schedule lifecycle failed: $($calls.Count)"}
  if(@(Read-CalibratedLedger "$OutDir/processes.csv" | Where-Object status -eq accepted).Count -ne 14){throw 'Durable lifecycle incomplete'}
 } else {
  $expected=if($scenario -eq 'prelaunch-lock'){0}else{1}
  if(!$failed -or $calls.Count -ne $expected -or $status -notmatch 'stop_reason \S'){throw 'Error/finally lifecycle failed'}
  if($scenario -eq 'terminal-lock' -and !(Get-ChildItem $OutDir -Filter '*.next')){throw 'Missing failed candidate'}
 }
 "PASS driver $scenario calls=$($calls.Count)"
}
'LEDGER_DRIVER_PASS'

