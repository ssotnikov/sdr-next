#Requires -Version 7.0
<#
.SYNOPSIS
    Build, test and check SDR Next on Windows.

.DESCRIPTION
    All builds use CGO_ENABLED=0 (ADR-0002), so every target, including Linux,
    can be cross-compiled from Windows. Binaries go to dist\.

.PARAMETER Task
    build  - build for Windows (default: both amd64 and arm64)
    cross  - build for every supported OS and architecture
    test   - run the tests
    vet    - run go vet
    fmt    - fail if any file is not gofmt-formatted
    vuln   - run govulncheck (must be installed)
    check  - fmt + vet + test
    all    - check + build (default)
    clean  - remove dist\

.PARAMETER Arch
    Windows architectures for the build task: amd64, arm64 or all (default).

.PARAMETER Version
    Version embedded in the binary. Defaults to `git describe`.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Task build -Arch arm64
    .\build.ps1 -Task cross -Version 0.1.0
#>
[CmdletBinding()]
param(
    [ValidateSet('all', 'build', 'cross', 'test', 'vet', 'fmt', 'vuln', 'check', 'clean')]
    [string] $Task = 'all',

    [ValidateSet('amd64', 'arm64', 'all')]
    [string] $Arch = 'all',

    [ValidatePattern('^[0-9A-Za-z._+-]{1,64}$')]
    [string] $Version
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root = $PSScriptRoot
$Dist = Join-Path $Root 'dist'
$Package = './cmd/sdrnext'
$Targets = @('windows/amd64', 'windows/arm64', 'linux/amd64', 'linux/arm64')

# Invoke runs a native command with explicit arguments (no shell
# interpolation) and stops on a non-zero exit code.
function Invoke([string] $Exe, [string[]] $Arguments) {
    Write-Host "> $Exe $($Arguments -join ' ')" -ForegroundColor DarkGray
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Exe failed with exit code $LASTEXITCODE"
    }
}

function Get-Version {
    if ($Version) { return $Version }
    $v = & git -C $Root describe --tags --always --dirty 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $v) { return 'dev' }
    return $v.Trim()
}

# WithEnv sets environment variables for the duration of a script block
# and restores the previous values afterwards.
function WithEnv([hashtable] $Vars, [scriptblock] $Body) {
    $saved = @{}
    foreach ($k in $Vars.Keys) {
        $saved[$k] = [Environment]::GetEnvironmentVariable($k)
        [Environment]::SetEnvironmentVariable($k, $Vars[$k])
    }
    try { & $Body }
    finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
}

function Build-Target([string] $Target) {
    $goos, $goarch = $Target -split '/'
    $ext = if ($goos -eq 'windows') { '.exe' } else { '' }
    $out = Join-Path $Dist "sdrnext-$goos-$goarch$ext"
    $ldflags = "-s -w -X main.version=$(Get-Version)"
    New-Item -ItemType Directory -Force -Path $Dist | Out-Null
    WithEnv @{ CGO_ENABLED = '0'; GOOS = $goos; GOARCH = $goarch } {
        Invoke 'go' @('build', '-trimpath', '-ldflags', $ldflags, '-o', $out, $Package)
    }
    Write-Host "built $out" -ForegroundColor Green
}

function Task-Build {
    $arches = if ($Arch -eq 'all') { @('amd64', 'arm64') } else { @($Arch) }
    foreach ($a in $arches) { Build-Target "windows/$a" }
}

function Task-Cross { foreach ($t in $Targets) { Build-Target $t } }

function Task-Test { WithEnv @{ CGO_ENABLED = '0' } { Invoke 'go' @('test', './...') } }

function Task-Vet { Invoke 'go' @('vet', './...') }

function Task-Fmt {
    $files = & gofmt -l $Root
    if ($LASTEXITCODE -ne 0) { throw 'gofmt failed' }
    if ($files) {
        $files | ForEach-Object { Write-Host "not formatted: $_" -ForegroundColor Red }
        throw 'run gofmt -w . to fix formatting'
    }
}

function Task-Vuln {
    if (-not (Get-Command govulncheck -ErrorAction SilentlyContinue)) {
        throw 'govulncheck not found; install it with: go install golang.org/x/vuln/cmd/govulncheck@latest'
    }
    Invoke 'govulncheck' @('./...')
}

function Task-Check { Task-Fmt; Task-Vet; Task-Test }

function Task-Clean {
    if (Test-Path $Dist) { Remove-Item -Recurse -Force -LiteralPath $Dist }
}

Push-Location $Root
try {
    switch ($Task) {
        'build' { Task-Build }
        'cross' { Task-Cross }
        'test'  { Task-Test }
        'vet'   { Task-Vet }
        'fmt'   { Task-Fmt }
        'vuln'  { Task-Vuln }
        'check' { Task-Check }
        'clean' { Task-Clean }
        'all'   { Task-Check; Task-Build }
    }
}
finally {
    Pop-Location
}
