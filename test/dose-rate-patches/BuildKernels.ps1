<#
.SYNOPSIS
  Build, verify and reuse the private Geant4 kernel variants of the patch-necessity suite.

.DESCRIPTION
  Each variant (kernels.json) is the pristine Geant4 source of .claude/.claude-project.json
  plus a set of the local dose-rate patches, built and installed under
  .scratch/kernels/<variant>/{source,build,install}. The shared source and install are never
  modified. A variant whose install/kernel.json verifies is skipped.

  -Verify   only check every variant (patches applied, recorded hashes, DLL hash), no build.
  -Variant  restrict to the named variants (default: all, in buildOrder).
  -Check    only run 'git apply --check' of each variant's patch set on the pristine source.
  -KeepBuild keep the build tree after install (default: deleted, it is several GB).
  -Jobs     parallel compile jobs (default 4).
#>
[CmdletBinding()]
param(
  [string[]]$Variant,
  [switch]$Verify,
  [switch]$Check,
  [switch]$KeepBuild,
  [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$config = Get-Content (Join-Path $PSScriptRoot 'kernels.json') -Raw | ConvertFrom-Json
$project = Get-Content (Join-Path $root '.claude\.claude-project.json') -Raw | ConvertFrom-Json

function Expand-ProjectPath([string]$value) {
  foreach ($name in 'DEV_DIR', 'G4_ROOT') {
    $v = [Environment]::GetEnvironmentVariable($name)
    if ($value.Contains("`${$name}")) {
      if ([string]::IsNullOrEmpty($v)) {
        throw "$name is not set: run ~/dotfiles/scripts/windows/setup-dev-env.ps1 and restart the shell"
      }
      $value = $value.Replace("`${$name}", $v)
    }
  }
  return [IO.Path]::GetFullPath($value)
}

$pristine = Expand-ProjectPath $project.geant4SourcePath
$toolchain = Expand-ProjectPath $project.build.toolchainFile
$vcpkgInstalled = Join-Path (Split-Path (Split-Path (Split-Path $toolchain))) 'installed\x64-windows'
$kernelsRoot = Join-Path $root '.scratch\kernels'
if (-not (Test-Path (Join-Path $pristine 'CMakeLists.txt'))) { throw "Pristine source not found: $pristine" }

$names = if ($Variant) { $Variant } else { $config.buildOrder }
foreach ($n in $names) { if (-not $config.variants.$n) { throw "Unknown variant '$n'" } }

$lfPatches = Join-Path ([IO.Path]::GetTempPath()) "g4patches-$PID"
New-Item -ItemType Directory -Force $lfPatches | Out-Null

# Patch files may be checked out with CRLF (autocrlf) while the Geant4 source is LF:
# apply an LF-normalised copy.
function Get-PatchPaths([string]$name) {
  $config.variants.$name | ForEach-Object {
    $src = Join-Path $root $config.patches.$_
    $dst = Join-Path $lfPatches "$_-$(Split-Path $src -Leaf)"
    if (-not (Test-Path $dst)) {
      $text = [IO.File]::ReadAllText($src) -replace "`r`n", "`n"
      [IO.File]::WriteAllText($dst, $text)
    }
    $dst
  }
}

function Get-OwnedFiles([string]$patch) {
  Select-String -Path $patch -Pattern '^\+\+\+ b/(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Get-Sha([string]$path) { (Get-FileHash -Algorithm SHA256 $path).Hash }

function Find-VcVars {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) { $bat = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'; if (Test-Path $bat) { return $bat } }
  }
  throw 'vcvars64.bat not found (vswhere returned nothing)'
}

function Test-Variant([string]$name) {
  $dir = Join-Path $kernelsRoot $name
  $meta = Join-Path $dir 'install\kernel.json'
  if (-not (Test-Path $meta)) { return 'MISSING (no install/kernel.json)' }
  $k = Get-Content $meta -Raw | ConvertFrom-Json
  $source = Join-Path $dir 'source'
  foreach ($p in Get-PatchPaths $name) {
    & git -C $source -c core.autocrlf=false apply --check --reverse $p 2>$null
    if ($LASTEXITCODE -ne 0) { return "FAIL (patch not applied: $(Split-Path $p -Leaf))" }
  }
  foreach ($f in $k.ownedFiles.PSObject.Properties) {
    $path = Join-Path $source $f.Name
    if ((Get-Sha $path) -ne $f.Value) { return "FAIL (hash changed: $($f.Name))" }
    if ((Get-Sha (Join-Path $pristine $f.Name)) -eq $f.Value) { return "FAIL (equals pristine: $($f.Name))" }
  }
  $dll = Join-Path $dir 'install\bin\G4processes_core.dll'
  if (-not (Test-Path $dll)) { return 'FAIL (no G4processes_core.dll)' }
  if ((Get-Sha $dll) -ne $k.processesCoreSha256) { return 'FAIL (DLL hash mismatch)' }
  return 'OK (patches applied, DLL hash matches)'
}

if ($Check) {
  $tmp = Join-Path ([IO.Path]::GetTempPath()) "g4check-$PID"
  $failed = $false
  foreach ($name in $names) {
    # Apply the set in order on a scratch copy of just the owned files.
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    New-Item -ItemType Directory $tmp | Out-Null
    & git -C $tmp init -q
    foreach ($p in Get-PatchPaths $name) {
      foreach ($f in Get-OwnedFiles $p) {
        $dst = Join-Path $tmp $f
        if (-not (Test-Path $dst)) {
          New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
          Copy-Item (Join-Path $pristine $f) $dst
        }
      }
    }
    $ok = $true
    foreach ($p in Get-PatchPaths $name) {
      & git -C $tmp -c core.autocrlf=false apply $p 2>&1 | Out-Null
      if ($LASTEXITCODE -ne 0) { $ok = $false; Write-Output "${name}: FAIL ($(Split-Path $p -Leaf) does not apply)"; break }
    }
    if ($ok) { Write-Output "${name}: OK (git apply --check)" } else { $failed = $true }
  }
  Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
  exit ([int]$failed)
}

if ($Verify) {
  $failed = $false
  foreach ($name in $names) {
    $r = Test-Variant $name
    Write-Output "${name}: $r"
    if (-not $r.StartsWith('OK')) { $failed = $true }
  }
  exit ([int]$failed)
}

$vcvars = Find-VcVars
foreach ($name in $names) {
  $r = Test-Variant $name
  if ($r.StartsWith('OK')) { Write-Output "${name}: reuse, $r"; continue }
  $dir = Join-Path $kernelsRoot $name
  $source = Join-Path $dir 'source'; $build = Join-Path $dir 'build'; $install = Join-Path $dir 'install'
  Write-Output "${name}: building in $dir ($(Get-Date -Format s))"
  Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force $dir | Out-Null

  robocopy $pristine $source /MIR /XD .git /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
  & git -C $source init -q
  $owned = [ordered]@{}
  foreach ($p in Get-PatchPaths $name) {
    & git -C $source -c core.autocrlf=false apply --verbose $p
    if ($LASTEXITCODE -ne 0) { throw "git apply failed: $p" }
    foreach ($f in Get-OwnedFiles $p) { $owned[$f] = $null }
  }
  foreach ($f in @($owned.Keys)) {
    $h = Get-Sha (Join-Path $source $f)
    if ($h -eq (Get-Sha (Join-Path $pristine $f))) { throw "Patch did not change $f" }
    $owned[$f] = $h
  }

  # CMake reads backslashes as escapes: pass forward-slash paths.
  $fs = { param($p) $p.Replace('\', '/') }
  $cSource = & $fs $source; $cBuild = & $fs $build; $cInstall = & $fs $install
  $cToolchain = & $fs $toolchain; $cVcpkg = & $fs $vcpkgInstalled
  $log = Join-Path $dir 'build.log'
  $cmd = Join-Path $dir 'build.cmd'
  @"
@echo off
call "$vcvars" >nul
cmake -S "$cSource" -B "$cBuild" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$cInstall" -DCMAKE_TOOLCHAIN_FILE="$cToolchain" -DGEANT4_BUILD_MULTITHREADED=ON -DGEANT4_USE_SYSTEM_ZLIB=ON -DGEANT4_USE_HDF5=ON -DGEANT4_USE_QT=OFF -DGEANT4_INSTALL_DATA=OFF -DHDF5_ROOT="$cVcpkg" -DHDF5_C_INCLUDE_DIR="$cVcpkg/include" -DHDF5_hdf5_LIBRARY_RELEASE="$cVcpkg/lib/hdf5.lib"
if errorlevel 1 exit /b 1
cmake --build "$cBuild" -j $Jobs
if errorlevel 1 exit /b 1
cmake --install "$cBuild"
if errorlevel 1 exit /b 1
"@ | Set-Content -Encoding ascii $cmd
  $t0 = Get-Date
  cmd /c "`"$cmd`" > `"$log`" 2>&1"
  if ($LASTEXITCODE -ne 0) { throw "${name}: build failed, see $log" }

  $dll = Join-Path $install 'bin\G4processes_core.dll'
  [ordered]@{
    variant = $name
    geant4Version = $config.geant4Version
    patches = @($config.variants.$name | ForEach-Object { $config.patches.$_ })
    ownedFiles = $owned
    processesCoreSha256 = Get-Sha $dll
    pristineSource = $pristine
    builtAt = (Get-Date -Format s)
    buildSeconds = [math]::Round(((Get-Date) - $t0).TotalSeconds)
  } | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $install 'kernel.json')
  if (-not $KeepBuild) { Remove-Item -Recurse -Force $build }
  Write-Output "${name}: $(Test-Variant $name)"
}
