param([Parameter(Mandatory)][string]$PackageDirectory,[switch]$ReceiveSynthetic)
$ErrorActionPreference = 'Stop'
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = Join-Path $package 'sdr_app.exe'
$start.Arguments = if($ReceiveSynthetic){'--smoke-rx'}else{'--smoke-test'}
$start.WorkingDirectory = $package
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardError = $true
$start.Environment['PATH'] = "$env:SystemRoot/System32;$env:SystemRoot"
$start.Environment['QT_QPA_PLATFORM'] = 'windows'
$start.Environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = Join-Path $package 'platforms'
$start.Environment['QT_FORCE_STDERR_LOGGING'] = '1'
foreach ($name in @('QT_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QUICK_BACKEND')) {
    $start.Environment.Remove($name) | Out-Null
}
$process = [Diagnostics.Process]::Start($start)
$errors = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(45000)) {
    $process.Kill()
    throw 'Packaged GUI did not close within 45 seconds.'
}
$message = $errors.GetAwaiter().GetResult()
if ($process.ExitCode -ne 0) { throw "Packaged GUI failed ($($process.ExitCode)): $message" }
if($ReceiveSynthetic) {
    Write-Output 'Packaged GUI processed synthetic IQ and updated spectrum without developer PATH (exit 0). Hardware/audio quality/endurance are separate checks.'
} else {
    Write-Output 'Packaged GUI opened and closed without developer PATH (exit 0). RX/audio/endurance are separate checks.'
}
if ($message) { Write-Output $message }
