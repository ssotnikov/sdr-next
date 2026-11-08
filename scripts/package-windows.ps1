param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$BuildDirectory = 'build/desktop',
    [string]$OutputDirectory = 'dist/sdr-app-0.1-preview-win64',
    [switch]$WithoutDeviceModules
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtPath = (Resolve-Path -LiteralPath $QtRoot).Path
$buildPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$outputPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
if (Test-Path -LiteralPath $outputPath) { throw 'Choose a fresh output directory; existing packages are preserved.' }
$binaryFolder = Join-Path $buildPath 'bin'
if (Test-Path -LiteralPath (Join-Path $binaryFolder 'Release/sdr_app.exe')) { $binaryFolder = Join-Path $binaryFolder 'Release' }
$exe = Join-Path $binaryFolder 'sdr_app.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build sdr_app first.' }
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $outputPath
if(-not $WithoutDeviceModules -and (Test-Path -LiteralPath (Join-Path $binaryFolder 'devices'))) {
    Copy-Item -LiteralPath (Join-Path $binaryFolder 'devices') -Destination (Join-Path $outputPath 'devices') -Recurse
}
[IO.File]::WriteAllText((Join-Path $outputPath 'qt.conf'), "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`n")
& (Join-Path $qtPath 'bin/windeployqt.exe') --release --qmldir (Join-Path $projectRoot 'app') --dir $outputPath --no-translations --no-ffmpeg --skip-plugin-types multimedia (Join-Path $outputPath 'sdr_app.exe')
if ($LASTEXITCODE) { throw 'Qt deployment failed; the partial package is not ready.' }
New-Item -ItemType Directory -Path (Join-Path $outputPath 'multimedia') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $qtPath 'plugins/multimedia/windowsmediaplugin.dll') -Destination (Join-Path $outputPath 'multimedia')
foreach ($runtime in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
    $runtimePath = Join-Path $qtPath "bin/$runtime"
    if (Test-Path -LiteralPath $runtimePath) { Copy-Item -LiteralPath $runtimePath -Destination $outputPath }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $outputPath
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $outputPath
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/USER_GUIDE.md') -Destination $outputPath
New-Item -ItemType Directory -Path (Join-Path $outputPath 'licenses') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $outputPath 'licenses/IBM-Plex') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets/fonts/OFL.txt') -Destination (Join-Path $outputPath 'licenses/IBM-Plex/OFL.txt')
if (Test-Path -LiteralPath (Join-Path $projectRoot 'third_party/licenses')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party/licenses') -Destination (Join-Path $outputPath 'licenses/upstream') -Recurse
}
if (Test-Path -LiteralPath (Join-Path $qtPath 'sbom')) { Copy-Item -LiteralPath (Join-Path $qtPath 'sbom') -Destination (Join-Path $outputPath 'licenses/qt-sbom') -Recurse }
$toolsRoot = [IO.Path]::GetFullPath((Join-Path $qtPath '../../Tools'))
if ($qtPath -match 'mingw' -and (Test-Path -LiteralPath $toolsRoot)) {
    $compiler = Get-ChildItem -LiteralPath $toolsRoot -Directory -Filter 'mingw*' | Select-Object -First 1
    if ($compiler) {
        foreach ($component in @('gcc','mingw-w64','winpthreads')) {
            $licenseFolder = Join-Path $compiler.FullName "licenses/$component"
            if (Test-Path -LiteralPath $licenseFolder) { Copy-Item -LiteralPath $licenseFolder -Destination (Join-Path $outputPath "licenses/$component") -Recurse }
        }
    }
}
New-Item -ItemType File -Path (Join-Path $outputPath 'portable.txt') | Out-Null
$launcher = @'
@echo off
setlocal
set "QT_QPA_PLATFORM=windows"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%~dp0platforms"
set "QT_MEDIA_BACKEND=windows"
if defined SDR_SOAPY_RUNTIME set "PATH=%SDR_SOAPY_RUNTIME%;%PATH%"
start "SDR Next" "%~dp0sdr_app.exe"
'@
[IO.File]::WriteAllText((Join-Path $outputPath 'Start-SDR.cmd'),$launcher)
$exampleOutput = Join-Path $outputPath 'examples'
New-Item -ItemType Directory -Path $exampleOutput -Force | Out-Null
foreach ($name in @('sdr_source_example.dll','sdr_demod_example.dll','source-example.json','demod-example.json')) {
    $example = Join-Path $binaryFolder $name
    if (Test-Path -LiteralPath $example) { Copy-Item -LiteralPath $example -Destination $exampleOutput }
}
if (Test-Path -LiteralPath (Join-Path $binaryFolder 'examples-device')) {
    Copy-Item -LiteralPath (Join-Path $binaryFolder 'examples-device') -Destination (Join-Path $exampleOutput 'device') -Recurse
}
$files = Get-ChildItem -LiteralPath $outputPath -File -Recurse | ForEach-Object {
    [ordered]@{path=[IO.Path]::GetRelativePath($outputPath,$_.FullName);bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
}
$manifest = [ordered]@{version='0.1.0-preview';qt='6.10.0';acceptance='incomplete';files=@($files)}
[IO.File]::WriteAllText((Join-Path $outputPath 'package-manifest.json'),($manifest | ConvertTo-Json -Depth 6))
Write-Output $outputPath
