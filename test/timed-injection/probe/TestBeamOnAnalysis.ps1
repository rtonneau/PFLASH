param([Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Refusing to overwrite analysis test evidence.'}
New-Item -ItemType Directory -Path $OutDir | Out-Null
$failures=@()
foreach($kind in @('Fresh','Empirical')) {
 $dir=Join-Path $OutDir $kind
 New-Item -ItemType Directory -Path $dir | Out-Null
 $rows=@(); $rates=if($kind -eq 'Fresh'){@(100,1000)}else{@(1,10,100,1000)}
 foreach($condition in @('water','oxygen')) {
  foreach($rate in $rates) {
   for($i=0;$i -lt 4;++$i) {
    $replicas=if($kind -eq 'Fresh'){1}else{2}
    for($r=0;$r -lt $replicas;++$r) {
     $name=if($kind -eq 'Fresh'){"$condition-realization$i-rate$rate"}else{"$condition-rate$rate-c$i-r$r"}
     $statusName=if($kind -eq 'Fresh'){"$name-history"}else{$name}
     $rows += [pscustomobject]@{name=$statusName;pass='True'}
     $species=if($rate -eq $rates[0]){'baseline-only'}else{'comparison-only'}
     @('dose_at_beam_end_Gy 0.002','packets 1','bulk_before 0','bulk_after 0',"species `"$species`" $($i+1)") | Set-Content "$dir/$name.history"
    }
   }
  }
 }
 $rows | Export-Csv "$dir/results.csv" -NoTypeInformation
 $analyzer=if($kind -eq 'Fresh'){'AnalyzeFreshBeamOn.ps1'}else{'AnalyzeBeamOn.ps1'}
 $output=@(& "$PSScriptRoot/$analyzer" -EvidenceDir $dir)
 $output | Set-Content "$dir/analysis.txt"
 foreach($condition in @('water','oxygen')) {
  foreach($rate in $rates[1..($rates.Count-1)]) {
   foreach($species in @('baseline-only','comparison-only')) {
    $line=@($output | Where-Object {$_ -like "$condition paired_rate=$rate *metric=species $species *"})
    if($line.Count -ne 1){$failures+="$kind/$condition/$rate missing $species";continue}
    if($line[0] -notmatch ' mean=(\S+) t3_lower=(\S+) t3_upper=(\S+)'){throw 'Malformed analysis output'}
    $actual=@([double]$Matches[1],[double]$Matches[2],[double]$Matches[3])
    $mean=if($species -eq 'baseline-only'){-2.5}else{2.5}
    # Four independent values 1,2,3,4: sample variance 5/3, SE sqrt(5/12).
    $half=3.182446305*[math]::Sqrt(5.0/12.0)
    $expected=@($mean,($mean-$half),($mean+$half))
    for($j=0;$j -lt 3;++$j){if([math]::Abs($actual[$j]-$expected[$j]) -gt 1e-10){$failures+="$kind/$condition/$rate incorrect $species statistic $j"}}
   }
  }
 }
 # A failed history invalidates the entire paired arm, not just its species.
 foreach($failedRate in @($rates[0],$rates[-1])) {
  $modified=@($rows | ForEach-Object {[pscustomobject]@{name=$_.name;pass=$_.pass}})
  $name=if($kind -eq 'Fresh'){"water-realization0-rate$failedRate-history"}else{"water-rate$failedRate-c0-r0"}
  ($modified | Where-Object {$_.name -eq $name}).pass='False'
  $modified | Export-Csv "$dir/results.csv" -NoTypeInformation
  $incomplete=@(& "$PSScriptRoot/$analyzer" -EvidenceDir $dir)
  $pattern=if($failedRate -eq $rates[0]){'water paired_rate=*'}else{"water paired_rate=$failedRate *"}
  if(@($incomplete | Where-Object {$_ -like $pattern}).Count){$failures+="$kind emitted a contrast for incomplete rate $failedRate"}
 }
 $rows | Export-Csv "$dir/results.csv" -NoTypeInformation
}
if($failures.Count){throw ($failures -join "`n")}
Write-Output 'PASS: both analyzers retain one-arm-only species with correct paired means and t3 intervals.'
