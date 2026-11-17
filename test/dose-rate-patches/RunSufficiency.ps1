<#
.SYNOPSIS
  Sufficiency check of one dose-rate patch: run its fixture on 'stock' (defect expected)
  and on 'p<N>' (fixed expected).

.DESCRIPTION
  Configures and builds the suite (test/dose-rate-patches) once per kernel variant into
  .scratch/dose-rate-patches/suite-<variant>, runs the fixture's ctest, checks that the
  Geant4 DLL loaded by the fixture comes from that variant's install, and prints one
  summary line. Exit 0 only when the demonstration holds on both kernels.

  -Patch   1, 2 or 3.
  -Variant kernels to run on (default: stock and p<N>).
  -Json    write the per-variant results to this file.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][ValidateSet('1', '2', '3')][string]$Patch,
  [string[]]$Variant,
  [string]$Json
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content (Join-Path $root '.claude\.claude-project.json') -Raw | ConvertFrom-Json

function Expand-ProjectPath([string]$value) {
  foreach ($name in 'DEV_DIR', 'G4_ROOT') {
    $v = [Environment]::GetEnvironmentVariable($name)
    if ($value.Contains("`${$name}")) {
      if ([string]::IsNullOrEmpty($v)) { throw "$name is not set: run ~/dotfiles/scripts/windows/setup-dev-env.ps1 and restart the shell" }
      $value = $value.Replace("`${$name}", $v)
    }
  }
  return [IO.Path]::GetFullPath($value)
}

$fixtures = @{ '1' = 'FinderCoincidentFixture'; '2' = 'SourceContactFixture'; '3' = 'ReboundFoldFixture' }
$labels = @{ '1' = 'finder-coincident'; '2' = 'source-contact'; '3' = 'rebound-fold' }
$fixture = $fixtures[$Patch]
if (-not $Variant) { $Variant = @('stock', "p$Patch") }

$toolchain = (Expand-ProjectPath $project.build.toolchainFile).Replace('\', '/')
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw 'vcvars64.bat not found' }

function Get-KernelInstall([string]$v) {
  if ($v -eq 'stock') { return Expand-ProjectPath $project.geant4InstallPath }
  $i = Join-Path $root ".scratch\kernels\$v\install"
  if (-not (Test-Path (Join-Path $i 'kernel.json'))) { throw "Kernel $v not built: run BuildKernels.ps1 -Variant $v" }
  return $i
}

$results = [ordered]@{}
$allOk = $true
foreach ($v in $Variant) {
  $install = Get-KernelInstall $v
  $g4dir = (Join-Path $install 'lib\cmake\Geant4').Replace('\', '/')
  $build = Join-Path $root ".scratch\dose-rate-patches\suite-$v"
  New-Item -ItemType Directory -Force $build | Out-Null
  $log = Join-Path $build "$fixture.log"
  Remove-Item $log -ErrorAction SilentlyContinue  # never read a stale result
  $src = $PSScriptRoot.Replace('\', '/'); $b = $build.Replace('\', '/')
  $cmd = "`"$vcvars`" >nul 2>&1 && cmake -S `"$src`" -B `"$b`" -G Ninja -DCMAKE_BUILD_TYPE=Release " +
         "-DGeant4_DIR=`"$g4dir`" -DDRP_KERNEL_VARIANT=$v -DCMAKE_TOOLCHAIN_FILE=`"$toolchain`" >`"$b/configure.log`" 2>&1 " +
         "&& cmake --build `"$b`" >`"$b/build.log`" 2>&1 " +
         "&& ctest --test-dir `"$b`" -R `"^$fixture`$`" -V >`"$log`" 2>&1"
  cmd /c $cmd | Out-Null
  $exit = $LASTEXITCODE
  $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
  if (-not $text) { $text = (Get-Content "$build/build.log" -Raw -ErrorAction SilentlyContinue) }

  $values = [ordered]@{}
  foreach ($m in [regex]::Matches($text, 'RESULT [\w-]+\.([\w.]+)=(\S+)')) { $values[$m.Groups[1].Value] = $m.Groups[2].Value }
  $expect = ([regex]::Match($text, 'FIXTURE \S+ expect=(\w+)')).Groups[1].Value
  $verdict = ([regex]::Match($text, 'VERDICT \S+ \w+ (\w+)')).Groups[1].Value
  $module = ([regex]::Match($text, 'MODULE G4processes_core=(.+)')).Groups[1].Value.Trim()
  $moduleOk = $module -and ([IO.Path]::GetFullPath($module)).StartsWith([IO.Path]::GetFullPath((Join-Path $install 'bin')), 'OrdinalIgnoreCase')
  $wanted = if ($v -eq 'stock' -or -not ($v -match "^p$Patch$|^all$|^all-[^$Patch]$")) { 'defect' } else { 'fixed' }
  $ok = ($exit -eq 0) -and ($verdict -eq 'PASS') -and ($expect -eq $wanted) -and $moduleOk
  if (-not $ok) { $allOk = $false }
  $results[$v] = [ordered]@{ expect = $expect; verdict = $verdict; exit = $exit; module = $module; moduleOk = $moduleOk; values = $values; log = $log }
  Write-Verbose "${v}: exit=$exit expect=$expect verdict=$verdict module=$module"
}

function Format-Values($r) { ($r.values.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ', ' }
$parts = foreach ($v in $Variant) {
  $r = $results[$v]
  $state = if ($r.verdict -eq 'PASS') { if ($r.expect -eq 'defect') { 'defect observed' } else { 'fixed' } } else { "FAILED (expect $($r.expect), exit $($r.exit), module ok $($r.moduleOk))" }
  "$v $state ($(Format-Values $r))"
}
Write-Output ("$($labels[$Patch]): " + ($parts -join '; ') + " -> " + ($(if ($allOk) { 'PASS' } else { 'FAIL' })))
if ($Json) { [ordered]@{ patch = $Patch; fixture = $fixture; pass = $allOk; variants = $results } | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $Json }
exit ([int](-not $allOk))
