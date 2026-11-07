param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$BuildDirectory = 'build/desktop',
    [string]$MingwRoot,
    [string]$SoapySdkPrefix
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtPath = (Resolve-Path -LiteralPath $QtRoot).Path
$arguments = @('-S', $projectRoot, '-B', $BuildDirectory, '-DCMAKE_BUILD_TYPE=Release')
if ($MingwRoot) {
    $compilerBin = Join-Path (Resolve-Path -LiteralPath $MingwRoot).Path 'bin'
    $env:PATH = "$compilerBin;$qtPath/bin;$env:PATH"
    $arguments += @('-G', 'Ninja', "-DCMAKE_C_COMPILER=$compilerBin/gcc.exe", "-DCMAKE_CXX_COMPILER=$compilerBin/g++.exe")
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install MSVC 2022 C++ Build Tools first.' }
    $installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'MSVC 2022 C++ toolset was not found. No compiler is installed automatically.' }
    Import-Module (Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
    $arguments += @('-G','Ninja','-DCMAKE_C_COMPILER=cl','-DCMAKE_CXX_COMPILER=cl')
    $env:PATH = "$qtPath/bin;$env:PATH"
}
$prefix = if ($SoapySdkPrefix) { "$qtPath;$SoapySdkPrefix" } else { $qtPath }
$arguments += "-DCMAKE_PREFIX_PATH=$prefix"
Push-Location $projectRoot
try {
    & cmake @arguments
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & cmake --build $BuildDirectory --parallel 4
    if ($LASTEXITCODE) { throw 'Compilation failed.' }
    & ctest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
} finally { Pop-Location }
