# Single writer, same-volume publication. Failed candidates are retained for audit.
function Save-CalibratedLedger {
 param($State)
 $path=Join-Path $State.Root 'processes.csv'
 $next="$path.$([guid]::NewGuid().ToString('N')).next"
 $State.Jobs.Values | Export-Csv -LiteralPath $next -NoTypeInformation
 $stream=[IO.File]::Open($next,'Open','Write','Read')
 try {$stream.Flush($true)} finally {$stream.Dispose()}
 $timer=[Diagnostics.Stopwatch]::StartNew()
 while($true){
  try {
   if([IO.File]::Exists($path)){[IO.File]::Replace($next,$path,[System.Management.Automation.Language.NullString]::Value)}
   else {[IO.File]::Move($next,$path)}
   return
  } catch {
   $cause=$_.Exception
   while($cause.InnerException){$cause=$cause.InnerException}
   $code=$cause.HResult -band 65535
   $remaining=14400-$State.Clock.Elapsed.TotalSeconds
   if($code -notin 32,33 -or $timer.Elapsed.TotalMilliseconds -ge 500 -or $remaining -le .025){
    throw "Ledger publication failed; previous ledger and candidate $next retained: $($cause.Message)"
   }
   $delay=[math]::Min(25,[math]::Min(500-$timer.Elapsed.TotalMilliseconds,$remaining*1000))
   if($delay -le 0){throw "Ledger publication deadline reached; candidate $next retained"}
   Start-Sleep -Milliseconds ([int][math]::Floor($delay))
   if($timer.Elapsed.TotalMilliseconds -ge 500 -or $State.Clock.Elapsed.TotalSeconds -ge 14400){
    throw "Ledger publication deadline reached; previous ledger and candidate $next retained"
   }
  }
 }
}
function Read-CalibratedLedger {
 param([string]$Path)
 # A new Windows opener can briefly see an unavailable pathname during replacement.
 $openTimer=[Diagnostics.Stopwatch]::StartNew()
 while($true){
  try {$stream=[IO.File]::Open($Path,'Open','Read',([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete));break}
  catch {
   $cause=$_.Exception;while($cause.InnerException){$cause=$cause.InnerException}
   if(($cause.HResult -band 65535) -notin 2,32,33 -or $openTimer.Elapsed.TotalMilliseconds -ge 500){throw}
   Start-Sleep -Milliseconds 5
  }
 }
 $reader=[IO.StreamReader]::new($stream)
 try {$reader.ReadToEnd() | ConvertFrom-Csv} finally {$reader.Dispose()}
}

