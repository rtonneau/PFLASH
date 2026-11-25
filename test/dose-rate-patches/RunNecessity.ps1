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
.PARAMETER Study     Study id or unique slug of the /g4-study skill: launch every history as a Study run
                     (g4-study.ps1 run <study> <runSet> --id <variant>-<run id> --target history ... -- JobLauncher ... history),
                     so each run has a provenance stamp under ${STUDIES_ROOT}. The JobLauncher memory and
                     wall caps still apply. Output: <run set>/<variant>-<run id>/{stamp.json,stdout.log,output/};
                     the analysis reads it through links in the work directory. Without -Study nothing changes.
.PARAMETER RunSet    run set of the Study (default: the config name). A config's runSets.<RunSet> entry
                     overrides its study keys (rates, realizations) and gets its own work directory
                     (<config>-<RunSet>), e.g. smoke = one realization at 1e8 Gy/s.
.PARAMETER StudyScript  g4-study.ps1 (default ~/.claude/skills/g4-study/scripts/g4-study.ps1)
.PARAMETER Orket     with -Study: submit every history to orket (g4-study.ps1 submit-run, same arguments and
                     JobLauncher caps) instead of running it, then stop: no links, no analysis. A run already
                     submitted or succeeded is skipped. The orket jobs run in the repo root. Once they finish,
                     g4-study.ps1 sync <study> completes their stamps; rerun with -Study -Reuse (no -Orket)
                     to link the outputs and analyze.
#>
[CmdletBinding()]
param([string]$Config, [switch]$Reuse, [int]$Parallel = 0, [string[]]$Variants, [string]$Python = 'python',
  [string]$Study, [string]$RunSet, [string]$StudyScript = (Join-Path $HOME '.claude\skills\g4-study\scripts\g4-study.ps1'),
  [switch]$Orket)
$ErrorActionPreference = 'Stop'
if ($Orket -and -not $Study) { throw '-Orket needs -Study: orket jobs are Study runs (g4-study.ps1 submit-run)' }

