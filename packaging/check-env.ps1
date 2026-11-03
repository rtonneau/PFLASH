# Checks that the local Geant4 environment can run the prebuilt sim.exe of this
# folder: the Geant4 DLLs it imports are on PATH, their Geant4 version matches
# VERSION.txt, and the datasets are reachable. Read-only; changes nothing.
# Usage (from this folder): powershell -ExecutionPolicy Bypass -File .\check-env.ps1

$ok = $true
function Report($good, $text) {
    if ($good) { Write-Host "  [ok]   $text" } else { Write-Host "  [FAIL] $text" -ForegroundColor Red; $script:ok = $false }
}
function FindOnPath($name) {
    foreach ($dir in ($env:PATH -split ';')) {
        if ($dir -and (Test-Path (Join-Path $dir $name))) { return (Join-Path $dir $name) }
    }
    return $null
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Write-Host "dnachem-min environment check ($here)"

Report (Test-Path (Join-Path $here 'sim.exe')) 'sim.exe present'
Report ((Test-Path (Join-Path $here 'hdf5.dll')) -and (Test-Path (Join-Path $here 'hdf5_cpp.dll'))) 'hdf5.dll and hdf5_cpp.dll next to sim.exe'

# Geant4 version expected by sim.exe
$expected = $null
$versionFile = Join-Path $here 'VERSION.txt'
if (Test-Path $versionFile) {
    $m = Select-String -Path $versionFile -Pattern 'Geant4 (\S+)' | Select-Object -First 1
    if ($m) { $expected = $m.Matches[0].Groups[1].Value }
}

# Geant4 libraries imported by sim.exe (the ones most often missing)
$g4global = FindOnPath 'G4global.dll'
Report ($null -ne $g4global) ("G4global.dll on PATH" + $(if ($g4global) { " ($g4global)" } else { ': add <Geant4 install>\bin to PATH' }))
foreach ($dll in 'G4interfaces.dll', 'G4OpenGL.dll', 'G4analysis.dll', 'G4processes_core.dll') {
    Report ($null -ne (FindOnPath $dll)) "$dll on PATH"
}

# Version of that Geant4 install
if ($g4global) {
    $cfg = Join-Path (Split-Path -Parent (Split-Path -Parent $g4global)) 'lib\cmake\Geant4\Geant4ConfigVersion.cmake'
    if (Test-Path $cfg) {
        $m = Select-String -Path $cfg -Pattern 'set\(PACKAGE_VERSION "([^"]+)"' | Select-Object -First 1
        $found = if ($m) { $m.Matches[0].Groups[1].Value } else { 'unknown' }
        if ($expected) { Report ($found -eq $expected) "Geant4 version $found (sim.exe needs $expected)" }
        else { Write-Host "  [info] Geant4 version $found (VERSION.txt missing, cannot compare)" }
    } else {
        Write-Host "  [info] cannot read the Geant4 version ($cfg not found); make sure it matches VERSION.txt"
    }
}

# Qt (pulled in by G4interfaces.dll / G4OpenGL.dll when Geant4 was built with Qt)
$qt = $null
foreach ($dir in ($env:PATH -split ';')) {
    if ($dir -and (Test-Path $dir)) {
        $hit = Get-ChildItem -Path $dir -Filter 'Qt*.dll' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^Qt\dCore' } | Select-Object -First 1
        if ($hit) { $qt = $hit.FullName; break }
    }
}
Report ($null -ne $qt) ("Qt core library on PATH" + $(if ($qt) { " ($qt)" } else { ': add the Qt bin folder Geant4 was built with' }))

# MSVC runtime
Report (Test-Path (Join-Path $env:SystemRoot 'System32\msvcp140.dll')) 'MSVC runtime (msvcp140.dll) installed'

# Datasets: an explicit variable, GEANT4_DATA_DIR, or the path compiled into the install
$data = @('G4LEDATA', 'G4ENSDFSTATEDATA', 'GEANT4_DATA_DIR') | Where-Object { [Environment]::GetEnvironmentVariable($_) }
if ($data) { Write-Host "  [ok]   dataset variable(s) set: $($data -join ', ')" }
else { Write-Host "  [info] no G4LEDATA/GEANT4_DATA_DIR set; Geant4 will use the data path of its install (fine for a standard install)" }

if ($ok) { Write-Host "`nAll checks passed. Try: .\sim.exe example_irt.in" }
else { Write-Host "`nSome checks failed; see README.txt." -ForegroundColor Red; exit 1 }
