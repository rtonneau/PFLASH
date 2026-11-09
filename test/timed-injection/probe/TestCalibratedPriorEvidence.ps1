param([Parameter(Mandatory=$true)][string]$EvidenceDir,[Parameter(Mandatory=$true)][string]$Checker,[switch]$RealInventory)
$ErrorActionPreference='Stop'
if(Test-Path $EvidenceDir){throw 'Fresh output required'}
New-Item -ItemType Directory $EvidenceDir | Out-Null
$EvidenceDir=(Resolve-Path $EvidenceDir).Path
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$session="$repo/.scratch/tests/2026-11-08__beam-on-fresh-source-calibration"
if($RealInventory){$root="$repo/.scratch/tests"; $out=$EvidenceDir}else{
 $root="$EvidenceDir/fixture"; $out="$root/active"
 foreach($d in @($root,"$root/study","$root/nested/synthetic","$root/build/deep","$out/descendant","$root/active-sibling")){New-Item -ItemType Directory $d -Force | Out-Null}
 Set-Content "$root/streams.tsv" "seed`n100000000"
 Set-Content "$root/study/streams.tsv" "seed`n100000001"
 Set-Content "$root/nested/synthetic/streams.tsv" "seed`n100000002"
 Set-Content "$root/build/CMakeCache.txt" 'fixture'
 Set-Content "$root/build/deep/streams.tsv" "seed`n100000003"
 Set-Content "$root/nested/prior.args" '100000004'
 Set-Content "$root/active-sibling/streams.tsv" "seed`n100000005"
 Set-Content "$out/streams.tsv" "seed`n777777771"
 Set-Content "$out/descendant/streams.tsv" "seed`n777777772"
}
# Execute the actual startup discovery, hashing, reservation and pure planner block.
$source=Get-Content "$PSScriptRoot/RunCalibratedBeamOn.ps1" -Raw
$start=$source.IndexOf('. "$PSScriptRoot/CalibratedPriorEvidence.ps1"')
$end=$source.IndexOf('foreach($seed in 900000001L..900000006L)',$start)
if($start -lt 0 -or $end -lt 0){throw 'Startup anchors missing'}
$block=$source.Substring($start,$end-$start)
# Only substitute the evidence root for the isolated fixture.
$block=$block.Replace('"$repo/.scratch/tests"',' $root ')
$block=$block.Replace('$PSScriptRoot', $PSScriptRoot)
if(!$RealInventory){$block=$block.Replace('& $checker plan', 'Move-Item -LiteralPath "$out/streams.tsv" -Destination "$EvidenceDir/excluded-streams.tsv"; & $checker plan')}
$OutDir=$out
. ([scriptblock]::Create($block))
if(!$RealInventory){
 foreach($seed in 100000000L..100000005L){if(!$reserved.Contains($seed)){throw "Omitted fixture seed $seed"}}
 foreach($seed in 777777771L..777777772L){if($reserved.Contains($seed)){throw 'Active subtree included'}}
 if($priorFiles.Count -ne 6){throw "Fixture file count $($priorFiles.Count)"}
 $failed=$false;try{Get-CalibratedPriorEvidence "$root/missing" $out}catch{$failed=$true};if(!$failed){throw 'Discovery did not fail closed'}
 New-Item -ItemType Junction -Path "$root/loop" -Target $root | Out-Null
 try {
  $failed=$false
  try {Get-CalibratedPriorEvidence $root $out | Out-Null}catch{$failed=$true}
  if(!$failed){throw 'Reparse loop not rejected'}
 } finally {Remove-Item -LiteralPath "$root/loop"}
}else{
 $expected=Import-Csv "$session/06-preflight/registry-inventory.csv"
 foreach($row in $expected){
  if($row.Path -notin $priorFiles.FullName){throw "Missing registry $($row.Path)"}
  if((Get-FileHash -LiteralPath $row.Path).Hash -ne $row.Hash){throw 'Prior registry changed'}
 }
 $priorFiles | Where-Object Name -eq 'streams.tsv' | Get-FileHash | Select-Object Path,Hash | Export-Csv "$out/registry-inventory.csv" -NoTypeInformation
 "ExpectedEarlierRegistries=$($expected.Count)" | Set-Content "$out/result.txt"
}
"PASS files=$($priorFiles.Count) reserved=$($reserved.Count) planned=$($freshSeeds.Count) unique/disjoint; pure planning only" | Tee-Object -FilePath "$EvidenceDir/verification.txt"
