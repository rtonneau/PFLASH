param(
 [Parameter(Mandatory=$true)][string]$BuildDir,
 [Parameter(Mandatory=$true)][string]$FinderBuildDir,
 [Parameter(Mandatory=$true)][string]$InstallPrefix,
 [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
if(Test-Path $OutDir) { throw 'Use a fresh evidence directory.' }
$BuildDir=(Resolve-Path $BuildDir).Path; $FinderBuildDir=(Resolve-Path $FinderBuildDir).Path
$InstallPrefix=(Resolve-Path $InstallPrefix).Path
New-Item -ItemType Directory $OutDir | Out-Null; $OutDir=(Resolve-Path $OutDir).Path
$env:PATH="$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$launcher="$FinderBuildDir/JobLauncher.exe"
function Invoke-Control($name,$exe,[string[]]$arguments,$marker) {
 $prefix="$OutDir/$name"
 & $launcher 512 60 $prefix $exe @arguments
 if($LASTEXITCODE) { throw "launcher failed: $name" }
 $meta=Get-Content "$prefix.meta" -Raw; $log=Get-Content "$prefix.log" -Raw; $err=Get-Content "$prefix.err" -Raw
 if($meta -notmatch '(?m)^reason complete\r?$' -or $meta -notmatch '(?m)^exit_code 0\r?$' -or $meta -notmatch '(?m)^cap_notification 0\r?$' -or $log -notmatch $marker -or ($log+$err) -match 'EEEE|FatalException') { throw "control failed: $name" }
 $modules=@(Get-Content "$prefix.modules" | Where-Object {[IO.Path]::GetFileName($_) -like 'G4*.dll'})
 if(!@($modules | Where-Object {[IO.Path]::GetFileName($_) -eq 'G4processes_core.dll'}).Count) { throw "missing core DLL: $name" }
 foreach($module in $modules) { if(!([IO.Path]::GetDirectoryName($module).Equals("$InstallPrefix\bin",[StringComparison]::OrdinalIgnoreCase))) { throw "foreign DLL: $module" } }
 Write-Host "PASS $name"
 return $log
}
foreach($fixture in @('simultaneous','shifted','delayed','nonreacting','newold','interrupted','empty','union','bulk','two','nearend','coincident')) {
 $macro=if($fixture -eq 'bulk'){"$BuildDir/BulkControl.in"}else{"$PSScriptRoot/Capture.in"}
 foreach($seed in @(1729,1733,1741)) {
  $null=Invoke-Control "source-$fixture-$seed" "$BuildDir/InjectionReference.exe" @($macro,$fixture,"$seed",'patched') 'INJECTION_CONTROL_PASS'
 }
}
$null=Invoke-Control 'reflection-unit' "$BuildDir/MesoBoundaryTest.exe" @() 'BOUNDARY_REFLECTION_PASS'
foreach($mode in @('transport','dissociation')) {
 foreach($axis in 0..2) {
  foreach($side in @(-1,1)) {
   $seen=$false
   foreach($seed in 1729..1792) {
    $log=Invoke-Control "edge-$mode-$axis-$side-$seed" "$BuildDir/MesoBoundaryIntegration.exe" @("$PSScriptRoot/MesoFixture4um.in",$mode,"$axis","$side","$seed") 'EDGE_PASS'
    if($log -notmatch 'EDGE_CROSS (\d+) (\d+) (\d+)') { throw 'missing edge activity' }
    if(($mode -eq 'transport' -and [int]$Matches[1] -gt 0) -or ($mode -eq 'dissociation' -and [int]$Matches[3] -gt 0)) { $seen=$true; break }
   }
   if(!$seen) { throw "no observed boundary activity: $mode $axis $side" }
  }
 }
}
Write-Output 'BEAM_ON_RUNTIME_CONTROLS_PASS'
