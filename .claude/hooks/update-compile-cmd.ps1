param()

$ErrorActionPreference = "Stop"

$hookLogPath = Join-Path $PSScriptRoot "hook-posttooluse.log"

$hookInput = [Console]::In.ReadToEnd() | ConvertFrom-Json
$filePath = $hookInput.tool_input.file_path
if (-not $filePath -or [IO.Path]::GetExtension($filePath) -notin ".cc", ".cpp", ".cxx") {
    "Skipped at $(Get-Date -Format o): $filePath" | Out-File -FilePath $hookLogPath -Append -Encoding utf8
    exit 0
}

"Triggered at $(Get-Date -Format o): $filePath" | Out-File -FilePath $hookLogPath -Append -Encoding utf8

# Fail-safe only (path validation lives in the dotfiles' check-env.ps1): without
# these variables the CMake paths below would silently be empty.
foreach ($name in "DEV_DIR", "G4_ROOT") {
    if (-not [Environment]::GetEnvironmentVariable($name)) {
        throw "$name is not set. Define the DEV_DIR and G4_ROOT user environment variables (dotfiles: scripts\setup-dev-env.ps1) and restart your shell/IDE."
    }
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = Join-Path $RepoRoot "build-ninja"
$SourceDir = $RepoRoot

if (-not (Test-Path -LiteralPath $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    "Created build directory at $(Get-Date -Format o): $BuildDir" | Out-File -FilePath $hookLogPath -Append -Encoding utf8
    Write-Host "Created build directory: $BuildDir" -ForegroundColor Cyan
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        throw "cl.exe is not available and Visual Studio's vswhere.exe was not found. Run from a Developer PowerShell for Visual Studio."
    }

    $vsInstallPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcVars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not $vsInstallPath -or -not (Test-Path $vcVars)) {
        throw "A Visual Studio C++ developer environment could not be located. Install the Desktop development with C++ workload."
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $devEnvironment = & cmd.exe /d /s /c "call `"$vcVars`" >nul && set" 2>&1
    $devEnvironmentExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($devEnvironmentExitCode -ne 0) {
        throw "Visual Studio's C++ developer environment failed to initialize (exit code $devEnvironmentExitCode)."
    }

    $devEnvironment |
    Where-Object { $_ -is [string] -and $_ -match "^[^=]+=.*$" } |
    ForEach-Object {
        $name, $value = $_ -split "=", 2
        Set-Item -Path "Env:$name" -Value $value
    }

    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        throw "Visual Studio's developer environment was initialized, but cl.exe is still unavailable."
    }
}

# Machine-specific roots come from the environment, never from this file:
#   G4_ROOT = Geant4 install dir; DEV_DIR = dev root (G4Vox lives under DEV_DIR\GEANT4\LIB).
# geant4InstallPath in .claude-project.json (may contain ${G4_ROOT} / ${DEV_DIR}) wins if present.
function Expand-PathVariables([string]$Value) {
    [regex]::Replace($Value, '\$\{(DEV_DIR|G4_ROOT)\}', { param($m) [Environment]::GetEnvironmentVariable($m.Groups[1].Value) })
}

$g4Install = $env:G4_ROOT
$projectConfig = Join-Path $RepoRoot ".claude\.claude-project.json"
if (Test-Path -LiteralPath $projectConfig) {
    $cfg = Get-Content -LiteralPath $projectConfig -Raw | ConvertFrom-Json
    if ($cfg.geant4InstallPath) { $g4Install = Expand-PathVariables $cfg.geant4InstallPath }
}
$g4Install = $g4Install.Replace("\", "/")
$g4VoxInstall = (Join-Path $env:DEV_DIR "GEANT4\LIB\G4Vox-install").Replace("\", "/")

$CMakeArgs = @(
    "-S", $SourceDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DGeant4_DIR=$g4Install/lib/cmake/Geant4",
    "-DG4Vox_DIR=$g4VoxInstall/lib/cmake/G4Vox",
    "-DCMAKE_PREFIX_PATH=$g4Install;$g4VoxInstall"
)

Write-Host "Configuring with CMake (Ninja)..." -ForegroundColor Cyan
& cmake @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed (exit code $LASTEXITCODE)."
}

$SourceFile = Join-Path $BuildDir "compile_commands.json"
$DestFile = Join-Path $SourceDir "compile_commands.json"
if (-not (Test-Path $SourceFile)) {
    throw "compile_commands.json was not generated."
}

Copy-Item $SourceFile $DestFile -Force
"Completed at $(Get-Date -Format o): copied compile_commands.json" | Out-File -FilePath $hookLogPath -Append -Encoding utf8
Write-Host "Copied compile_commands.json to the repository root." -ForegroundColor Green
