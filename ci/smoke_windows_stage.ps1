# Exercise the packaged Windows QML runtime without the runner's Qt SDK or
# vcpkg directories. Test before NSIS creates an installer from this stage.
param(
    [Parameter(Mandatory = $true)][string] $Stage
)
$ErrorActionPreference = 'Stop'
$stagePath = (Resolve-Path $Stage).Path
$executable = Join-Path $stagePath 'Genycaster.exe'
if (-not (Test-Path $executable -PathType Leaf)) {
    throw "Missing bundled Windows executable: $executable"
}
$settings = Join-Path $env:RUNNER_TEMP "genycaster-bundle-smoke-$PID.ini"
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $executable
$psi.WorkingDirectory = $stagePath
$psi.ArgumentList.Add('--smoke-test')
$psi.ArgumentList.Add("--settings-path=$settings")
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true
foreach ($variable in @(
    'QT_ROOT_DIR', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH',
    'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH',
    'VCPKG_ROOT', 'VCPKG_INSTALLATION_ROOT'
)) {
    [void]$psi.Environment.Remove($variable)
}
# Windows and System32 only. Do not find Qt or OpenSSL from the runner's PATH.
$psi.Environment['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
$psi.Environment['QT_QPA_PLATFORM'] = 'windows'
$psi.Environment['QSG_RHI_BACKEND'] = 'software'
$psi.Environment['QML_DISABLE_DISK_CACHE'] = '1'
$process = [System.Diagnostics.Process]::Start($psi)
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(45000)) {
    $process.Kill($true)
    throw 'Packaged Windows QML startup timed out'
}
$output = $stdoutTask.GetAwaiter().GetResult()
$errors = $stderrTask.GetAwaiter().GetResult()
if ($output) { Write-Output $output }
if ($errors) { Write-Output $errors }
if ($process.ExitCode -ne 0) {
    throw "Packaged Windows QML startup failed with exit code $($process.ExitCode)"
}
if (($output + $errors) -match 'QQmlApplicationEngine failed to load component') {
    throw 'Packaged Windows QML failed to load'
}
Write-Output 'Packaged Windows Qt/QML startup smoke: PASS'
