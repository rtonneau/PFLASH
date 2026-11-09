param([Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
$OutDir=(Resolve-Path $OutDir).Path
. "$PSScriptRoot/BoundedProcess.ps1"
if(Test-Path "$OutDir/execution-started.json"){throw 'Execution already claimed: no resume, reset or retry'}
Assert-Hashes "$OutDir/seal.csv"
Assert-Hashes "$OutDir/frozen-hashes.csv"
Assert-Hashes "$OutDir/input-hashes.csv"
Assert-Hashes "$OutDir/original-hashes.csv"
$config=Get-Content "$OutDir/runtime.json" -Raw | ConvertFrom-Json
if($config.root -ne $OutDir){throw 'Frozen output ownership mismatch'}
$env:PATH="$($config.install)/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$jobs=@(Import-Csv "$OutDir/jobs.tsv" -Delimiter "`t")
foreach($job in $jobs){$job | Add-Member status 'unexecuted'}
# Controls are ordered by condition/rate/type in the frozen table; science follows.
$jobs=@($jobs | Where-Object kind -eq control)+@($jobs | Where-Object kind -eq science)
$state=@{Root=$OutDir;Jobs=$jobs;Clock=[Diagnostics.Stopwatch]::new();Stopped=$false;Counts=@{science=0;control=0};Caps=@{science=682;control=24}}
$state.Save={Save-Outcomes $state}.GetNewClosure()
$culture=[Globalization.CultureInfo]::InvariantCulture
$state.Launch={param($seconds,$stem,$arguments) & $config.launcher 512 ($seconds.ToString('R',$culture)) $stem $config.nativeExe @arguments;return $LASTEXITCODE}.GetNewClosure()
$state.Audit={param($id) & $config.checker audit $OutDir $id $config.install > "$OutDir/jobs/$id.audit.log" 2>&1;return $LASTEXITCODE}.GetNewClosure()
# Atomic exclusive claim before the first native job; deadline cannot be reset.
Claim-MatchedExecution $OutDir
$state.Clock.Start()
try {
 & $state.Save
 foreach($job in $jobs){
  if($job.kind -eq 'science' -and @($jobs|Where-Object {$_.kind -eq 'control' -and $_.status -ne 'accepted'}).Count){$state.Stopped=$true;break}
  if(!(Invoke-MatchedJob $state $job)){break}
 }
} catch {
 $state.Stopped=$true
 $state.StopReason=$_.ToString()
} finally {
 try {& $state.Save} finally {
  @{elapsedSeconds=$state.Clock.Elapsed.TotalSeconds;scienceAttempts=$state.Counts.science;controlAttempts=$state.Counts.control;stopped=$state.Stopped} | ConvertTo-Json | Set-Content "$OutDir/execution-status.json"
 }
 Assert-Hashes "$OutDir/seal.csv"
 Assert-Hashes "$OutDir/frozen-hashes.csv"
 Assert-Hashes "$OutDir/input-hashes.csv"
 Assert-Hashes "$OutDir/original-hashes.csv"
}
& $config.checker analyze $OutDir > "$OutDir/analysis.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Analysis validation failed'}
if(@($jobs|Where-Object status -ne accepted).Count){throw 'Incomplete study; see outcomes and availability. No ensemble inference.'}
Write-Output 'MATCHED_STUDY_COMPLETE'

