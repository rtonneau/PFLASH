#requires -Version 7
<#
.SYNOPSIS
  Whole patch-necessity suite: kernels, sufficiency fixtures 1-3, leave-one-out
  histories for every config of necessity/necessity.json, and the report.

.DESCRIPTION
  1. BuildKernels.ps1 (reuses every variant that verifies);
  2. RunSufficiency.ps1 -Patch 1..3 (incremental suite builds);
  3. RunNecessity.ps1 -Config <each config>;
  4. necessity/report.py writes <Report> and <Report>.json.
  Exit 0 only when every demonstration holds.

.PARAMETER Reuse   keep finished necessity runs (RunNecessity -Reuse)
.PARAMETER Report  report path (default .scratch/tests/<newest session>/patch-necessity-report.md)
.PARAMETER Python  interpreter with numpy and h5py (default python)
#>
[CmdletBinding()]
param([switch]$Reuse, [string]$Report, [string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path.Replace('\', '/')
$work = "$root/.scratch/dose-rate-patches"
if (-not $Report) {
  $session = Get-ChildItem "$root/.scratch/tests" -Directory | Sort-Object Name | Select-Object -Last 1
  $Report = "$($session.FullName)/patch-necessity-report.md".Replace('\', '/')
}

& "$PSScriptRoot/BuildKernels.ps1"
if ($LASTEXITCODE -ne 0) { throw 'BuildKernels.ps1 failed' }

$suf = "$work/sufficiency"
New-Item -ItemType Directory -Force $suf | Out-Null
foreach ($n in 1..3) {
  & "$PSScriptRoot/RunSufficiency.ps1" -Patch $n -Json "$suf/sufficiency-$n.json"
}

$settings = Get-Content "$PSScriptRoot/necessity/necessity.json" -Raw | ConvertFrom-Json
foreach ($c in $settings.configs.PSObject.Properties.Name) {
  & "$PSScriptRoot/RunNecessity.ps1" -Config $c -Reuse:$Reuse -Python $Python | Select-String -Pattern '^patch\d|^results:' | ForEach-Object { "[$c] $($_.Line)" }
}

& $Python "$PSScriptRoot/necessity/report.py" --sufficiency $suf --necessity "$work/necessity" --settings "$PSScriptRoot/necessity/necessity.json" --out $Report
exit $LASTEXITCODE
