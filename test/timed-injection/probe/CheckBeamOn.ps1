param(
  [Parameter(Mandatory=$true)][string]$Exe,
  [Parameter(Mandatory=$true)][string]$Launcher,
  [Parameter(Mandatory=$true)][string]$InstallPrefix,
  [Parameter(Mandatory=$true)][string]$Library,
  [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
if (Test-Path $OutDir) { throw 'Use a fresh evidence directory.' }
New-Item -ItemType Directory $OutDir | Out-Null
$OutDir=(Resolve-Path $OutDir).Path
$Exe=(Resolve-Path $Exe).Path; $Launcher=(Resolve-Path $Launcher).Path
$InstallPrefix=(Resolve-Path $InstallPrefix).Path; $Library=(Resolve-Path $Library).Path
$env:PATH="$InstallPrefix/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$paths=foreach($i in 0..1) { '"' + (Resolve-Path "$Library/library$i.packet").Path.Replace('\','/') + '"' }
Set-Content "$OutDir/packets.manifest" $paths
function Check-Runtime($prefix) {
  $modules=@(Get-Content "$prefix.modules" | Where-Object {[IO.Path]::GetFileName($_) -like 'G4*.dll'})
  if(!@($modules | Where-Object {[IO.Path]::GetFileName($_) -eq 'G4processes_core.dll'}).Count) { throw 'missing core DLL' }
  foreach($module in $modules) { if(!([IO.Path]::GetDirectoryName($module).Equals("$InstallPrefix\bin",[StringComparison]::OrdinalIgnoreCase))) { throw 'foreign DLL' } }
}
$cases=@(
  @{Name='empty'; Lines=@(); Count=0},
  @{Name='single'; Lines=@('0 0'); Count=1},
  @{Name='simultaneous'; Lines=@('0 0','0 0'); Count=2},
  @{Name='close'; Lines=@('0 0','0.1 1'); Count=2},
  @{Name='delayed'; Lines=@('0 0','8 1'); Count=2},
  @{Name='boundary'; Lines=@('9.5 0'); Count=1}
)
foreach($case in $cases) {
  $prefix="$OutDir/$($case.Name)"; [IO.File]::WriteAllLines("$prefix.schedule", [string[]]$case.Lines)
  & $Launcher 512 60 $prefix $Exe "$PSScriptRoot/MesoFixture4um.in" "$OutDir/packets.manifest" "$prefix.history" 10 12 1729 1733 1741 fixed "$prefix.schedule" 8
  if($LASTEXITCODE) { throw "launcher failed: $($case.Name)" }
  $meta=Get-Content "$prefix.meta" -Raw
  $log=Get-Content "$prefix.log" -Raw; $err=Get-Content "$prefix.err" -Raw
  if($meta -notmatch '(?m)^reason complete\r?$' -or $meta -notmatch '(?m)^exit_code 0\r?$' -or $meta -notmatch '(?m)^cap_notification 0\r?$' -or $log -notmatch 'BEAM_ON_HISTORY_PASS' -or ($log+$err) -match 'EEEE|FatalException') { throw "history failed: $($case.Name)" }
  Check-Runtime $prefix
  $history=Get-Content "$prefix.history"
  if(!($history -contains "packets $($case.Count)")) { throw 'source count mismatch' }
  $expected=@{}; $observed=@{}
  foreach($line in $history) {
    if($line -match '^expected_activation (\d+) source (\d+) time_ps (\S+)$') { $expected[$Matches[1]]=[double]$Matches[3] }
    if($line -match '^observed_activation (\d+) time_ps (\S+)$') { $observed[$Matches[1]]=[double]$Matches[2] }
  }
  if($expected.Count -ne $observed.Count) { throw 'activation count mismatch' }
  foreach($id in $expected.Keys) { if(!$observed.ContainsKey($id) -or $observed[$id] -ne $expected[$id]) { throw "wrong activation $id" } }
  if($case.Name -eq 'boundary' -and !@($expected.Values | Where-Object {$_ -gt 10}).Count) { throw 'post-beam activation absent' }
  if($case.Name -eq 'empty') {
    if(!($history -contains 'observation_ps 12 live 0') -or !($history -match '^species .+ 0$')) { throw 'empty population missing' }
    $before=($history | Where-Object {$_ -match '^bulk_before '}) -replace '^bulk_before ',''
    $after=($history | Where-Object {$_ -match '^bulk_after '}) -replace '^bulk_after ',''
    if($before -eq '' -or $before -ne $after) { throw 'empty bulk changed' }
  }
  Write-Host "PASS $($case.Name)"
}
foreach($case in @(
  @{Name='unsorted'; Lines=@('8 0','1 1'); Macro="$PSScriptRoot/MesoFixture4um.in"; Endpoint=12; Error='invalid schedule record'},
  @{Name='out-of-window'; Lines=@('10 0'); Macro="$PSScriptRoot/MesoFixture4um.in"; Endpoint=12; Error='invalid schedule record'},
  @{Name='excluded-activation'; Lines=@('9.5 0'); Macro="$PSScriptRoot/MesoFixture4um.in"; Endpoint=10.25; Error='endpoint excludes activation'},
  @{Name='incompatible'; Lines=@('0 0'); Macro="$PSScriptRoot/MesoFixture4umOxygen.in"; Endpoint=12; Error='invalid packet'}
)) {
  $prefix="$OutDir/reject-$($case.Name)"; [IO.File]::WriteAllLines("$prefix.schedule", [string[]]$case.Lines)
  & $Launcher 512 60 $prefix $Exe $case.Macro "$OutDir/packets.manifest" "$prefix.history" 10 $case.Endpoint 1729 1733 1741 fixed "$prefix.schedule" 8
  if($LASTEXITCODE) { throw "launcher failed: $($case.Name)" }
  $meta=Get-Content "$prefix.meta" -Raw; $err=Get-Content "$prefix.err" -Raw
  if($meta -notmatch '(?m)^reason complete\r?$' -or $meta -notmatch '(?m)^exit_code 2\r?$' -or $meta -notmatch '(?m)^cap_notification 0\r?$' -or !$err.Contains($case.Error)) { throw "expected rejection failed: $($case.Name)" }
  Check-Runtime $prefix
  Write-Host "PASS rejection $($case.Name)"
}
$streamCases=@(
  @{Name='stream-a'; Chemistry=1729; Selection=1741},
  @{Name='stream-chemistry'; Chemistry=1799; Selection=1741},
  @{Name='stream-selection'; Chemistry=1729; Selection=1777}
)
foreach($case in $streamCases) {
  $prefix="$OutDir/$($case.Name)"
  & $Launcher 512 60 $prefix $Exe "$PSScriptRoot/MesoFixture4um.in" "$OutDir/packets.manifest" "$prefix.history" 10 12 $case.Chemistry 1733 $case.Selection poisson 4e8 8
  if($LASTEXITCODE) { throw "launcher failed: $($case.Name)" }
  $meta=Get-Content "$prefix.meta" -Raw; $log=Get-Content "$prefix.log" -Raw
  if($meta -notmatch '(?m)^reason complete\r?$' -or $meta -notmatch '(?m)^exit_code 0\r?$' -or $meta -notmatch '(?m)^cap_notification 0\r?$' -or $log -notmatch 'BEAM_ON_HISTORY_PASS') { throw "stream control failed: $($case.Name)" }
  Check-Runtime $prefix
}
$a=@(Get-Content "$OutDir/stream-a.history" | Where-Object {$_ -match '^arrival '})
$b=@(Get-Content "$OutDir/stream-chemistry.history" | Where-Object {$_ -match '^arrival '})
$c=@(Get-Content "$OutDir/stream-selection.history" | Where-Object {$_ -match '^arrival '})
if(!$a.Count -or (Compare-Object $a $b -SyncWindow 0)) { throw 'chemistry seed changed source records or source was empty' }
$timesA=@($a | ForEach-Object { ($_ -split ' ')[3] }); $timesC=@($c | ForEach-Object { ($_ -split ' ')[3] })
if($timesA.Count -ne $timesC.Count -or (Compare-Object $timesA $timesC -SyncWindow 0)) { throw 'selection seed changed source times' }
Write-Host 'PASS source stream invariance'
Write-Output 'BEAM_ON_CHECKS_PASS'
