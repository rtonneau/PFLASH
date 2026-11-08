param([Parameter(Mandatory=$true)][string]$BuildDir,[Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Use fresh synthetic evidence'}
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path;$BuildDir=(Resolve-Path $BuildDir).Path
. "$PSScriptRoot/CalibratedLedger.ps1"
. "$PSScriptRoot/CalibratedProcess.ps1"
function Assert($ok,$message){if(!$ok){throw $message}}
# Only the pure planner runs natively. Never invoke physical/chemistry executables.
'900000001' | Set-Content "$OutDir/prior.txt"
& "$BuildDir/CheckCalibratedBeamOn.exe" plan $OutDir "$OutDir/prior.txt" > "$OutDir/plan.log"
Assert ($LASTEXITCODE -eq 0) 'Synthetic plan failed'
$launcher="$OutDir/MockLauncher.ps1";$checker="$OutDir/MockChecker.ps1"
@'
param($memory,$seconds,$stem,$exe,[Parameter(ValueFromRemainingArguments=$true)][string[]]$arguments)
$ErrorActionPreference='Stop'
if(Test-Path "$stem.mock"){throw "Duplicate mock launch: $stem"}
'synthetic' | Set-Content "$stem.mock"
$exitCode=0;$reason='complete'
if($exe -like '*AnalyzeCalibratedBeamOn.exe'){
 if($arguments.Count -eq 2){"energy`tcalibration`tmean`t0.001" | Set-Content "$($arguments[0])/calibration.summary.tsv"}
} elseif($exe -like '*FreshSchedule.exe'){
 $arrivals=if($stem.Replace('\','/') -like '*water/pair0-schedule'){@()}else{@(0..7 | ForEach-Object {"$_ $_"})}
 [IO.File]::WriteAllLines($arguments[3],[string[]]@($arrivals))
 [IO.File]::WriteAllLines($arguments[4],[string[]]@($arrivals))
 if($stem.Replace('\','/') -like '*water/pair2-schedule'){$exitCode=3}
} elseif($exe -like '*TrackCapture4um.exe'){
 if($stem.Replace('\','/') -like '*water/water-capture1288'){$exitCode=4}
 if($stem -like '*water-capture0'){'0 0.5 0.5' | Set-Content "$stem.energy"}
} elseif($exe -like '*BeamOnHistory.exe'){
 if($stem.Replace('\','/') -like '*water/pair3-100'){$exitCode=2;$reason='timeout'}
 if($stem.Replace('\','/') -like '*water/pair4-100'){$exitCode=2;'source cap exceeded; history censored' | Set-Content "$stem.err"}
} else {throw "Unexpected mock executable $exe"}
"reason $reason" | Set-Content "$stem.meta"
Write-Output $exitCode
'@ | Set-Content $launcher
@'
param($mode,$path,$id,$install)
if($mode -notin 'process','capture','identities'){throw "Unexpected audit $mode"}
exit 0
'@ | Set-Content $checker
$InstallPrefix='synthetic';$culture=[Globalization.CultureInfo]::InvariantCulture
$inputs=@(Get-Item $launcher,$checker);$frozen=@(Get-Item "$OutDir/water/captures.tsv","$OutDir/oxygen/captures.tsv")
$inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
$frozen | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/manifest-hashes.csv" -NoTypeInformation
# Execute production AST statements from state initialization to EOF, replacing
# only Launch for script mocks. A .ps1 exit does not update the module-local
# LASTEXITCODE captured by GetNewClosure as a native executable does.
# Installation/provenance startup is outside this synthetic boundary.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot/RunCalibratedBeamOn.ps1",[ref]$tokens,[ref]$errors)
Assert (!$errors) 'Production parse failed'
$statements=@($ast.EndBlock.Statements)
$start=0;while($start -lt $statements.Count -and $statements[$start].Extent.Text -notlike '$state=@{Root=*'){$start++}
Assert ($start -lt $statements.Count) 'Production orchestration boundary missing'
$driver=($statements[$start..($statements.Count-1)] | ForEach-Object {$_.Extent.Text}) -join "`n"
$driver=[regex]::Replace($driver,'(?m)^\$state.Launch=.*$', '$state.Launch={param($seconds,$stem,$exe,$arguments) return (& $launcher 512 ($seconds.ToString(''R'',$culture)) $stem $exe @arguments)}.GetNewClosure()')
# Check the mock's nonzero results before the expensive full-driver run.
$contractRoot="$OutDir-mock-contract"
if(Test-Path $contractRoot){throw 'Fresh mock contract evidence required'}
New-Item -ItemType Directory "$contractRoot/water" | Out-Null
Assert ((& $launcher 512 60 "$contractRoot/water/water-capture1288" 'TrackCapture4um.exe') -eq 4) 'Capture fault injection lost'
Assert ((& $launcher 512 60 "$contractRoot/water/pair3-100" 'BeamOnHistory.exe') -eq 2) 'Timeout injection lost'
Assert ((& $launcher 512 60 "$contractRoot/water/pair4-100" 'BeamOnHistory.exe') -eq 2) 'Source cap injection lost'
Assert ((& $launcher 512 60 "$contractRoot/water/pair2-schedule" 'FreshSchedule.exe' 0.001 100 1 "$contractRoot/low" "$contractRoot/high") -eq 3) 'Schedule fault injection lost'
$driver | Set-Content "$OutDir/ProductionOrchestration.ps1.txt"
$sync=[hashtable]::Synchronized(@{Stop=$false;Snapshots=0;Failure='';Busy=0})
$worker=[powershell]::Create()
[void]$worker.AddScript({param($root,$sync,$helper)
 . $helper
 while(!$sync.Stop){
  if(Test-Path "$root/processes.csv"){
   try {
    $rows=@(Read-CalibratedLedger "$root/processes.csv")
    if($rows.Count -ne 5380 -or @($rows.name | Select-Object -Unique).Count -ne 5380 -or @($rows | Where-Object {$_.status -notin 'unexecuted','attempted','accepted','failed','censored_timeout','censored_source_cap'}).Count){throw 'Partial or invalid snapshot'}
    $sync.Snapshots++
   } catch {
    $cause=$_.Exception;while($cause.InnerException){$cause=$cause.InnerException}
    if(($cause.HResult -band 65535) -in 32,33){$sync.Busy++}else{$sync.Failure=$_.ToString();return}
   }
  }
  Start-Sleep -Milliseconds 30
 }
}).AddArgument($OutDir).AddArgument($sync).AddArgument("$PSScriptRoot/CalibratedLedger.ps1")
$async=$worker.BeginInvoke()
try {. ([scriptblock]::Create($driver))} finally {$sync.Stop=$true;$worker.EndInvoke($async);$worker.Dispose();$sync | ConvertTo-Json | Set-Content "$OutDir/ReaderResult.json"}
Assert (!$sync.Failure -and $sync.Snapshots -gt 10) "Concurrent reader failed: $($sync.Failure)"
$rows=@(Read-CalibratedLedger "$OutDir/processes.csv")
Assert ($rows.Count -eq 5380) 'Missing registry slots'
Assert (@($rows | Where-Object status -eq unexecuted).Count -eq 20) 'Wrong unused slots'
Assert (@($rows | Where-Object status -eq failed).Count -eq 2) 'Lost failures'
Assert (@($rows | Where-Object status -like 'censored_*').Count -eq 2) 'Lost censors'
Assert (@($rows | Where-Object status -eq accepted).Count -eq 5356) 'Wrong accepted count'
Assert (@(Get-ChildItem $OutDir -Recurse -Filter '*.mock').Count -eq 5360) 'Wrong launch count'
Assert ((Get-Content "$OutDir/water/water-capture0.energy") -eq '0 0.5 0.5') 'Zero-deposit fixture lost'
'CALIBRATED_DRIVER_PASS 5380 slots; 5360 mock launches; 5356 accepted; 2 failed; 2 censored; 20 unexecuted'
