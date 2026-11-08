param([string]$OutDir)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/CalibratedLedger.ps1"
. "$PSScriptRoot/CalibratedProcess.ps1"
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
function Assert($ok,$message){if(!$ok){throw $message}}
$state=@{Root=$OutDir;Clock=[Diagnostics.Stopwatch]::StartNew();Jobs=@{one=[pscustomobject]@{name='one';status='unexecuted';deadlineSeconds=0;startedSeconds=0;finishedSeconds=0;launcherExit=-1}};Counts=@{capture=0};Failures=@{};Stopped=$false;StopReason='';Calls=0}
$saveLedger=${function:Save-CalibratedLedger};$state.Save={& $saveLedger $state}.GetNewClosure()
& $state.Save
# Independent runspace releases an incompatible read handle while writer retries.
$sync=[hashtable]::Synchronized(@{Ready=$false})
$worker=[powershell]::Create()
[void]$worker.AddScript({param($path,$sync) $h=[IO.File]::Open($path,'Open','Read','Read');$sync.Ready=$true;Start-Sleep -Milliseconds 200;$h.Dispose()}).AddArgument("$OutDir/processes.csv").AddArgument($sync)
$async=$worker.BeginInvoke()
while(!$sync.Ready){Start-Sleep -Milliseconds 5}
$state.Jobs.one.status='prepared'
& $state.Save
$worker.EndInvoke($async);$worker.Dispose()
Assert ((Read-CalibratedLedger "$OutDir/processes.csv").status -eq 'prepared') 'Transient lock not recovered'
$state.Jobs.one.status='unexecuted'
$state.Launch={param($seconds,$stem,$exe,$arguments) $state.Calls++;$state.Lock=[IO.File]::Open("$OutDir/processes.csv",'Open','Read','Read');return 0}.GetNewClosure()
$state.Audit={return 0}
try {
 $failed=$false;try{Invoke-CalibratedProcess $state one capture mock @()}catch{$failed=$true}
 Assert $failed 'Terminal save fault missing'
 Assert ($state.Calls -eq 1 -and $state.Jobs.one.status -eq 'accepted') 'Completed result lost'
 $rejected=$false;try{Invoke-CalibratedProcess $state one capture mock @()}catch{$rejected=$true}
 Assert ($rejected -and $state.Calls -eq 1) 'Duplicate launch'
} finally {$state.Lock.Dispose()}
& $state.Save
Assert ((Read-CalibratedLedger "$OutDir/processes.csv").status -eq 'accepted') 'Terminal state not recoverable'
# A directory at the argument-file destination forces prelaunch I/O failure.
$state.Jobs['argument-fault']=[pscustomobject]@{name='argument-fault';status='unexecuted';deadlineSeconds=0;startedSeconds=0;finishedSeconds=0;launcherExit=-1}
New-Item -ItemType Directory "$OutDir/argument-fault.args" | Out-Null
$beforeCount=$state.Counts.capture;$beforeCalls=$state.Calls
$failed=$false
try { Invoke-CalibratedProcess $state argument-fault capture mock @('argument') } catch {$failed=$true}
Assert $failed 'Argument persistence failure missing'
Assert ($state.Counts.capture -eq $beforeCount -and $state.Calls -eq $beforeCalls) 'Prelaunch argument failure counted or launched'
Assert ($state.Stopped -and $state.StopReason -match 'argument persistence failed') 'Argument failure stop reason missing'
Assert ((Read-CalibratedLedger "$OutDir/processes.csv" | Where-Object name -eq 'argument-fault').status -eq 'unexecuted_argument_failure') 'Argument failure not durably classified'
'LEDGER_FAULTS_PASS'
