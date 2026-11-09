param([string]$Root,[string]$Out,[int]$Packet,[int]$Rep,[int]$Condition)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$Root/../../..").Path
$runtime="$Root/06-activation/install/bin"
$env:PATH="$runtime;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
$base="$Out/p${Packet}r${Rep}m${Condition}"
$other=if($Condition -eq 2){"library$Packet.packet"}else{"holdout$Packet.packet"}
$mode=if($Condition -eq 1){1}else{0}
$delay=if($Condition -eq 3){1.5}else{0}
$arguments=@("$repo/test/timed-injection/probe/Capture.in","$Root/01-msvc/validation/library$Packet.packet","$Root/01-msvc/validation/$other","$base.txt",(91001+$Packet*101+$Rep*1009),$mode,$delay)
$p=Start-Process -FilePath "$Root/06-activation/probe/CombinedHistory.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$base.log" -RedirectStandardError "$base.err"
$modules=@{}
 $timer=[Diagnostics.Stopwatch]::StartNew()
while(!$p.HasExited){if($timer.Elapsed.TotalSeconds -gt 180){Stop-Process -Id $p.Id -Force; throw "timeout $base after 180 seconds"};try{foreach($m in $p.Modules){if($m.ModuleName -like 'G4*.dll'){$modules[$m.ModuleName]=$m.FileName}}}catch{}; Start-Sleep -Milliseconds 100; $p.Refresh()}
$p.WaitForExit()
$modules.GetEnumerator()|Sort-Object Name|ForEach-Object {"$($_.Name) $($_.Value)"}|Set-Content "$base.modules.txt"
if($p.ExitCode -ne 0 -or $modules.Count -eq 0 -or !(Select-String -Path "$base.log" -Pattern 'COMBINED_PASS' -Quiet)){throw "failure $base exit $($p.ExitCode)"}
foreach($path in $modules.Values){if(!$path.StartsWith([IO.Path]::GetFullPath($runtime),[StringComparison]::OrdinalIgnoreCase)){throw "wrong DLL $path"}}


Set-Content "$base.ok" "validated"
