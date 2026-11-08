param(
    [double]$Seconds = 28800,
    [string]$BuildDirectory = 'build/windows-device-msvc2026',
    [string]$RunDirectory = ('build/endurance/' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [string]$QtRoot = 'D:/Tools/sdr-build/Qt/6.10.0/msvc2022_64',
    [string]$SoapyRuntime = 'D:/Tools/radioconda/Library/bin'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$runPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $RunDirectory))
$binaryPath = [IO.Path]::GetFullPath((Join-Path $projectRoot "$BuildDirectory/bin/sdr_app.exe"))
if (-not [double]::IsFinite($Seconds) -or $Seconds -lt 1 -or $Seconds -gt 28800) { throw 'Duration must be 1..28800 seconds.' }
if (Test-Path -LiteralPath $runPath) { throw 'Choose a fresh output directory.' }
if (-not (Test-Path -LiteralPath $binaryPath)) { throw 'Build the receiver first.' }
$space = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($runPath))
if ($space.AvailableFreeSpace -lt ($Seconds * 96000 + 1GB)) { throw 'Insufficient disk space for WAV and logs.' }
if (Get-Process sdr_app -ErrorAction SilentlyContinue) { throw 'Another receiver is open; finish its session first.' }
New-Item -ItemType Directory -Path $runPath | Out-Null
$env:PATH = "$QtRoot/bin;$SoapyRuntime;$env:PATH"
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_MEDIA_BACKEND = 'windows'
$env:QT_FORCE_STDERR_LOGGING = '1'
foreach ($name in @('QT_QUICK_BACKEND','QT_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM_PLUGIN_PATH')) {
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
$metadata = [ordered]@{
    requested_seconds = $Seconds
    prepared_utc = [DateTimeOffset]::UtcNow.ToString('o')
    timezone = 'Europe/Moscow'
    commit = (git -C $projectRoot rev-parse HEAD)
    binary_sha256 = (Get-FileHash -LiteralPath $binaryPath).Hash
    dirty_worktree = @((git -C $projectRoot status --porcelain))
    os = (Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber)
    registry_os_compatibility_name = (Get-ItemProperty 'HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion' | Select-Object ProductName,DisplayVersion,CurrentBuild,UBR)
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    memory_bytes = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    graphics = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    power_scheme = ((powercfg /getactivescheme) -join ' ')
    free_bytes = $space.AvailableFreeSpace
    profile = 'HackRF / 2.4 MSPS / 100 MHz / WFM / gain 16 / WAV mono PCM16 48 kHz'
    qt = '6.10.0 msvc2022_64'
    runtime_path = $SoapyRuntime
    runtime_sha256 = (Get-FileHash -LiteralPath (Join-Path $SoapyRuntime 'SoapySDR.dll')).Hash
    modules = @(Get-ChildItem -LiteralPath (Join-Path (Split-Path $binaryPath) 'devices') -Filter '*.dll' | ForEach-Object {
        @{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    measurement_limits = @('UI queued ping is a proxy; input latency not verified','Physical RF-to-audio latency not verified','WAV only; no continuous Raw IQ','No system sleep/power plan settings are changed')
}
if (Test-Path -LiteralPath (Join-Path $SoapyRuntime 'hackrf_info.exe')) {
    $hardwareInfo = & (Join-Path $SoapyRuntime 'hackrf_info.exe') 2>&1
    $metadata.hardware_versions = @($hardwareInfo | ForEach-Object { "$_" } | Where-Object { $_ -match 'hackrf_info version|libhackrf version|Firmware Version|Hardware Revision|Board ID' })
}
[IO.File]::WriteAllText((Join-Path $runPath 'environment.json'), ($metadata | ConvertTo-Json -Depth 7))
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $binaryPath
$start.WorkingDirectory = Split-Path $binaryPath
$start.UseShellExecute = $false
# The actual receiver window stays visible to exercise native rendering.
$start.ArgumentList.Add('--endurance-dir')
$start.ArgumentList.Add((Join-Path $runPath 'session'))
$start.ArgumentList.Add('--endurance-seconds')
$start.ArgumentList.Add($Seconds.ToString([Globalization.CultureInfo]::InvariantCulture))
$start.ArgumentList.Add('--endurance-provider')
$start.ArgumentList.Add('device:org.sdr.device.hackrf:')
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::Start($start)
$errors = $process.StandardError.ReadToEndAsync()
$state = @{pid=$process.Id;state='running';launched_utc=[DateTimeOffset]::UtcNow.ToString('o');run_directory=$runPath}
[IO.File]::WriteAllText((Join-Path $runPath 'process.json'), ($state | ConvertTo-Json))
$summaryPath = Join-Path $runPath 'session/summary.json'
while (-not $process.WaitForExit(1000)) {
    if (Test-Path -LiteralPath $summaryPath) {
        try {
            $result = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
            if ($result.state -eq 'failed' -and $state.state -ne 'failed_window_open') {
                $state.state = 'failed_window_open'
                $state.test_exit_code = 2
                [IO.File]::WriteAllText((Join-Path $runPath 'process.json'), ($state | ConvertTo-Json))
            }
        } catch { } # A summary may still be in the process of being written.
    }
}
[IO.File]::WriteAllText((Join-Path $runPath 'stderr.log'), $errors.GetAwaiter().GetResult())
$state.state = 'exited'
$state.exit_code = $process.ExitCode
$state.test_exit_code = $process.ExitCode
if (Test-Path -LiteralPath $summaryPath) {
    $result = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    if ($result.state -ne 'duration_completed') { $state.test_exit_code = 2 }
}
$state.finished_utc = [DateTimeOffset]::UtcNow.ToString('o')
[IO.File]::WriteAllText((Join-Path $runPath 'process.json'), ($state | ConvertTo-Json))
Write-Output "Endurance process exited $($process.ExitCode); report: $runPath"
exit $state.test_exit_code
