# Read-only integrity helpers; all writes belong to the new evidence root.
function Claim-MatchedExecution([string]$Root) {
 $claim=[IO.File]::Open("$Root/execution-started.json",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 $writer=[IO.StreamWriter]::new($claim)
 $start=[DateTime]::UtcNow
 try {$writer.WriteLine((@{startUtc=$start.ToString('o');deadlineUtc=$start.AddHours(2).ToString('o');seconds=7200}|ConvertTo-Json -Compress))} finally {$writer.Dispose()}
}
function Assert-Hashes([string]$File) {
 foreach($row in (Import-Csv -LiteralPath $File)) {
  if((Get-FileHash -LiteralPath $row.Path -Algorithm SHA256).Hash -ne $row.Hash){throw "Frozen evidence changed: $($row.Path)"}
 }
}
function Save-Outcomes($State) {
 $text=[Collections.Generic.List[string]]::new();$text.Add("id`tstatus")
 foreach($job in $State.Jobs){$text.Add("$($job.id)`t$($job.status)")}
 $path=Join-Path $State.Root 'outcomes.tsv'
 $next="$path.$([guid]::NewGuid().ToString('N')).next"
 $text | Set-Content -LiteralPath $next
 $stream=[IO.File]::Open($next,'Open','Write','Read')
 try {$stream.Flush($true)} finally {$stream.Dispose()}
 # Atomic same-volume publication; on failure retain old ledger and candidate.
 if([IO.File]::Exists($path)){[IO.File]::Replace($next,$path,[System.Management.Automation.Language.NullString]::Value)}
 else {[IO.File]::Move($next,$path)}
}
function Invoke-MatchedJob($State,$Job) {
 if($Job.status -ne 'unexecuted'){throw 'Unknown or repeated attempt'}
 if($State.Clock.Elapsed.TotalSeconds -ge 7200){$Job.status='unexecuted_deadline'; & $State.Save;return $false}
 if($State.Stopped){return $false}
 if($State.Counts[$Job.kind] -ge $State.Caps[$Job.kind]){throw 'Native job cap exceeded'}
 $stem=Join-Path $State.Root $Job.stem
 $arguments=@(Get-Content -LiteralPath "$stem.args")
 $Job.status='attempted'
 try {& $State.Save} catch {$Job.status='unexecuted_persistence_failure';$State.Stopped=$true;throw}
 $remaining=7200-$State.Clock.Elapsed.TotalSeconds
 if($remaining -le 0){$Job.status='unexecuted_deadline'; & $State.Save;return $false}
 $State.Counts[$Job.kind]++
 try {
  $exit=& $State.Launch ([Math]::Min(60.0,$remaining)) $stem $arguments
  Set-Content -LiteralPath "$stem.launcher-exit" -Value $exit
  $audit=& $State.Audit $Job.id
  $Job.status=if($exit -eq 0 -and $audit -eq 0){'accepted'}else{'failed'}
  if($Job.status -ne 'accepted'){
   $meta=if(Test-Path -LiteralPath "$stem.meta"){Get-Content -LiteralPath "$stem.meta" -Raw}else{''}
   if($meta -match '(?m)^reason (timeout|job_memory_limit|system_floor|system_floor_prelaunch)\r?$'){$Job.status='censored_'+$Matches[1]}
   $err=if(Test-Path -LiteralPath "$stem.err"){Get-Content -LiteralPath "$stem.err" -Raw}else{''}
   if($err -match 'source cap exceeded'){$Job.status='censored_source_overflow'}
   $State.Stopped=$true # no retries and no successful-subset study
  }
 } catch {$Job.status='failed_exception';$State.Stopped=$true;throw} finally {& $State.Save}
 return $Job.status -eq 'accepted'
}
