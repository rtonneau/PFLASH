param([Parameter(Mandatory=$true)][string]$EvidenceDir)
$ErrorActionPreference='Stop'; $EvidenceDir=(Resolve-Path $EvidenceDir).Path
$status=@{}; foreach($row in (Import-Csv "$EvidenceDir/results.csv")) { $status[$row.name]=$row.pass -eq 'True' }
$measures=@{}
foreach($condition in @('water','oxygen')) {
 foreach($rate in @(100,1000)) {
  $names=for($r=0;$r -lt 4;++$r){"$condition-realization$r-rate$rate"}
  $complete=$true; foreach($name in $names){$key="$name-history"; if(!$status.ContainsKey($key) -or !$status[$key]){$complete=$false}}
  if(!$complete){Write-Output "$condition rate=$rate INCOMPLETE_OR_CENSORED"; continue}
  $records=@()
  foreach($name in $names) {
   $values=@{}
   foreach($line in (Get-Content "$EvidenceDir/$name.history")) {
    if($line -match '^species "(.*)" (\d+)$'){$values['species '+$Matches[1]]=[double]$Matches[2]}
    elseif($line -match '^(dose_at_beam_end_Gy|packets|bulk_before|bulk_after|disjoint) (\S+)$'){$values[$Matches[1]]=[double]$Matches[2]}
   }
   foreach($key in @('dose_at_beam_end_Gy','packets','bulk_before','bulk_after')){if(!$values.ContainsKey($key)){throw "Missing $key in $name"}}
   $values['oxygen_change']=$values['bulk_after']-$values['bulk_before']; $records+=,$values
  }
  $keys=@($records | ForEach-Object {$_.Keys} | Sort-Object -Unique)
  foreach($key in $keys) {
   $values=foreach($record in $records){if($record.ContainsKey($key)){$record[$key]}else{0}}
   $measures["$condition|$rate|$key"]=$values
   $mean=($values | Measure-Object -Average).Average; $sum=0; foreach($x in $values){$sum+=($x-$mean)*($x-$mean)}
   $half=3.182446305*([math]::Sqrt($sum/3)/2)
   Write-Output "$condition rate=$rate metric=$key mean=$mean t3_lower=$($mean-$half) t3_upper=$($mean+$half) independent_realizations=4"
  }
 }
 if(!$measures.ContainsKey("$condition|100|packets") -or !$measures.ContainsKey("$condition|1000|packets")){continue}
 $metrics=@($measures.Keys | Where-Object {$_.StartsWith("$condition|100|") -or $_.StartsWith("$condition|1000|")} | ForEach-Object {($_ -split '\|',3)[2]} | Sort-Object -Unique)
 foreach($metric in $metrics) {
  $key="$condition|1000|$metric"; $baseline="$condition|100|$metric"
  $baseValues=if($measures.ContainsKey($baseline)){$measures[$baseline]}else{@(0,0,0,0)}
  $comparisonValues=if($measures.ContainsKey($key)){$measures[$key]}else{@(0,0,0,0)}
  $d=for($i=0;$i -lt 4;++$i){$comparisonValues[$i]-$baseValues[$i]}
  $mean=($d | Measure-Object -Average).Average; $sum=0; foreach($x in $d){$sum+=($x-$mean)*($x-$mean)}
  $half=3.182446305*([math]::Sqrt($sum/3)/2)
  Write-Output "$condition paired_rate=1000 minus=100 metric=$metric mean=$mean t3_lower=$($mean-$half) t3_upper=$($mean+$half) independent_realizations=4"
 }
}
