param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$Out,[int]$Replicas=16,[string]$Conditions='0,1,2,3',[int]$Parallel=6)
$ErrorActionPreference='Stop'
if($Parallel -lt 1 -or $Parallel -gt 6 -or $Replicas -lt 1){throw 'Invalid resource bound'}
$Root=(Resolve-Path $Root).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$Out=(Resolve-Path $Out).Path
$binary=Join-Path $Root '06-activation/probe/CombinedHistory.exe'
$repo=(Resolve-Path "$Root/../../..").Path
$inputs=@($binary,"$repo/test/timed-injection/probe/Capture.in",$PSCommandPath,"$PSScriptRoot/RunCombinedWorker.ps1")
$inputs+=@(Get-ChildItem "$Root/06-activation/install/bin/G4*.dll" | Sort-Object Name | ForEach-Object FullName)
foreach($packet in 0..7){$inputs+="$Root/01-msvc/validation/library$packet.packet"; $inputs+="$Root/01-msvc/validation/holdout$packet.packet"}
$manifest=@($inputs | ForEach-Object { $item=Get-Item $_; "$($item.FullName) $((Get-FileHash $item.FullName -Algorithm SHA256).Hash)" })
$manifestPath=Join-Path $Out 'batch-manifest.sha256'
if(Test-Path $manifestPath){
 if(((Get-Content $manifestPath) -join "`n") -ne ($manifest -join "`n")){throw 'Batch manifest mismatch; use a new output directory'}
} else {
 if(@(Get-ChildItem $Out -Force).Count -ne 0){throw 'Existing output has no manifest; use a new output directory'}
 Set-Content $manifestPath $manifest
}
$queue=[Collections.Generic.Queue[object]]::new()
foreach($packet in 0..7){foreach($rep in 0..($Replicas-1)){foreach($condition in $Conditions.Split(',')){ $queue.Enqueue(@($packet,$rep,[int]$condition)) }}}
$running=[Collections.Generic.List[object]]::new()
while($queue.Count -or $running.Count){
 while($queue.Count -and $running.Count -lt $Parallel){
  $task=$queue.Dequeue(); $base="$Out/p$($task[0])r$($task[1])m$($task[2])"
  if((Test-Path "$base.txt") -and (Test-Path "$base.modules.txt") -and (Test-Path "$base.ok") -and (Select-String "$base.log" -Pattern 'COMBINED_PASS' -Quiet)){continue}
  $arg=@('-NoProfile','-File',"`"$PSScriptRoot/RunCombinedWorker.ps1`"",'-Root',"`"$Root`"",'-Out',"`"$Out`"",'-Packet',$task[0],'-Rep',$task[1],'-Condition',$task[2])
  $proc=Start-Process pwsh -ArgumentList $arg -WindowStyle Hidden -PassThru -RedirectStandardOutput "$base.worker.log" -RedirectStandardError "$base.worker.err"
  $running.Add([pscustomobject]@{Process=$proc;Base=$base})
 }
 foreach($item in @($running)){ $item.Process.Refresh(); if($item.Process.HasExited){ $item.Process.WaitForExit(); if($item.Process.ExitCode -ne 0){throw "Failed: $($item.Base)"}; Write-Host "PASS $($item.Base)"; [void]$running.Remove($item) } }
 Start-Sleep -Milliseconds 250
}
$after=@($inputs | ForEach-Object { $item=Get-Item $_; "$($item.FullName) $((Get-FileHash $item.FullName -Algorithm SHA256).Hash)" })
if(($after -join "`n") -ne ($manifest -join "`n")){throw 'Batch inputs changed during execution'}
Write-Host 'ALL PASS'
