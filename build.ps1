$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root 'hvac_system.c'
$outDir = Join-Path $root 'build'
$outFile = Join-Path $outDir 'hvac-dashboard.exe'

if (-not (Test-Path $src)) {
    Write-Error "Source file not found: $src"
    exit 1
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$compiler = $null
foreach ($candidate in @('gcc', 'clang', 'cl')) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) {
        $compiler = $candidate
        break
    }
}

if (-not $compiler) {
    Write-Host 'No C compiler found on PATH. Install MinGW/MSYS2 or Visual Studio Build Tools and try again.'
    exit 1
}

if ($compiler -eq 'gcc' -or $compiler -eq 'clang') {
    & $compiler $src -o $outFile -lws2_32
}
elseif ($compiler -eq 'cl') {
    & $compiler /nologo $src /Fe$outFile /link Ws2_32.lib
}

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Build succeeded: $outFile"
