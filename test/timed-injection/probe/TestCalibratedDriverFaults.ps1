param([Parameter(Mandatory=$true)][string]$BuildDir,[Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Use fresh synthetic evidence'}
New-Item -ItemType Directory $OutDir | Out-Null
$root=(Resolve-Path $OutDir).Path;$BuildDir=(Resolve-Path $BuildDir).Path
. "$PSScriptRoot/CalibratedLedger.ps1"
. "$PSScriptRoot/CalibratedProcess.ps1"
function Assert($ok,$message){if(!$ok){throw $message}}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot/RunCalibratedBeamOn.ps1",[ref]$tokens,[ref]$errors)
Assert (!$errors) 'Production parse failed'
$statements=@($ast.EndBlock.Statements)
$start=0;while($start -lt $statements.Count -and $statements[$start].Extent.Text -notlike '$state=@{Root=*'){$start++}
Assert ($start -lt $statements.Count) 'Production orchestration boundary missing'
$driver=($statements[$start..($statements.Count-1)] | ForEach-Object {$_.Extent.Text}) -join "`n"
$driver | Set-Content "$root/ProductionOrchestration.ps1.txt"
$realSave=${function:Save-CalibratedLedger}
foreach($mode in 'Prelaunch','Terminal','Deadline'){
 $OutDir="$root/$mode";New-Item -ItemType Directory $OutDir | Out-Null
 '900000001' | Set-Content "$OutDir/prior.txt"
 & "$BuildDir/CheckCalibratedBeamOn.exe" plan $OutDir "$OutDir/prior.txt" > "$OutDir/plan.log"
 Assert ($LASTEXITCODE -eq 0) 'Synthetic plan failed'
 $launcher="$OutDir/MockLauncher.ps1";$checker="$OutDir/MockChecker.ps1"
 @'
param($memory,$seconds,$stem,$exe,[Parameter(ValueFromRemainingArguments=$true)]$arguments)
if(Test-Path "$stem.mock"){throw 'Duplicate mock launch'}
if([double]$seconds -gt 60 -or [double]$seconds -le 0){throw 'Bad native deadline'}
'synthetic' | Set-Content "$stem.mock"
exit 0
'@ | Set-Content $launcher
 'exit 0' | Set-Content $checker
 $InstallPrefix='synthetic';$culture=[Globalization.CultureInfo]::InvariantCulture
 $inputs=@(Get-Item $launcher,$checker);$frozen=@(Get-Item "$OutDir/water/captures.tsv","$OutDir/oxygen/captures.tsv")
 $inputs | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
 $frozen | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/manifest-hashes.csv" -NoTypeInformation
 $fault=@{Mode=$mode;Saves=0;Lock=$null}
 # Inject a real incompatible reader at the publication boundary. Every save
 # still executes the actual production persistence helper.
 $wrapper={param($State)
  $fault.Saves++
  if($fault.Mode -eq 'Deadline'){
   if($fault.Saves -eq 1){
    $State.Clock=[pscustomobject]@{Elapsed=[timespan]::FromSeconds(14399.75)}
    $State.Clock | Add-Member ScriptMethod Start {}
   }
   if($fault.Saves -eq 3){$State.Clock.Elapsed=[timespan]::FromSeconds(14400)}
  }
  $target=if($fault.Mode -eq 'Prelaunch'){2}elseif($fault.Mode -eq 'Terminal'){3}else{-1}
  if($fault.Saves -eq $target){$fault.Lock=[IO.File]::Open("$($State.Root)/processes.csv",'Open','Read','Read')}
  & $realSave $State
 }.GetNewClosure()
 Set-Item Function:Save-CalibratedLedger $wrapper
 $failed=$false
 try {. ([scriptblock]::Create($driver))} catch {$failed=$true;$_ | Out-String | Set-Content "$OutDir/DriverError.txt"} finally {if($fault.Lock){$fault.Lock.Dispose()}}
 Assert $failed "$mode driver incorrectly succeeded"
 $status=Get-Content "$OutDir/study-status.txt" -Raw
 Assert ($status -match '(?m)^stop_reason .+') "$mode lost terminal reason"
 $launches=@(Get-ChildItem $OutDir -Recurse -Filter '*.mock').Count
 if($mode -eq 'Prelaunch'){
  Assert ($launches -eq 0 -and $state.Counts.capture -eq 0 -and $state.Jobs['water/water-capture0'].status -eq 'unexecuted_persistence_failure') 'Prelaunch accounting wrong'
  Assert (@(Read-CalibratedLedger "$OutDir/processes.csv" | Where-Object status -ne unexecuted).Count -eq 0) 'Prelaunch damaged old ledger'
 } else {
  Assert ($launches -eq 1 -and $state.Counts.capture -eq 1 -and $state.Jobs['water/water-capture0'].status -eq 'accepted') "$mode completed launch lost or repeated"
 }
 if($mode -eq 'Deadline'){
  Assert ($status -match 'wall-clock ceiling' -and @($state.Jobs.Values | Where-Object status -eq unexecuted).Count -eq 5379) 'Deadline accounting wrong'
 } else {
  Assert (@(Get-ChildItem $OutDir -Filter '*.next').Count -ge 2) 'Failed candidates not retained'
  Assert ($status -match 'Ledger publication (failed|deadline reached)') 'Persistence exception missing from status'
 }
 "CALIBRATED_DRIVER_FAULT_PASS $mode launches=$launches"
}
Set-Item Function:Save-CalibratedLedger $realSave
