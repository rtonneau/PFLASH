# Process orchestration only. Native CheckCalibratedBeamOn owns evidence validation.
function Invoke-CalibratedProcess {
 param($State,[string]$Name,[string]$Kind,[string]$Exe,[string[]]$Arguments,[string]$Marker='-',[bool]$Kernel=$false)
 $row=$State.Jobs[$Name]
 if(!$row -or $row.status -ne 'unexecuted') { throw "Unknown or repeated planned process $Name" }
 $remaining=14400-$State.Clock.Elapsed.TotalSeconds
 if($remaining -le 0 -or $State.Stopped) { return $false }
 $caps=@{capture=4608;history=512;schedule=256;analysis=4}
 if($State.Counts[$Kind] -ge $caps[$Kind]) { throw "Process budget exceeded: $Kind" }
 $deadline=[math]::Min(60.0,[double]$remaining)
 $row.status='attempted'; $row.deadlineSeconds=$deadline; $row.startedSeconds=$State.Clock.Elapsed.TotalSeconds
 $State.Counts[$Kind]++; & $State.Save
 $stem=Join-Path $State.Root $Name
 $Arguments | Set-Content "$stem.args"
 $launchExit=& $State.Launch $deadline $stem $Exe $Arguments
 Set-Content "$stem.launcher-exit" $launchExit
 $row.launcherExit=$launchExit
 $install=if($Kernel){$State.Install}else{'-'}
 $auditExit=& $State.Audit $stem $Marker $install
 $row.status=if($launchExit -eq 0 -and $auditExit -eq 0){'accepted'}else{'failed'}
 if($row.status -eq 'failed') {
  $meta=if(Test-Path "$stem.meta"){Get-Content "$stem.meta" -Raw}else{''}
  $err=if(Test-Path "$stem.err"){Get-Content "$stem.err" -Raw}else{''}
  if($err -match 'source cap exceeded; history censored') {$row.status='censored_source_cap'}
  elseif($meta -match '(?m)^reason (timeout|job_memory_limit|system_floor|system_floor_prelaunch)\r?$') {$row.status='censored_'+$Matches[1]}
  # Censoring is retained; only repeated technical failures stop all further work.
  if($row.status -ne 'censored_source_cap') {
   $key="$Kind/$($row.status)/$launchExit"
   if(!$State.Failures.ContainsKey($key)) {$State.Failures[$key]=0}
   $State.Failures[$key]++
   if($State.Failures[$key] -ge 2) {$State.Stopped=$true; $State.StopReason="BLOCKED repeated $key"}
  }
 }
 $row.finishedSeconds=$State.Clock.Elapsed.TotalSeconds
 & $State.Save
 return $row.status -eq 'accepted'
}

