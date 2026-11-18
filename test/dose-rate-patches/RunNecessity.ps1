#requires -Version 7
<#
.SYNOPSIS
  Leave-one-out necessity runs: dose-rate-study histories on the 'all' kernel and on
  each kernel missing one patch (all-1, all-2, all-3), then analyze_necessity.py.

.DESCRIPTION
  Settings: necessity/necessity.json. Steps (each skipped with -Reuse when its output exists):
    1. build examples/dose-rate-study against every variant kernel
       (.scratch/kernels/<variant>/install) into .scratch/dose-rate-patches/drs-<variant>;
    2. one capture library on 'all';
    3. one plan (studies/make_configs.py): the same configs, seeds and packets for every variant;
    4. every history of the plan on every variant, under JobLauncher (memory cap, wall cap);
    5. necessity/analyze_necessity.py writes necessity.json results and prints the verdicts.
  Work directory: .scratch/dose-rate-patches/necessity. The kernels are built by
  BuildKernels.ps1. The shared Geant4 install is never used here.
  Exit status: the analysis status (0 = every demonstration holds).

.PARAMETER Config    named study config of necessity.json (default: its defaultConfig)
.PARAMETER Reuse     keep existing builds, library, plan and finished runs
.PARAMETER Parallel  histories at once (default: necessity.json parallel)
.PARAMETER Variants  subset of the variants to run (analysis still reads all)
.PARAMETER Python    interpreter with numpy and h5py (default python; e.g. the GEANT4 conda env)
#>
[CmdletBinding()]
param([string]$Config, [switch]$Reuse, [int]$Parallel = 0, [string[]]$Variants, [string]$Python = 'python')
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path "$PSScriptRoot/../..").Path.Replace('\', '/')
$settings = Get-Content "$PSScriptRoot/necessity/necessity.json" -Raw | ConvertFrom-Json
if (-not $Variants) { $Variants = $settings.variants }
if ($Parallel -le 0) { $Parallel = $settings.parallel }
if (-not $Config) { $Config = $settings.defaultConfig }
$study = $settings.configs.$Config
if (-not $study) { throw "no config '$Config' in necessity.json" }
if (-not $env:DEV_DIR) { throw 'DEV_DIR is unset: run ~/dotfiles/scripts/windows/setup-dev-env.ps1 and restart the shell' }
$scratch = "$root/.scratch/dose-rate-patches"
$work = "$scratch/necessity"
New-Item -ItemType Directory -Force $work | Out-Null
$drs = "$root/examples/dose-rate-study"
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = "$vs\VC\Auxiliary\Build\vcvars64.bat"
$toolchain = "$env:DEV_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake".Replace('\', '/')

function Invoke-Msvc([string]$command, [string]$log) {
  cmd /c "`"$vcvars`" >nul 2>&1 && ($command) > `"$log`" 2>&1"
  return $LASTEXITCODE
}
function Get-KernelBin([string]$variant) { "$root/.scratch/kernels/$variant/install/bin" }

# 1. Builds.
foreach ($v in ($settings.variants | Select-Object -Unique)) {
  $b = "$scratch/drs-$v"
  if ($Reuse -and (Test-Path "$b/history.exe")) { continue }
  $kernel = "$root/.scratch/kernels/$v/install/lib/cmake/Geant4"
  if (-not (Test-Path $kernel)) { throw "kernel $v missing ($kernel): run BuildKernels.ps1" }
  Write-Output "build drs-$v"
  $rc = Invoke-Msvc "cmake -S $drs -B $b -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=$kernel -DCMAKE_TOOLCHAIN_FILE=$toolchain && cmake --build $b" "$b.build.log"
  if ($rc -ne 0) { throw "build drs-$v failed, see $b.build.log" }
}
# The launcher (stand-alone Win32, no Geant4).
$launcherBuild = "$scratch/launcher"
if (-not ($Reuse -and (Test-Path "$launcherBuild/JobLauncher.exe"))) {
  New-Item -ItemType Directory -Force $launcherBuild | Out-Null
  $rc = Invoke-Msvc "cl /nologo /EHsc /O2 /std:c++20 /Fe:$launcherBuild/JobLauncher.exe /Fo:$launcherBuild/ $root/test/dose-rate-patches/JobLauncher.cc psapi.lib" "$launcherBuild/build.log"
  if ($rc -ne 0) { throw "JobLauncher build failed, see $launcherBuild/build.log" }
}
$launcher = "$launcherBuild/JobLauncher.exe"

# 2. Capture library on the reference kernel.
$reference = $settings.reference
$library = "$work/library"
if (-not ($Reuse -and (Test-Path "$library/library.csv"))) {
  if (Test-Path $library) { Remove-Item -Recurse -Force $library }
  Write-Output "capture $($settings.capture.count) packets on $reference"
  $c = $settings.capture
  $rc = Invoke-Msvc "set `"PATH=$(Get-KernelBin $reference);%PATH%`" && $scratch/drs-$reference/capture.exe --macro $drs/macros/capture.mac --output $library --count $($c.count) --first-seed $($c.firstSeed) --max-empty $($c.maxEmpty)" "$work/capture.log"
  if ($rc -ne 0) { throw "capture failed, see $work/capture.log" }
}

# 3. Plan (shared by every variant).
$planDir = "$work/$Config/plan"
if (-not ($Reuse -and (Test-Path "$planDir/plan.json"))) {
  if (Test-Path $planDir) { Remove-Item -Recurse -Force $planDir }
  New-Item -ItemType Directory -Force "$work/$Config" | Out-Null
  $study | ConvertTo-Json -Depth 5 | Set-Content "$work/$Config/study.json"
  $snap = if ($settings.snapshots) { '--snapshots' } else { '' }
  & $Python "$drs/studies/make_configs.py" --library $library --study "$work/$Config/study.json" --out $planDir $snap
  if ($LASTEXITCODE -ne 0) { throw 'make_configs.py failed' }
}
$plan = Get-Content "$planDir/plan.json" -Raw | ConvertFrom-Json

# 4. Histories: every run on every variant, under the memory and wall caps.
$jobs = foreach ($v in $Variants) {
  foreach ($r in $plan.runs) {
    $out = "$work/$Config/runs/$v/$($r.id)"
    $meta = "$out.launcher.txt"
    if ($Reuse -and (Test-Path $meta) -and (Select-String -Path $meta -Pattern '^RESULT launcher.reason=' -Quiet)) { continue }
    [pscustomobject]@{ Variant = $v; Id = $r.id; Config = (Join-Path $planDir $r.config).Replace('\', '/'); Out = $out; Meta = $meta }
  }
}
Write-Output "histories to run: $(@($jobs).Count) (parallel $Parallel)"
$cap = $settings.memoryCapMiB
$wall = $study.wallTimeCapS
$jobs | ForEach-Object -ThrottleLimit $Parallel -Parallel {
  $j = $_
  foreach ($p in @($j.Out, "$($j.Out).log", "$($j.Out).err", $j.Meta)) { if (Test-Path $p) { Remove-Item -Recurse -Force $p } }
  New-Item -ItemType Directory -Force (Split-Path $j.Out) | Out-Null
  $bin = "$using:root/.scratch/kernels/$($j.Variant)/install/bin"
  $history = "$using:scratch/drs-$($j.Variant)/history.exe"
  $start = Get-Date
  cmd /c "set `"PATH=$bin;%PATH%`" && `"$using:launcher`" $using:cap $using:wall `"$($j.Out)`" --expect fixed `"$history`" --config `"$($j.Config)`" --output `"$($j.Out)`" > `"$($j.Meta)`" 2>&1"
  $reason = (Select-String -Path $j.Meta -Pattern '^RESULT launcher.reason=(.*)$').Matches.Groups[1].Value
  '{0,-6} {1,-10} {2,-11} exit {3,-4} {4:n0} s' -f $j.Variant, $j.Id, $reason, $LASTEXITCODE, ((Get-Date) - $start).TotalSeconds
}

# 5. Analysis.
& $Python "$PSScriptRoot/necessity/analyze_necessity.py" --settings "$PSScriptRoot/necessity/necessity.json" --config $Config --work "$work/$Config" --out "$work/$Config/necessity-results.json"
exit $LASTEXITCODE