$root = (Resolve-Path "$PSScriptRoot/../..").Path.Replace('\', '/')
$settings = Get-Content "$PSScriptRoot/necessity/necessity.json" -Raw | ConvertFrom-Json
if (-not $Config) { $Config = $settings.defaultConfig }
$sweep = $settings.configs.$Config
if (-not $sweep) { throw "no config '$Config' in necessity.json" }
# A config may name its own variants (rateSweep: all, all-2); -Variants wins. 'all,all-2' passed as one
# token by `pwsh -File` is split here.
if (-not $Variants) { $Variants = if ($sweep.variants) { @($sweep.variants) } else { $settings.variants } }
$Variants = @($Variants | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($Parallel -le 0) { $Parallel = $settings.parallel }
# The run set's overrides (runSets.<name>) replace study keys; the plan then has its own directory.
$workName = $Config
if ($Study -and -not $RunSet) { $RunSet = $Config }
if ($RunSet -and $sweep.runSets -and $sweep.runSets.$RunSet) {
  $merged = [ordered]@{}
  foreach ($p in $sweep.PSObject.Properties) { $merged[$p.Name] = $p.Value }
  foreach ($p in $sweep.runSets.$RunSet.PSObject.Properties) { $merged[$p.Name] = $p.Value }
  $sweep = [pscustomobject]$merged
  $workName = "$Config-$RunSet"
}
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
$buildVariants = if ($PSBoundParameters.ContainsKey('Variants') -or $sweep.variants) { @($Variants + $settings.reference) } else { $settings.variants }
foreach ($v in ($buildVariants | Select-Object -Unique)) {
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
$planDir = "$work/$workName/plan"
if (-not ($Reuse -and (Test-Path "$planDir/plan.json"))) {
  if (Test-Path $planDir) { Remove-Item -Recurse -Force $planDir }
  New-Item -ItemType Directory -Force "$work/$workName" | Out-Null
  $sweep | Select-Object -Property * -ExcludeProperty comment, variants, runSets | ConvertTo-Json -Depth 5 | Set-Content "$work/$workName/study.json"
  $snap = if ($settings.snapshots) { '--snapshots' } else { '' }
  & $Python "$drs/studies/make_configs.py" --library $library --study "$work/$workName/study.json" --out $planDir $snap
  if ($LASTEXITCODE -ne 0) { throw 'make_configs.py failed' }
}
$plan = Get-Content "$planDir/plan.json" -Raw | ConvertFrom-Json

# 4. Histories: every run on every variant, under the memory and wall caps.
# Study run ids drop the '+' of ids like r1e+08_000 (a run id allows letters, digits, '.', '_', '-').
# With -Study each history is a Study run (g4-study.ps1 run) whose child is JobLauncher, so the caps stay;
# the wrapper stamps the run under ${STUDIES_ROOT}. The stamp's executable is the launcher and its target
# (--target) is the history executable: both are snapshotted into <study>/bin/ and hashed, the launcher
# runs the history snapshot, and the Geant4 install is looked up from the history (the variant kernel,
# which must carry a g4-build-info.json marker: g4-study.ps1 mark). The config is copied to input/.
$runSetDir = $null
if ($Study) {
  if ($RunSet -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw "run set '$RunSet' must use letters, digits, '.', '_' or '-' (pass -RunSet)" }
  if (-not (Test-Path -LiteralPath $StudyScript)) { throw "g4-study.ps1 not found: $StudyScript (install the /g4-study skill with claude-sync-skills.ps1)" }
  $statusOut = & pwsh -NoProfile -File $StudyScript status $Study --json --project-root $root 2>&1
  if ($LASTEXITCODE -ne 0) { throw "study '$Study' not resolved: $(($statusOut | Out-String).Trim())" }
  $info = ($statusOut | Out-String) | ConvertFrom-Json
  $Study = $info.study.id
  $dataPath = [string]$info.study.dataPath
  foreach ($v in 'STUDIES_ROOT', 'DEV_DIR', 'G4_ROOT') { $dataPath = $dataPath.Replace('${' + $v + '}', [string][Environment]::GetEnvironmentVariable($v)) }
  $runSetDir = [IO.Path]::GetFullPath((Join-Path (Join-Path $dataPath 'runs') $RunSet))
  Write-Output "Study $Study, run set $runSetDir"
}
$cap = $settings.memoryCapMiB
$wall = $sweep.wallTimeCapS
$verb = if ($Orket) { 'submit-run' } else { 'run' }
$jobs = foreach ($v in $Variants) {
  foreach ($r in $plan.runs) {
    $out = "$work/$workName/runs/$v/$($r.id)"
    $cfg = (Join-Path $planDir $r.config).Replace('\', '/')
    if ($Study) {
      $runDir = Join-Path $runSetDir "$v-$($r.id.Replace('+', ''))"
      $log = Join-Path $runDir 'stdout.log'
      # A succeeded Study run is a provenance record: never rerun (the wrapper refuses it too); a run
      # handed to orket is not rerun before g4-study sync has completed its stamp.
      $stampPath = Join-Path $runDir 'stamp.json'
      if ((Test-Path $stampPath) -and ((Get-Content $stampPath -Raw | ConvertFrom-Json).status -in 'succeeded', 'submitted')) { continue }
      if ($Reuse -and (Test-Path $log) -and (Select-String -Path $log -Pattern '^RESULT launcher.reason=' -Quiet)) { continue }
      $a = @('-NoProfile', '-File', $StudyScript, $verb, $Study, $RunSet, '--id', "$v-$($r.id.Replace('+', ''))", '--project-root', $root)
      if (-not $Orket) { $a += '--quiet' }
      if (Test-Path $runDir) { $a += '--rerun' }
      foreach ($k in 'chemistrySeed', 'sourceSeed') { if ($r.PSObject.Properties[$k]) { $a += @('--seed', "$k=$($r.$k)") } }
      $a += @('--param', "rate_Gy_s=$(([double]$r.rate_Gy_s).ToString('R', [Globalization.CultureInfo]::InvariantCulture))", '--param', "variant=$v")
      if ($null -ne $r.firstPacket) { $a += @('--param', "firstPacket=$($r.firstPacket)") }
      $history = "$scratch/drs-$v/history.exe"
      $a += @('--target', $history, '--', $launcher, "$cap", "$wall", '{run}/launcher', '--expect', 'fixed', $history, '--config', $cfg, '--output', '{output}')
      [pscustomobject]@{ Variant = $v; Id = $r.id; Config = $cfg; Out = $out; Meta = "$out.launcher.txt"; StudyArgs = $a; RunDir = $runDir }
    } else {
      $meta = "$out.launcher.txt"
      if ($Reuse -and (Test-Path $meta) -and (Select-String -Path $meta -Pattern '^RESULT launcher.reason=' -Quiet)) { continue }
      [pscustomobject]@{ Variant = $v; Id = $r.id; Config = $cfg; Out = $out; Meta = $meta; StudyArgs = $null; RunDir = $null }
    }
  }
}
if ($Orket) {
  # One submit-run per history, one after the other; nothing is waited for. The variant kernel's bin
  # goes first on the wrapper's PATH: the wrapper copies its DLLs into the history's Binary snapshot,
  # so the orket job (which starts with the orket daemon's PATH) loads that kernel.
  Write-Output "histories to submit: $(@($jobs).Count)"
  $failed = 0
  foreach ($j in $jobs) {
    $psi = [Diagnostics.ProcessStartInfo]::new('pwsh')
    foreach ($x in $j.StudyArgs) { $psi.ArgumentList.Add([string]$x) }
    $psi.UseShellExecute = $false
    $psi.WorkingDirectory = $root
    $psi.Environment['PATH'] = "$(Get-KernelBin $j.Variant);$($psi.Environment['PATH'])"
    $proc = [Diagnostics.Process]::Start($psi)
    $proc.WaitForExit()
    $state = if ($proc.ExitCode -eq 0) { 'submitted' } else { $failed++; 'FAILED' }
    '{0,-6} {1,-10} {2,-11} exit {3}' -f $j.Variant, $j.Id, $state, $proc.ExitCode
  }
  Write-Output "$(@($jobs).Count - $failed) submitted to orket, $failed failed"
  Write-Output "follow them with: pwsh -File `"$StudyScript`" status $Study --project-root `"$root`""
  Write-Output "once they finish: pwsh -File `"$StudyScript`" sync $Study --project-root `"$root`", then rerun with -Study -Reuse (no -Orket) to analyze"
  exit $(if ($failed -gt 0) { 1 } else { 0 })
}
Write-Output "histories to run: $(@($jobs).Count) (parallel $Parallel)"
$jobs | ForEach-Object -ThrottleLimit $Parallel -Parallel {
  $j = $_
  $bin = "$using:root/.scratch/kernels/$($j.Variant)/install/bin"
  $history = "$using:scratch/drs-$($j.Variant)/history.exe"
  $start = Get-Date
  if ($j.StudyArgs) {
    $psi = [Diagnostics.ProcessStartInfo]::new('pwsh')
    foreach ($x in $j.StudyArgs) { $psi.ArgumentList.Add([string]$x) }
    $psi.UseShellExecute = $false
    $psi.Environment['PATH'] = "$bin;$($psi.Environment['PATH'])"
    # Parallel starts are safe: the wrapper serializes its own start-up (Study and project locks).
    $proc = [Diagnostics.Process]::Start($psi)
    $proc.WaitForExit()
    $exitCode = $proc.ExitCode
    $m = Select-String -Path (Join-Path $j.RunDir 'stdout.log') -Pattern '^RESULT launcher.reason=(.*)$' -ErrorAction SilentlyContinue
    $reason = if ($m) { $m.Matches.Groups[1].Value } else { 'no-result' }
  } else {
    foreach ($p in @($j.Out, "$($j.Out).log", "$($j.Out).err", $j.Meta)) { if (Test-Path $p) { Remove-Item -Recurse -Force $p } }
    New-Item -ItemType Directory -Force (Split-Path $j.Out) | Out-Null
    cmd /c "set `"PATH=$bin;%PATH%`" && `"$using:launcher`" $using:cap $using:wall `"$($j.Out)`" --expect fixed `"$history`" --config `"$($j.Config)`" --output `"$($j.Out)`" > `"$($j.Meta)`" 2>&1"
    $reason = (Select-String -Path $j.Meta -Pattern '^RESULT launcher.reason=(.*)$').Matches.Groups[1].Value
  }
  '{0,-6} {1,-10} {2,-11} exit {3,-4} {4:n0} s' -f $j.Variant, $j.Id, $reason, $(if ($j.StudyArgs) { $exitCode } else { $LASTEXITCODE }), ((Get-Date) - $start).TotalSeconds
}
if ($Study) {
  # The analysis reads <work>/runs/<variant>/<id>/ and <id>.launcher.txt: link each Study run's output
  # there and copy the launcher's RESULT lines (the run's stdout.log).
  foreach ($v in $Variants) {
    foreach ($r in $plan.runs) {
      $runDir = Join-Path $runSetDir "$v-$($r.id.Replace('+', ''))"
      if (-not (Test-Path (Join-Path $runDir 'stdout.log'))) { continue }
      $link = "$work/$workName/runs/$v/$($r.id)"
      New-Item -ItemType Directory -Force (Split-Path $link) | Out-Null
      if (Test-Path $link) {
        if (-not ((Get-Item $link -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$link is a real directory (a run without -Study); remove it or use another config" }
        [IO.Directory]::Delete($link)
      }
      New-Item -ItemType Junction -Path $link -Target (Join-Path $runDir 'output') | Out-Null
      Copy-Item -LiteralPath (Join-Path $runDir 'stdout.log') -Destination "$link.launcher.txt" -Force
    }
  }
}

# 5. Analysis.
& $Python "$PSScriptRoot/necessity/analyze_necessity.py" --settings "$PSScriptRoot/necessity/necessity.json" --config $Config --work "$work/$workName" --out "$work/$workName/necessity-results.json"
exit $LASTEXITCODE
