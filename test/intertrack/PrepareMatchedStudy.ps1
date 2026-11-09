param([Parameter(Mandatory=$true)][string]$BuildDir,[Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(!$env:DEV_DIR -or !$env:G4_ROOT){throw 'Run ~/dotfiles/scripts/windows/setup-dev-env.ps1 and restart your shell.'}
$repo=(Resolve-Path "$PSScriptRoot/../..").Path
$BuildDir=(Resolve-Path $BuildDir).Path
$original=(Resolve-Path "$repo/.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/06-study").Path
$priorRoot=Split-Path $original
$native=(Resolve-Path "$priorRoot/03-probe-release").Path
$install=(Resolve-Path "$repo/.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install").Path
$launcher=(Resolve-Path "$repo/.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/finder-probe/JobLauncher.exe").Path
$probe=(Resolve-Path "$repo/test/timed-injection/probe").Path
$taskRoot=[IO.Path]::GetFullPath("$repo/.scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation")
$OutDir=[IO.Path]::GetFullPath($OutDir)
if(!$OutDir.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be a new directory under this task scratch root'} 
if(Test-Path -LiteralPath $OutDir){throw 'Use a fresh output directory; existing attempts are immutable.'}
New-Item -ItemType Directory -Path $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
. "$PSScriptRoot/BoundedProcess.ps1"
# Compare original bytes to the immutable archive inventory, not to a newly computed baseline.
$evidence=@(Import-Csv "$priorRoot/06-audit/archive-manifest.csv" | Where-Object {$_.Path.StartsWith($original+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)})
if($evidence.Count -lt 10000){throw 'Incomplete original archive inventory'}
$evidence | Export-Csv "$OutDir/original-hashes.csv" -NoTypeInformation
Assert-Hashes "$OutDir/original-hashes.csv"
# Verify current reusable binaries/runtime/fixtures/source against frozen Study06 bytes.
$snapshots=@(Import-Csv "$original/input-snapshots.csv")
foreach($row in $snapshots){
 if((Get-FileHash -LiteralPath $row.snapshot).Hash -ne $row.hash){throw "Original snapshot changed: $($row.snapshot)"}
 # The live declaration was supplemented after Study06; its frozen snapshot is authoritative.
 if($row.source -ne "$repo\.claude\plans\beam-on-fresh-source-calibration-new-study.md" -and (Get-FileHash -LiteralPath $row.source).Hash -ne $row.hash){throw "Reuse compatibility mismatch: $($row.source)"}
}
if((Get-FileHash "$install/bin/G4processes_core.dll").Hash -ne '11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B'){throw 'Wrong private runtime'}
$snapshots | Export-Csv "$OutDir/reuse-snapshots.csv" -NoTypeInformation
# Check loaded-module provenance of every reused shared scientific history.
foreach($condition in 'water','oxygen'){for($r=0;$r -lt 128;$r++){foreach($rate in 100,1000){
 $stem="$original/$condition/pair$r-$rate";$core=$false
 foreach($line in Get-Content "$stem.modules"){$path=[IO.Path]::GetFullPath($line);if([IO.Path]::GetFileName($path) -like 'G4*.dll'){
  if([IO.Path]::GetDirectoryName($path) -ne "$install\bin"){throw 'Foreign shared runtime module'}
  if([IO.Path]::GetFileName($path) -eq 'G4processes_core.dll'){$core=$true}
 }}
 if(!$core){throw 'Missing shared core module'}
}}}
& "$BuildDir/ArrivalAudit.exe" $original > "$OutDir/arrival-audit.txt" 2>&1
if($LASTEXITCODE -ne 0){throw 'Arrival reconstruction failed; chemistry blocked, no reroll'}
. "$probe/CalibratedPriorEvidence.ps1"
$prior=@(Get-CalibratedPriorEvidence -Root "$repo/.scratch/tests" -ActiveOutput $OutDir | Sort-Object FullName -Unique)
$prior | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/prior-input-hashes.csv" -NoTypeInformation
$reserved=[Collections.Generic.HashSet[long]]::new()
foreach($file in $prior){foreach($match in [regex]::Matches([IO.File]::ReadAllText($file.FullName),'(?<![\w.])\d+(?![\w.])')){$v=0L;if([long]::TryParse($match.Value,[ref]$v) -and $v -gt 0 -and $v -le 2147483646){[void]$reserved.Add($v)}}}
# The original reservation file additionally includes prior unused domains.
foreach($value in Get-Content "$original/prior-seeds.txt"){[void]$reserved.Add([long]$value)}
foreach($row in Import-Csv "$original/streams.tsv" -Delimiter "`t"){if(!$reserved.Contains([long]$row.seed)){throw 'Missing Study06 reserved slot'}}
@($reserved | Sort-Object) | Set-Content "$OutDir/prior-seeds.txt"
& "$BuildDir/MatchedStudy.exe" plan $original $OutDir $probe > "$OutDir/plan.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Scientific plan validation failed'}
$jobs=@(Import-Csv "$OutDir/jobs.tsv" -Delimiter "`t")
if(@($jobs|Where-Object kind -eq science).Count -ne 682 -or @($jobs|Where-Object kind -eq control).Count -ne 24){throw 'Incorrect native job caps'}
$seen=[Collections.Generic.HashSet[long]]::new()
foreach($row in Import-Csv "$OutDir/streams.tsv" -Delimiter "`t"){if($reserved.Contains([long]$row.seed) -or !$seen.Add([long]$row.seed)){throw 'Fresh stream collision'}}
# Four intentional historical same-seed controls and four late-repeat controls are
# harness-only. Every job nevertheless reserves an unused slot; science never reuses.
$nativeExe="$native/BeamOnHistory.exe"
@{original=$original;install=$install;launcher=$launcher;nativeExe=$nativeExe;checker="$BuildDir/MatchedStudy.exe";root=$OutDir} | ConvertTo-Json | Set-Content "$OutDir/runtime.json"
Copy-Item "$repo/docs/dose-rate/beam-on-intertrack-design.md" "$OutDir/approved-design.md"
Copy-Item "$PSScriptRoot/README.md" "$OutDir/protocol.md"
$inputs=@(Get-ChildItem $PSScriptRoot -File)+@(Get-Item "$BuildDir/MatchedStudy.exe","$BuildDir/ArrivalAudit.exe",$nativeExe,$launcher)+@(Get-ChildItem "$install/bin/G4*.dll")
$inputs+=@($snapshots|ForEach-Object{Get-Item -LiteralPath $_.source})
$inputs | Sort-Object FullName -Unique | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/input-hashes.csv" -NoTypeInformation
$owned=@(Get-ChildItem $OutDir -File -Recurse)
$owned | Get-FileHash | Select-Object Path,Hash | Export-Csv "$OutDir/frozen-hashes.csv" -NoTypeInformation
# Seal hashes of the manifest and ownership table. Execution verifies all entries.
Get-FileHash "$OutDir/frozen-hashes.csv" | Select-Object Path,Hash | Export-Csv "$OutDir/seal.csv" -NoTypeInformation
Write-Output "PREPARED_NO_CHEMISTRY $OutDir"

