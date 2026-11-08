param([Parameter(Mandatory=$true)][string]$BuildDir,[Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Use a fresh synthetic evidence directory.'}
$BuildDir=(Resolve-Path $BuildDir).Path; New-Item -ItemType Directory $OutDir | Out-Null; $OutDir=(Resolve-Path $OutDir).Path
$checker="$BuildDir/CheckCalibratedBeamOn.exe"
function Assert($ok,$message){if(!$ok){throw $message}}
function Native($exe,$arguments,$expected){& $exe @arguments >> "$OutDir/native.log" 2>&1;Assert (($LASTEXITCODE -eq 0) -eq $expected) "Unexpected native result: $arguments"}
. "$PSScriptRoot/CalibratedProcess.ps1"
# Real complete positive-molecule analyzer fixtures and malformed activation/energy mutations.
Native "$BuildDir/CalibrationStatisticsTest.exe" @("$BuildDir/AnalyzeCalibratedBeamOn.exe","$OutDir/analysis") $true
Native "$BuildDir/CaptureEvidenceTest.exe" @() $true
New-Item -ItemType Directory "$OutDir/plan","$OutDir/install/bin" | Out-Null
'synthetic module inventory only' | Set-Content "$OutDir/install/bin/G4processes_core.dll"
"100000000`n100000002`n12345" | Set-Content "$OutDir/prior.txt"
Native $checker @('plan',"$OutDir/plan","$OutDir/prior.txt") $true
$streams=@(Import-Csv "$OutDir/plan/streams.tsv" -Delimiter "`t")
Assert ($streams.Count -eq 14594) 'Wrong independent stream count'
Assert (@($streams.seed | Select-Object -Unique).Count -eq $streams.Count) 'Seed stream overlap'
Assert (@($streams | Where-Object seed -in '100000000','100000002','12345').Count -eq 0) 'Prior seed overlap'
foreach($condition in 'water','oxygen'){
 $rows=@(Import-Csv "$OutDir/plan/$condition/captures.tsv" -Delimiter "`t")
 Assert ($rows.Count -eq 2304 -and @($rows | Where-Object split -eq '0').Count -eq 1024 -and @($rows | Where-Object split -eq '1').Count -eq 256) 'Wrong frozen attempts'
 Native $checker @('identities',"$OutDir/plan/$condition") $true
}
# Actual capture audit: a complete positive packet, stale success after failure,
# missing energy and invalid module provenance must not enter accepted subset.
function CaptureFixture($n){
 $stem="$OutDir/plan/water/water-capture$n"
 Copy-Item "$OutDir/analysis/capture1280.packet" "$stem.packet"
 '0.001 0.499 0.5' | Set-Content "$stem.energy"
 'PACKET_VALIDATED 1 Edep 1000 secondary 0 massKg 6.4e-14' | Set-Content "$stem.log"
 '' | Set-Content "$stem.err"; '0' | Set-Content "$stem.launcher-exit"
 "reason complete`nexit_code 0`ncap_notification 0`nelapsed_seconds 1`njob_peak_process_memory_bytes 1024`nmin_free_bytes 4000000000" | Set-Content "$stem.meta"
 "$OutDir/install/bin/G4processes_core.dll" | Set-Content "$stem.modules"
 return $stem
}
$stem=CaptureFixture 0;Native $checker @('capture',"$OutDir/plan/water",'water-capture0',"$OutDir/install") $true
Native $checker @('capture',"$OutDir/plan/water",'water-capture0',"$OutDir/install") $false
$stem=CaptureFixture 1;'FatalException after packet write' | Set-Content "$stem.err"
Native $checker @('capture',"$OutDir/plan/water",'water-capture1',"$OutDir/install") $false
$stem=CaptureFixture 2;Remove-Item -LiteralPath "$stem.energy"
Native $checker @('capture',"$OutDir/plan/water",'water-capture2',"$OutDir/install") $false
$stem=CaptureFixture 3;'' | Set-Content "$stem.modules"
Native $checker @('capture',"$OutDir/plan/water",'water-capture3',"$OutDir/install") $false
Native $checker @('identities',"$OutDir/plan/water") $true
$ledger=Get-Content "$OutDir/plan/water/capture-ledger.txt" -Raw
Set-Content "$OutDir/plan/water/capture-ledger.txt" ($ledger.Replace('water-capture4','water-capture0'))
Native $checker @('identities',"$OutDir/plan/water") $false
Set-Content "$OutDir/plan/water/capture-ledger.txt" $ledger
# The real launcher integration is replaced only at its boundary. C++ validates
# generated artifacts; the production orchestration code must retain failures.
$state=@{Root=$OutDir;Install="$OutDir/install";Clock=[pscustomobject]@{Elapsed=[timespan]::FromSeconds(14399.75)};Counts=@{capture=0;history=0;schedule=0;analysis=0};Jobs=@{};Failures=@{};Stopped=$false;StopReason='';Calls=0;Fixture='timeout'}
foreach($name in 'deadline','memory','early','ninth','early-again','after-stop','expired'){$state.Jobs[$name]=[pscustomobject]@{status='unexecuted';deadlineSeconds=0;startedSeconds=0;finishedSeconds=0;launcherExit=-1}}
$state.Save={ $state.Jobs | ConvertTo-Json -Depth 4 | Set-Content "$OutDir/orchestration.json" }.GetNewClosure()
$state.Launch={param($seconds,$stem,$exe,$arguments)
 $state.Calls++;if(!($seconds -gt 0 -and $seconds -le .250001)){throw 'Remaining-time cap not passed'}
 $reason=if($state.Fixture -eq 'memory'){'job_memory_limit'}elseif($state.Fixture -eq 'timeout'){'timeout'}else{'complete'}
 "reason $reason`nexit_code 2`ncap_notification 0`nelapsed_seconds 0.1`njob_peak_process_memory_bytes 1024`nmin_free_bytes 4000000000" | Set-Content "$stem.meta"
 'stale marker' | Set-Content "$stem.log"
 $(if($state.Fixture -eq 'ninth'){'source cap exceeded; history censored'}else{'early failure'}) | Set-Content "$stem.err"
 return 2
}.GetNewClosure()
$state.Audit={param($stem,$marker,$install) & $checker process $stem $marker $install >> "$OutDir/native.log" 2>&1;return $LASTEXITCODE}.GetNewClosure()
foreach($item in @(@('deadline','timeout'),@('memory','memory'),@('early','early'),@('ninth','ninth'))){
 $state.Fixture=$item[1];Assert (!(Invoke-CalibratedProcess $state $item[0] 'schedule' 'fake' @() '-')) 'Failed process accepted'
}
Assert ($state.Jobs.ninth.status -eq 'censored_source_cap') 'Ninth arrival not censored'
Assert ($state.Calls -eq 4 -and $state.Counts.schedule -eq 4) 'Resampling detected'
$threw=$false;try{[void](Invoke-CalibratedProcess $state 'ninth' 'schedule' 'fake' @() '-')}catch{$threw=$true};Assert $threw 'Repeated attempt allowed'
$state.Fixture='early';[void](Invoke-CalibratedProcess $state 'early-again' 'schedule' 'fake' @() '-')
Assert $state.Stopped 'Repeated technical failure did not block'
[void](Invoke-CalibratedProcess $state 'after-stop' 'schedule' 'fake' @() '-')
Assert ($state.Jobs['after-stop'].status -eq 'unexecuted' -and $state.Calls -eq 5) 'Launched after block'
$state.Stopped=$false;$state.Clock.Elapsed=[timespan]::FromSeconds(14400)
[void](Invoke-CalibratedProcess $state 'expired' 'schedule' 'fake' @() '-')
Assert ($state.Jobs.expired.status -eq 'unexecuted' -and $state.Calls -eq 5) 'Launched after expiry'
# Real schedule helper: one normalized draw, two scaled arms, overflow retained.
Native "$BuildDir/FreshSchedule.exe" @('0.001','100','100000001',"$OutDir/low", "$OutDir/high") $true
$low=@(Get-Content "$OutDir/low");$high=@(Get-Content "$OutDir/high");$u=@(Get-Content "$OutDir/low.normalized")
Assert ($low.Count -eq $high.Count -and $low.Count -eq $u.Count) 'Paired count mismatch'
for($i=0;$i -lt $low.Count;$i++){
 $a=$low[$i] -split ' ';$b=$high[$i] -split ' ';$n=$u[$i] -split ' '
 Assert ([math]::Abs([double]$a[0]-10*[double]$b[0]) -lt 1e-6 -and [math]::Abs([double]$n[0]-[double]$a[0]/2e7) -lt 1e-14 -and $a[1] -eq "$i" -and $b[1] -eq "$i") 'Paired normalized time mismatch'
}
Native "$BuildDir/FreshSchedule.exe" @('0.000000001','100','100000001',"$OutDir/overflow-low","$OutDir/overflow-high") $false
Assert (!(Test-Path "$OutDir/overflow-low")) 'Overflow produced usable schedule'
'CALIBRATED_BEAM_ON_CHECKS_PASS'
