param([Parameter(Mandatory=$true)][string]$BuildDir,[Parameter(Mandatory=$true)][string]$PrivateInstall,[Parameter(Mandatory=$true)][string]$LibraryRoot,[Parameter(Mandatory=$true)][string]$JobLauncher,[Parameter(Mandatory=$true)][string]$OutDir)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir){throw 'Output directory must be new'}
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
$exe=(Resolve-Path "$BuildDir/RetentionGate.exe").Path
$install=(Resolve-Path $PrivateInstall).Path
$env:PATH="$install/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$inputs=@($exe)+@(Get-ChildItem "$install/bin/G4*.dll" | ForEach-Object FullName)+@(Get-ChildItem "$LibraryRoot/library4um-16/*.packet","$LibraryRoot/library4um-oxygen-8/*.packet" | ForEach-Object FullName)+@((Resolve-Path "$PSScriptRoot/RetentionGate.cc").Path,(Resolve-Path "$PSScriptRoot/MesoFixture4um.in").Path,(Resolve-Path "$PSScriptRoot/MesoFixture4umOxygen.in").Path)
$before=@($inputs | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash)
$before | Export-Csv "$OutDir/provenance-before.csv" -NoTypeInformation
foreach($oxygen in 0,1){
 $library=if($oxygen){"$LibraryRoot/library4um-oxygen-8"}else{"$LibraryRoot/library4um-16"}
 $macro=if($oxygen){"$PSScriptRoot/MesoFixture4umOxygen.in"}else{"$PSScriptRoot/MesoFixture4um.in"}
 foreach($time in 50,100,150){foreach($pair in 0..7){foreach($replica in 0..1){foreach($mode in 0..2){
  $prefix="$OutDir/o${oxygen}t${time}p${pair}r${replica}m${mode}"
  & $JobLauncher 256 10 $prefix $exe $macro "$library/library$pair.packet" "$library/holdout$pair.packet" "$prefix.txt" (91001+101*$pair+1009*$replica) $mode 1.5 $time
  if($LASTEXITCODE -ne 0){throw "Launcher failed $prefix"}
  $meta=Get-Content "$prefix.meta" -Raw
  if($meta -notmatch 'reason complete\r?\nexit_code 0\r?\ncap_notification 0'){throw "Failed capped job $prefix"}
  if((Get-Content "$prefix.log" -Raw) -notmatch 'RETENTION_GATE_PASS'){throw "Missing success $prefix"}
  $modules=@(Get-Content "$prefix.modules" | Where-Object {$_ -match '[\\/]G4[^\\/]*\.dll$'})
  if(!$modules.Count -or !($modules | Where-Object {$_ -match 'G4processes_core.dll'})){throw "Missing DLL inventory $prefix"}
  foreach($module in $modules){if(!$module.StartsWith("$install\bin\",[System.StringComparison]::OrdinalIgnoreCase)){throw "Wrong DLL $module"}}
 }}}}
}
$after=@($inputs | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash)
$after | Export-Csv "$OutDir/provenance-after.csv" -NoTypeInformation
if(Compare-Object $before $after -Property Path,Hash){throw 'Runtime/input changed'}
'RETAINED_NATIVE_288_COMPLETE' | Set-Content "$OutDir/completion.txt"

