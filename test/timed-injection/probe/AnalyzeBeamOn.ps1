param([Parameter(Mandatory=$true)][string]$EvidenceDir)
$ErrorActionPreference='Stop'
$EvidenceDir=(Resolve-Path $EvidenceDir).Path
$resultFile="$EvidenceDir/results.csv"
if(!(Test-Path $resultFile)) { throw 'Missing protocol results.' }
$status=@{}; foreach($row in (Import-Csv $resultFile)) { $status[$row.name]=$row.pass -eq 'True' }
$measurements=@{}
foreach($condition in @('water','oxygen')) {
 foreach($rate in @(1,10,100,1000)) {
  $names=for($cluster=0;$cluster -lt 4;++$cluster) { for($replica=0;$replica -lt 2;++$replica) { "$condition-rate$rate-c$cluster-r$replica" } }
  $complete=$true
  foreach($name in $names) { if(!$status.ContainsKey($name) -or !$status[$name]) { $complete=$false } }
  if(!$complete) { Write-Output "$condition rate=$rate INCOMPLETE_OR_CENSORED"; continue }
  $records=@()
  foreach($name in $names) {
   $values=@{}
   foreach($line in (Get-Content "$EvidenceDir/$name.history")) {
    if($line -match '^species "(.*)" (\d+)$') { $values['species '+$Matches[1]]=[double]$Matches[2] }
    elseif($line -match '^(dose_at_beam_end_Gy|packets|bulk_before|bulk_after|disjoint) (\S+)$') { $values[$Matches[1]]=[double]$Matches[2] }
   }
   foreach($required in @('dose_at_beam_end_Gy','packets','bulk_before','bulk_after')) { if(!$values.ContainsKey($required)) { throw "Missing $required in $name" } }
   $values['oxygen_change']=$values['bulk_after']-$values['bulk_before']
   $records+=,$values
  }
  $keys=@($records | ForEach-Object {$_.Keys} | Sort-Object -Unique)
  foreach($key in $keys) {
   $clusterMeans=for($cluster=0;$cluster -lt 4;++$cluster) {
    $a=$records[2*$cluster]; $b=$records[2*$cluster+1]
    $av=if($a.ContainsKey($key)){$a[$key]}else{0}; $bv=if($b.ContainsKey($key)){$b[$key]}else{0}
    ($av+$bv)/2
   }
   $measurements["$condition|$rate|$key"]=$clusterMeans
   $mean=($clusterMeans | Measure-Object -Average).Average
   $sum=0; foreach($x in $clusterMeans) { $sum+=($x-$mean)*($x-$mean) }
   $half=3.182446305*([math]::Sqrt($sum/3)/2)
   Write-Output "$condition rate=$rate metric=$key mean=$mean t3_lower=$($mean-$half) t3_upper=$($mean+$half) clusters=4 replicas_per_cluster=2"
  }
 }
}
foreach($condition in @('water','oxygen')) {
 foreach($rate in @(10,100,1000)) {
  $prefix="$condition|$rate|"
  $basePrefix="$condition|1|"
  if(!$measurements.ContainsKey("${basePrefix}packets") -or !$measurements.ContainsKey("${prefix}packets")) { continue }
  $metrics=@($measurements.Keys | Where-Object {$_.StartsWith($prefix) -or $_.StartsWith($basePrefix)} | ForEach-Object {($_ -split '\|',3)[2]} | Sort-Object -Unique)
  foreach($metric in $metrics) {
   $key="$prefix$metric"; $base="$basePrefix$metric"
   $baseValues=if($measurements.ContainsKey($base)){$measurements[$base]}else{@(0,0,0,0)}
   $comparisonValues=if($measurements.ContainsKey($key)){$measurements[$key]}else{@(0,0,0,0)}
   $d=for($i=0;$i -lt 4;++$i) { $comparisonValues[$i]-$baseValues[$i] }
   $mean=($d | Measure-Object -Average).Average; $sum=0; foreach($x in $d) { $sum+=($x-$mean)*($x-$mean) }
   $half=3.182446305*([math]::Sqrt($sum/3)/2)
   Write-Output "$condition paired_rate=$rate minus=1 metric=$metric mean=$mean t3_lower=$($mean-$half) t3_upper=$($mean+$half) clusters=4"
  }
 }
}
