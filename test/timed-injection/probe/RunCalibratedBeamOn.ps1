param(
 [Parameter(Mandatory=$true)][string]$BuildDir,
 [Parameter(Mandatory=$true)][string]$FinderBuildDir,
 [Parameter(Mandatory=$true)][string]$InstallPrefix,
 [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
$culture=[Globalization.CultureInfo]::InvariantCulture
if(!$env:DEV_DIR -or !$env:G4_ROOT){throw 'Run ~/dotfiles/scripts/windows/setup-dev-env.ps1 and restart the shell.'}
if(Test-Path $OutDir){throw 'Use a fresh evidence directory; resuming or replacing attempts is forbidden.'}
$BuildDir=(Resolve-Path $BuildDir).Path; $FinderBuildDir=(Resolve-Path $FinderBuildDir).Path
$InstallPrefix=(Resolve-Path $InstallPrefix).Path
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$private=(Resolve-Path "$repo/.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install").Path
if($InstallPrefix -ne $private){throw 'Use the validated private runtime.'}
if((Get-FileHash "$InstallPrefix/bin/G4processes_core.dll").Hash -ne '11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B'){throw 'Private core hash differs from validated runtime.'}
$launcher="$FinderBuildDir/JobLauncher.exe"; $checker="$BuildDir/CheckCalibratedBeamOn.exe"
$exePaths=@($launcher,$checker,"$BuildDir/AnalyzeCalibratedBeamOn.exe","$BuildDir/FreshSchedule.exe","$BuildDir/TrackCapture4um.exe","$BuildDir/BeamOnHistory.exe")
foreach($exe in $exePaths){if(!(Test-Path $exe)){throw "Missing executable $exe"}}
New-Item -ItemType Directory $OutDir | Out-Null; $OutDir=(Resolve-Path $OutDir).Path
$env:PATH="$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
. "$PSScriptRoot/CalibratedProcess.ps1"
# Reserve observed historical numeric tokens, including intentionally paired reuse,
# once per seed value. Keep every source hash so this conservative inventory is reproducible.
$oldRoots=@(Get-ChildItem "$repo/.scratch/tests" -Directory | Where-Object {$_.Name -ne '2026-11-08__beam-on-fresh-source-calibration'})
function Get-PriorEvidence($directory){
 # Prune runtime/source/build trees before recursion; those are not evidence.
 if((Test-Path "$directory/CMakeCache.txt") -or (Test-Path "$directory/lib/cmake/Geant4") -or (Test-Path "$directory/source/processes") -or ([IO.Path]::GetFileName($directory) -in '.git','CMakeFiles','vcpkg_installed')){return}
 foreach($item in Get-ChildItem -LiteralPath $directory){
  if($item.PSIsContainer){Get-PriorEvidence $item.FullName}
  elseif($item.Extension -in '.args','.history','.calibration' -or $item.Name -in 'histories.csv','runs.csv','results.csv' -or $item.Name -match '^(protocol-at-run|seed|capture-ledger)'){$item}
 }
}
$priorFiles=@($oldRoots | ForEach-Object {Get-PriorEvidence $_.FullName})
$controls="$repo/.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/03-native-controls"
if(Test-Path $controls){$priorFiles+=@(Get-ChildItem $controls -File | Where-Object {$_.Extension -in '.args','.history'})}
if(!$priorFiles.Count){throw 'Missing prior seed evidence.'}
$priorFiles | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/prior-input-hashes.csv" -NoTypeInformation
$reserved=[Collections.Generic.HashSet[long]]::new()
foreach($file in $priorFiles){
 foreach($match in [regex]::Matches([IO.File]::ReadAllText($file.FullName),'(?<![\w.])\d+(?![\w.])')){
  $v=0L; if([long]::TryParse($match.Value,[ref]$v) -and $v -gt 0 -and $v -le 2147483646){[void]$reserved.Add($v)}
 }
}
@($reserved | Sort-Object) | Set-Content "$OutDir/prior-seeds.txt"
& $checker plan $OutDir "$OutDir/prior-seeds.txt" | Set-Content "$OutDir/plan.log"
if($LASTEXITCODE -ne 0){throw 'Manifest freeze failed.'}
Copy-Item "$PSScriptRoot/CalibratedBeamOnProtocol.md" "$OutDir/protocol-at-run.md"
$inputs=@($exePaths | Get-Item)+@(Get-ChildItem "$InstallPrefix/bin/G4*.dll")+@(Get-ChildItem "$PSScriptRoot/*" -File | Where-Object {$_.Extension -in '.cc','.hh','.ps1','.in','.md' -or $_.Name -eq 'CMakeLists.txt'})
$inputs+=@(Get-ChildItem "$repo/src","$repo/header" -Recurse -File)+@(Get-Item "$repo/.claude/.claude-project.json","$repo/test/molecular-finder/JobLauncher.cc")
$inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
New-Item -ItemType Directory "$OutDir/input-bytes" | Out-Null
$snapshotRows=[Collections.Generic.List[object]]::new(); $inputIndex=0
foreach($inputFile in $inputs){
 $copy="$OutDir/input-bytes/$inputIndex-$($inputFile.Name)"; Copy-Item -LiteralPath $inputFile.FullName -Destination $copy
 $snapshotRows.Add([pscustomobject]@{source=$inputFile.FullName;snapshot=$copy;hash=(Get-FileHash $copy).Hash});$inputIndex++
}
$snapshotRows | Export-Csv "$OutDir/input-snapshots.csv" -NoTypeInformation
$frozen=@(Get-Item "$OutDir/streams.tsv","$OutDir/prior-seeds.txt","$OutDir/prior-input-hashes.csv","$OutDir/protocol-at-run.md")
foreach($condition in 'water','oxygen'){$frozen+=@(Get-Item "$OutDir/$condition/planned-ledger.txt","$OutDir/$condition/captures.tsv","$OutDir/$condition/pairs.tsv","$OutDir/$condition/paired-index.txt","$OutDir/$condition/capture-index.txt","$OutDir/$condition/analysis-input.txt")}
$frozen | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/manifest-hashes.csv" -NoTypeInformation
$state=@{Root=$OutDir;Install=$InstallPrefix;Clock=[Diagnostics.Stopwatch]::new();Counts=@{capture=0;history=0;schedule=0;analysis=0};Jobs=[ordered]@{};Failures=@{};Stopped=$false;StopReason=''}
function Add-Job($name,$kind){$state.Jobs[$name]=[pscustomobject]@{name=$name;kind=$kind;status='unexecuted';deadlineSeconds=0;startedSeconds=0;finishedSeconds=0;launcherExit=-1}}
foreach($condition in 'water','oxygen'){
 foreach($capture in (Import-Csv "$OutDir/$condition/captures.tsv" -Delimiter "`t")){Add-Job "$condition/$($capture.id)" 'capture'}
 for($r=0;$r -lt 128;$r++){Add-Job "$condition/pair$r-schedule" 'schedule';foreach($rate in 100,1000){Add-Job "$condition/pair$r-$rate" 'history'}}
 Add-Job "$condition/calibration-analysis" 'analysis';Add-Job "$condition/final-analysis" 'analysis'
}
$state.Save={ $state.Jobs.Values | Export-Csv "$OutDir/processes.csv.next" -NoTypeInformation; Move-Item -LiteralPath "$OutDir/processes.csv.next" -Destination "$OutDir/processes.csv" -Force }.GetNewClosure()
$state.Launch={param($seconds,$stem,$exe,$arguments) & $launcher 512 ($seconds.ToString('R',$culture)) $stem $exe @arguments; return $LASTEXITCODE}.GetNewClosure()
$state.Audit={param($stem,$marker,$install) & $checker process $stem $marker $install > "$stem.audit.log" 2>&1; return $LASTEXITCODE}.GetNewClosure()
& $state.Save
function Capture-One($condition,$row,$macro){
 $name="$condition/$($row.id)"; $prefix="$OutDir/$name"
 $ok=$false
 try {
  $ok=Invoke-CalibratedProcess $state $name 'capture' "$BuildDir/TrackCapture4um.exe" @($macro,'capture',$prefix,$row.physics,$row.position,$row.precursor) 'PACKET_VALIDATED' $true
 } finally {
  # Reconcile even when launch or process audit throws after persisting an attempt.
  # The native audit retains incomplete evidence as a rejection, without retrying.
 if($state.Jobs[$name].status -ne 'unexecuted'){
  & $checker capture "$OutDir/$condition" $row.id $InstallPrefix > "$prefix.capture-audit.log" 2>&1
  if($LASTEXITCODE -ne 0){
   if($state.Jobs[$name].status -eq 'accepted'){
    if(!$state.Failures.ContainsKey('capture/audit')){$state.Failures['capture/audit']=0}
    $state.Failures['capture/audit']++;if($state.Failures['capture/audit'] -ge 2){$state.Stopped=$true;$state.StopReason='BLOCKED repeated capture/audit'}
   }
   # Preserve the launch censor reason as well as the native rejection diagnostic.
   if($state.Jobs[$name].status -eq 'accepted'){$state.Jobs[$name].status='failed_capture_audit'}
   $ok=$false;& $state.Save
  }
 }
 }
 return $ok
}
$state.Clock.Start(); [DateTime]::UtcNow.ToString('o') | Set-Content "$OutDir/study-start-utc.txt"
try {
 foreach($condition in 'water','oxygen'){
  $dir="$OutDir/$condition"; $macro=if($condition -eq 'water'){"$PSScriptRoot/MesoFixture4um.in"}else{"$PSScriptRoot/MesoFixture4umOxygen.in"}
  $captures=@(Import-Csv "$dir/captures.tsv" -Delimiter "`t")
  foreach($row in $captures | Where-Object split -eq '0'){[void](Capture-One $condition $row $macro)}
  $calibrated=Invoke-CalibratedProcess $state "$condition/calibration-analysis" 'analysis' "$BuildDir/AnalyzeCalibratedBeamOn.exe" @($dir,'--calibration-only')
  # Freeze calibration output before any holdout process, even if incomplete.
  if(Test-Path "$dir/calibration.summary.tsv"){
   Copy-Item "$dir/calibration.summary.tsv" "$dir/frozen-calibration.tsv"
   Get-FileHash "$dir/frozen-calibration.tsv" | Export-Csv "$dir/frozen-calibration-hash.csv" -NoTypeInformation
  }
  foreach($row in $captures | Where-Object split -eq '1'){[void](Capture-One $condition $row $macro)}
  if($calibrated){
   $summary=(Get-Content "$dir/frozen-calibration.tsv" | Where-Object {$_ -like "energy`tcalibration`t*"}) -split "`t"
   $mean=$summary[3]
   foreach($pair in (Import-Csv "$dir/pairs.tsv" -Delimiter "`t")){
    $r=$pair.realization; $low="$dir/pair$r-100.schedule"; $high="$dir/pair$r-1000.schedule"
    if(!(Invoke-CalibratedProcess $state "$condition/pair$r-schedule" 'schedule' "$BuildDir/FreshSchedule.exe" @($mean,'100',$pair.arrivalSeed,$low,$high) 'FRESH_SCHEDULE_PASS')){continue}
    $arrivals=@(Get-Content $low | Where-Object {$_ -ne ''});$paths=[Collections.Generic.List[string]]::new();$valid=$true
    for($k=0;$k -lt $arrivals.Count;$k++){
     $row=$captures[1280+8*[int]$r+$k]
     if(!(Capture-One $condition $row $macro)){$valid=$false}
     $paths.Add('"'+("$dir/$($row.id).packet".Replace('\','/'))+'"')
    }
    if(!$valid){continue}
    # Empty schedules import no packet. An accepted calibration packet supplies geometry only.
    if(!$paths.Count){$paths.Add('"'+("$dir/$condition-capture0.packet".Replace('\','/'))+'"')}
    $manifest="$dir/pair$r.manifest"; $paths | Set-Content $manifest
    foreach($rate in 100,1000){
     $exposure=2e9/$rate; $endpoint=$exposure+2; $stem="$dir/pair$r-$rate"
     @("frozen_calibration frozen-calibration.tsv","mean_MeV $mean","mass_kg 6.4e-14","dose_rate_Gy_s $rate","nominal_dose_Gy 0.002","arrival_seed $($pair.arrivalSeed)","schedule_law exponential-normalized-shared-pair","schedule_job pair$r-schedule") | Set-Content "$stem.calibration"
     [void](Invoke-CalibratedProcess $state "$condition/pair$r-$rate" 'history' "$BuildDir/BeamOnHistory.exe" @($macro,$manifest,"$stem.history",$exposure.ToString('R',$culture),$endpoint.ToString('R',$culture),$pair.chemistrySeed,$pair.arrivalSeed,$pair.selectionSeed,'fixed',"$stem.schedule",'8') 'BEAM_ON_HISTORY_PASS' $true)
    }
   }
  }
  [void](Invoke-CalibratedProcess $state "$condition/final-analysis" 'analysis' "$BuildDir/AnalyzeCalibratedBeamOn.exe" @($dir))
 }
} finally {
 & $state.Save
 $inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-after.csv" -NoTypeInformation
 $frozen | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/manifest-hashes-after.csv" -NoTypeInformation
 @("elapsed_seconds $($state.Clock.Elapsed.TotalSeconds)","stop_reason $($state.StopReason)","physical_attempts $($state.Counts.capture)","chemistry_histories $($state.Counts.history)","schedule_jobs $($state.Counts.schedule)") | Set-Content "$OutDir/study-status.txt"
 foreach($condition in 'water','oxygen'){& $checker identities "$OutDir/$condition" > "$OutDir/$condition/identity-audit.log" 2>&1;if($LASTEXITCODE -ne 0){$state.StopReason='manifest identity audit failed'}}
 if((Compare-Object (Import-Csv "$OutDir/hashes-before.csv") (Import-Csv "$OutDir/hashes-after.csv") -Property Path,Hash) -or (Compare-Object (Import-Csv "$OutDir/manifest-hashes.csv") (Import-Csv "$OutDir/manifest-hashes-after.csv") -Property Path,Hash)){throw 'Frozen inputs changed during study.'}
}
if($state.StopReason){throw $state.StopReason}
if($state.Clock.Elapsed.TotalSeconds -ge 14400){throw 'Study wall-clock ceiling reached; unfinished work remains unexecuted.'}
if($state.Jobs['water/final-analysis'].status -ne 'accepted' -or $state.Jobs['oxygen/final-analysis'].status -ne 'accepted'){throw 'Incomplete or unsupported comparison; inspect retained evidence.'}
Write-Output 'CALIBRATED_BEAM_ON_COMPLETE'
