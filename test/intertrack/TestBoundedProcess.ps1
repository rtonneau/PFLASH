param([Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/BoundedProcess.ps1"
New-Item -ItemType Directory -Force $Root | Out-Null
function Assert($ok,$why){if(!$ok){throw $why}}
function State($name){
 $job=[pscustomobject]@{id=$name;kind='science';stem=$name;status='unexecuted'}
 'fixture' | Set-Content "$Root/$name.args"
 $s=@{Root=$Root;Jobs=@($job);Clock=[pscustomobject]@{Elapsed=[pscustomobject]@{TotalSeconds=0}};Stopped=$false;Counts=@{science=0};Caps=@{science=682};Launches=0;Audits=0}
 $s.Save={Save-Outcomes $s}.GetNewClosure()
 $s.Launch={param($seconds,$stem,$arguments)$s.Launches++;Assert ($seconds -le 60) 'deadline exceeded';return 0}.GetNewClosure()
 $s.Audit={param($id)$s.Audits++;return 0}.GetNewClosure()
 return $s
}
$s=State 'success';Assert (Invoke-MatchedJob $s $s.Jobs[0]) 'success rejected';Assert ($s.Launches -eq 1 -and $s.Counts.science -eq 1) 'incorrect attempts'
$threw=$false;try {Invoke-MatchedJob $s $s.Jobs[0]}catch{$threw=$true};Assert $threw 'retry accepted'
$s=State 'child-failed';$s.Audit={param($id)return 2};Assert (!(Invoke-MatchedJob $s $s.Jobs[0])) 'launcher zero masked child failure';Assert $s.Stopped 'failure did not stop'
$s=State 'deadline';$s.Clock.Elapsed.TotalSeconds=7200;Assert (!(Invoke-MatchedJob $s $s.Jobs[0])) 'deadline ignored';Assert ($s.Launches -eq 0) 'launched after deadline'
$s=State 'write-delay';$s.Save={$s.Clock.Elapsed.TotalSeconds=7201}.GetNewClosure();Assert (!(Invoke-MatchedJob $s $s.Jobs[0])) 'deadline not rechecked after persistence';Assert ($s.Launches -eq 0) 'launched after delayed write'
$s=State 'write-failure';$s.Save={throw 'synthetic persistence failure'};$threw=$false;try {Invoke-MatchedJob $s $s.Jobs[0]}catch{$threw=$true};Assert ($threw -and $s.Launches -eq 0) 'launched without persisted ledger'
$s=State 'cap';$s.Counts.science=682;$threw=$false;try{Invoke-MatchedJob $s $s.Jobs[0]}catch{$threw=$true};Assert ($threw -and $s.Launches -eq 0) 'cap ignored'
$s=State 'timeout';"reason timeout" | Set-Content "$Root/timeout.meta";$s.Audit={param($id)return 2};Assert (!(Invoke-MatchedJob $s $s.Jobs[0])) 'timeout accepted';Assert ($s.Jobs[0].status -eq 'censored_timeout') 'timeout lost'
$claimRoot=Join-Path $Root ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $claimRoot | Out-Null
$helper=(Resolve-Path "$PSScriptRoot/BoundedProcess.ps1").Path
$competitors=1..2 | ForEach-Object {Start-Job -ScriptBlock {param($helper,$root,$id) . $helper;try {Claim-MatchedExecution $root;Set-Content "$root/winner.txt" $id;return 'won'}catch{return 'lost'}} -ArgumentList $helper,$claimRoot,$_}
$results=@($competitors | Wait-Job | Receive-Job)
$competitors | Remove-Job
Assert (@($results|Where-Object {$_ -eq 'won'}).Count -eq 1) 'exclusive claim admitted multiple writers'
Assert (@($results|Where-Object {$_ -eq 'lost'}).Count -eq 1) 'exclusive claim did not reject competitor'
$before=Get-Content "$Root/outcomes.tsv" -Raw
$lock=[IO.File]::Open("$Root/outcomes.tsv",'Open','Read','Read')
try {$s=State 'locked-ledger';$failed=$false;try {Save-Outcomes $s}catch{$failed=$true};Assert $failed 'locked publication unexpectedly succeeded'}finally{$lock.Dispose()}
Assert ((Get-Content "$Root/outcomes.tsv" -Raw) -eq $before) 'failed publication damaged prior ledger'
Assert (@(Get-ChildItem "$Root/outcomes.tsv.*.next").Count -gt 0) 'failed publication candidate lost'
'SAFE_BOUNDED_RUNNER_PASS'
