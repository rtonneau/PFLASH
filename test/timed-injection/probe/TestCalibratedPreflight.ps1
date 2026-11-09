param([Parameter(Mandatory=$true)][string]$EvidenceDir)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/CalibratedPreflight.ps1"
if(Test-Path -LiteralPath $EvidenceDir){throw 'Use fresh synthetic evidence'}
$evidence=New-Item -ItemType Directory $EvidenceDir
$macro=Join-Path $evidence.FullName 'dummy.in'
Set-Content $macro '# never executed'
$script:checks=0
function Reject($label,[scriptblock]$action){
 $rejected=$false
 try {& $action | Out-Null} catch {$rejected=$true}
 if(!$rejected){throw "Expected rejection: $label"}
 $script:checks++; "PASS $label"
}
$root=Join-Path $evidence.FullName 'fresh'
$plan=New-CalibratedPreflightPlan ($root.Replace('\','/')+'\') $macro
if(Test-Path $root){throw 'Planner wrote output'}
if($plan.Jobs.Count -ne 3 -or $plan.Destinations.Count -ne 29){throw 'Incomplete plan'}
$plan | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence.FullName 'plan.json')
Reject outside {Assert-PreflightDestinations $root @("$evidence/outside.packet")}
Reject sibling {Assert-PreflightDestinations $root @("$root-sibling/file.packet")}
Reject escape {Assert-PreflightDestinations $root @("$root/../escape.packet")}
Reject duplicate {Assert-PreflightDestinations $root @("$root/a.packet","$root\a.packet")}
foreach($i in 0..($plan.Destinations.Count-1)){
 $case=Join-Path $evidence.FullName "existing-$i"
 $candidate=New-CalibratedPreflightPlan $case $macro
 New-Item -ItemType Directory $case | Out-Null
 Set-Content -LiteralPath $candidate.Destinations[$i] 'original dummy bytes'
 Reject "existing-$i" {Assert-PreflightDestinations $case $candidate.Destinations}
 if((Get-Content -LiteralPath $candidate.Destinations[$i] -Raw).Trim() -ne 'original dummy bytes'){throw 'Dummy overwritten'}
}
$old=Join-Path $evidence.FullName '03-native-controls'
New-Item -ItemType Directory $old | Out-Null
Set-Content "$old/control-capture.packet" 'old evidence'
Reject old-bug {Assert-PreflightDestinations $root @("$old/control-capture.packet")}
foreach($job in $plan.Jobs){foreach($arg in $job.Arguments){if($arg.Contains('03-native-controls')){throw 'Old argument reused'}}}
if(Test-Path $root){throw 'Validation created control root'}
"PASS fresh mixed separators; no writes or launches"
"PREFLIGHT_PREVENTION_PASS rejectionChecks=$script:checks"
