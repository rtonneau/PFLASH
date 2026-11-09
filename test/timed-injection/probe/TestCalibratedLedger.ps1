param([string]$OutDir)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/CalibratedLedger.ps1"
. "$PSScriptRoot/CalibratedProcess.ps1"
New-Item -ItemType Directory $OutDir -ErrorAction Stop | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
function Assert($ok,$message){if(!$ok){throw $message}}
$state=@{Root=$OutDir;Clock=[Diagnostics.Stopwatch]::StartNew();Jobs=[ordered]@{};Counts=@{capture=0;history=0;schedule=0;analysis=0};Failures=@{};Stopped=$false;StopReason='';Calls=0}
foreach($kind in 'capture','history','schedule','analysis'){
 $count=@{capture=4608;history=512;schedule=256;analysis=4}[$kind]
 for($i=0;$i -lt $count;$i++){$name="$kind$i";$state.Jobs[$name]=[pscustomobject]@{name=$name;kind=$kind;status='unexecuted';deadlineSeconds=0;startedSeconds=0;finishedSeconds=0;launcherExit=-1}}
}
$saveLedger=${function:Save-CalibratedLedger}; $state.Save={ & $saveLedger $state }.GetNewClosure()
$state.Launch={param($seconds,$stem,$exe,$arguments) $state.Calls++; return 0}.GetNewClosure()
$state.Audit={return 0}
& $state.Save
$path="$OutDir/processes.csv"
$old=[IO.File]::ReadAllText($path)
$lock=[IO.File]::Open($path,'Open','Read','Read')
try {
 $failed=$false;try{Invoke-CalibratedProcess $state 'capture0' 'capture' 'mock' @()}catch{$failed=$true;$_ | Out-String | Set-Content "$OutDir/permanent-lock.txt"}
 Assert $failed 'Permanent reader lock must fail'
 Assert ($state.Calls -eq 0 -and $state.Counts.capture -eq 0) 'Prelaunch failure counted as native attempt'
 Assert ($state.Jobs.capture0.status -eq 'unexecuted_persistence_failure') 'Incorrect prelaunch status'
 Assert ([IO.File]::ReadAllText($path) -eq $old) 'Old ledger damaged'
 Assert (@(Get-ChildItem $OutDir -Filter '*.next').Count -gt 0) 'Candidate lost'
} finally {$lock.Dispose()}
$state.Stopped=$false
& $state.Save
# Keep a realistic monitoring snapshot open during every replacement.
$reader=[IO.File]::Open($path,'Open','Read',([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
try {
 foreach($row in $state.Jobs.Values){
  if($row.name -eq 'capture0'){continue}
  [void](Invoke-CalibratedProcess $state $row.name $row.kind 'mock' @())
 }
} finally {$reader.Dispose()}
$rows=@(Read-CalibratedLedger $path)
Assert ($rows.Count -eq 5380 -and $state.Calls -eq 5379) 'Lifecycle count mismatch'
Assert (@($rows | Where-Object status -eq accepted).Count -eq 5379) 'Lost terminal state'
'LEDGER_LIFECYCLE_PASS 5380 reserved, 5379 mock launches, one prelaunch persistence failure'
