param(
  [Parameter(Mandatory=$true)][string]$BuildDir,
  [Parameter(Mandatory=$true)][string]$InstallPrefix,
  [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
$BuildDir=(Resolve-Path $BuildDir).Path
$InstallPrefix=(Resolve-Path $InstallPrefix).Path
if(Test-Path $OutDir){throw 'Use a new evidence directory; never overwrite a previous run.'}
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
$repo=(Resolve-Path "$PSScriptRoot/../..").Path
if(!$env:DEV_DIR){throw 'DEV_DIR missing; run setup-dev-env.ps1 and restart the shell.'}
$env:PATH="$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$files=@(Get-ChildItem "$InstallPrefix/bin/G4*.dll")+@(Get-Item "$BuildDir/FinderOracle.exe","$BuildDir/NativeContacts.exe","$BuildDir/JobLauncher.exe")
$files|Get-FileHash|Select-Object Path,Hash|Export-Csv "$OutDir/hashes-before.csv" -NoTypeInformation
function Invoke-Capped([string]$Name,[string]$Exe,[string[]]$Arguments,[string]$Marker){
  $prefix="$OutDir/$Name"
  & "$BuildDir/JobLauncher.exe" 256 5 $prefix $Exe @Arguments
  if($LASTEXITCODE -ne 0){throw "Launcher failed: $Name"}
  $meta=Get-Content "$prefix.meta" -Raw
  if($meta -notmatch '(?m)^reason complete\r?$' -or $meta -notmatch '(?m)^exit_code 0\r?$' -or $meta -notmatch '(?m)^cap_notification 0\r?$'){throw "Capped test failed: $Name; see $prefix.meta"}
  if((Get-Content "$prefix.log" -Raw) -notmatch $Marker){throw "Missing success marker: $Name"}
  if(((Get-Content "$prefix.log" -Raw)+(Get-Content "$prefix.err" -Raw)) -match 'EEEE|FatalException'){throw "Kernel error in $Name"}
  $modules=@(Get-Content "$prefix.modules" | Where-Object { [IO.Path]::GetFileName($_) -like 'G4*.dll' })
  if(!($modules | Where-Object {[IO.Path]::GetFileName($_) -eq 'G4processes_core.dll'})){throw "Missing core module evidence: $Name"}
  foreach($module in $modules){if(!([IO.Path]::GetDirectoryName($module).Equals("$InstallPrefix\bin",[StringComparison]::OrdinalIgnoreCase))){throw "Foreign kernel module: $module"}}
  Write-Output "PASS $Name"
}
foreach($count in @(1,2,3,4,11,4096)){
  Invoke-Capped "coincident-$count" "$BuildDir/FinderOracle.exe" @('coincident',"$count") 'ORACLE_PASS'
}
foreach($case in @(@('boundary','216'),@('near','100'),@('adjacent','100'),@('line','1000'),@('random','1000'),@('microscopic','1000'),@('ordinary','1000'),@('multikey','100'))){
  Invoke-Capped "$($case[0])-$($case[1])" "$BuildDir/FinderOracle.exe" $case 'ORACLE_PASS'
}
foreach($seed in @(1729,1733,1741)){
  foreach($count in @(1,2,3,4)){
    foreach($separation in @('0','0.01','100')){
      Invoke-Capped "native-$count-$separation-$seed" "$BuildDir/NativeContacts.exe" @("$repo/test/molecular-finder/Contact.in","$count",$separation,"$seed") 'NATIVE_PASS'
    }
  }
  Invoke-Capped "native-hole-$seed" "$BuildDir/NativeContacts.exe" @("$repo/test/molecular-finder/Contact.in",'0','0',"$seed") 'NATIVE_PASS'
}
foreach($count in @(-1,-2)){
  Invoke-Capped "reaction-guard-$count" "$BuildDir/NativeContacts.exe" @("$repo/test/molecular-finder/Contact.in","$count",'0','1729') 'GUARD_PASS'
}
$files|Get-FileHash|Select-Object Path,Hash|Export-Csv "$OutDir/hashes-after.csv" -NoTypeInformation
if(Compare-Object (Import-Csv "$OutDir/hashes-before.csv") (Import-Csv "$OutDir/hashes-after.csv") -Property Path,Hash){throw 'Runtime changed during validation'}
Write-Output 'FINDER_VALIDATION_PASS 55 capped cases'
