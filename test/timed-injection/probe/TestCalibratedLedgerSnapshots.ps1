param([Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Use fresh synthetic evidence'}
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
. "$PSScriptRoot/CalibratedLedger.ps1"
$state=@{Root=$OutDir;Clock=[Diagnostics.Stopwatch]::StartNew();Jobs=[ordered]@{}}
for($i=0;$i -lt 256;$i++){$state.Jobs["row$i"]=[pscustomobject]@{name="row$i";generation=0;payload=('x'*256)}}
Save-CalibratedLedger $state
$sync=[hashtable]::Synchronized(@{Stop=$false;Reads=0;Error=''})
$worker=[powershell]::Create()
[void]$worker.AddScript({param($path,$sync,$helper)
 . $helper
 while(!$sync.Stop){
  try {
   $rows=@(Read-CalibratedLedger $path)
   if($rows.Count -ne 256 -or @($rows.name | Select-Object -Unique).Count -ne 256 -or @($rows.generation | Select-Object -Unique).Count -ne 1 -or @($rows | Where-Object {$_.payload -ne ('x'*256)}).Count){throw 'Mixed or truncated generation'}
   $sync.Reads++
  }catch{$sync.Error=$_.ToString();return}
 }
}).AddArgument("$OutDir/processes.csv").AddArgument($sync).AddArgument("$PSScriptRoot/CalibratedLedger.ps1")
$async=$worker.BeginInvoke()
try {
 for($generation=1;$generation -le 100;$generation++){
  foreach($row in $state.Jobs.Values){$row.generation=$generation}
  Save-CalibratedLedger $state
 }
} finally {$sync.Stop=$true;$worker.EndInvoke($async);$worker.Dispose()}
$sync | ConvertTo-Json | Set-Content "$OutDir/SnapshotResult.json"
if($sync.Error -or $sync.Reads -lt 10){throw "Snapshot check failed: $($sync.Error); reads=$($sync.Reads)"}
"CALIBRATED_SNAPSHOTS_PASS replacements=100 reads=$($sync.Reads)"
