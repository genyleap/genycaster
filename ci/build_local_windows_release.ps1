# Local equivalent of the Windows build/package job in .github/workflows/ci.yml.
# Run inside the native Visual Studio 2022 Developer PowerShell.
param(
    [ValidateSet('arm64','x86_64')][string]$Architecture = 'arm64',
    [ValidateRange(1,32)][int]$Jobs = 3
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function CheckExit([string]$operation) {
    if ($LASTEXITCODE -ne 0) { throw "$operation failed: exit code $LASTEXITCODE" }
}
Set-Location (Resolve-Path "$PSScriptRoot/..").Path
$repo = (Get-Location).Path
$native = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$expected = if ($Architecture -eq 'arm64') { 'Arm64' } else { 'X64' }
if ($native -ne $expected) { throw "Native $expected Windows required, got $native" }
$target = if ($Architecture -eq 'arm64') { 'arm64' } else { 'amd64' }
if ($env:VSCMD_ARG_TGT_ARCH -ne $target) {
    throw "Open Visual Studio Developer PowerShell with -arch=$target, not $env:VSCMD_ARG_TGT_ARCH"
}
if ((& git branch --show-current).Trim() -ne 'main') { throw 'Checkout main first.' }
if ((& git status --porcelain | Out-String).Trim()) { throw 'Commit/stash local changes.' }
$vcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
if (-not $vcpkgRoot) { $vcpkgRoot = $env:VCPKG_ROOT }
if (-not $vcpkgRoot) { throw 'Set VCPKG_INSTALLATION_ROOT to a bootstrapped vcpkg checkout.' }
$vcpkg = Join-Path $vcpkgRoot 'vcpkg.exe'
if (-not (Test-Path $vcpkg)) { throw "Missing $vcpkg" }
$llvm = Join-Path $env:ProgramFiles 'LLVM/bin'
$clang = Join-Path $llvm 'clang-cl.exe'
$scanner = Join-Path $llvm 'clang-scan-deps.exe'
foreach ($file in @($clang,$scanner)) { if (-not (Test-Path $file)) { throw "Missing $file" } }
foreach ($command in @('cmake','ninja','python','makensis')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Missing $command" }
}
& python ci/release_gate.py --ref refs/heads/main --event workflow_dispatch
CheckExit 'Release version gate'
$version = (& python -c 'from pathlib import Path; from ci.release_gate import release_designation; print(release_designation(Path.cwd()))').Trim()
CheckExit 'Version'
& python -m unittest ci.test_audit_windows_stage ci.test_nsis_cpack_config
CheckExit 'Windows packaging tests'

# Identical official pinned Qt archive installer as the Windows hosted workflow.
$qtTree = Join-Path $repo 'Qt'
$qtConfigs = @(Get-ChildItem -Path $qtTree -Recurse -File -Filter 'Qt6Config.cmake' -ErrorAction SilentlyContinue)
if ($qtConfigs.Count -ne 1) {
    & python ci/install_qt_windows.py --destination Qt --architecture $Architecture
    CheckExit 'Qt 6.12.0 installation'
    $qtConfigs = @(Get-ChildItem -Path $qtTree -Recurse -File -Filter 'Qt6Config.cmake')
}
if ($qtConfigs.Count -ne 1) { throw 'Exactly one Qt installation must exist in Qt/.' }
$env:QT_ROOT_DIR = (Resolve-Path (Join-Path $qtConfigs[0].DirectoryName '../../..')).Path
$triplet = if ($Architecture -eq 'arm64') { 'arm64-windows' } else { 'x64-windows' }
$cache = Join-Path $repo 'build/vcpkg-binary-cache'
New-Item -ItemType Directory -Force $cache | Out-Null
$env:VCPKG_DEFAULT_BINARY_CACHE = $cache
$packages = @('boost-asio','boost-beast','boost-json','boost-multiprecision',
    'boost-property-tree','boost-system','curl','openssl','sqlite3') |
    ForEach-Object { ('{0}:{1}' -f $_, $triplet) }
& $vcpkg install @packages
CheckExit 'vcpkg installation'
$include = Join-Path $vcpkgRoot "installed/$triplet/include"
foreach ($header in @('boost/json/src.hpp','boost/multiprecision/cpp_int.hpp',
    'boost/asio.hpp','boost/beast.hpp','boost/property_tree/json_parser.hpp',
    'boost/system/error_code.hpp','curl/curl.h','sqlite3.h')) {
    if (-not (Test-Path (Join-Path $include $header))) { throw "Missing $header" }
}
$build = Join-Path $repo "build/local-windows-$Architecture"
$configure = @('-S','.','-B',$build,'-G','Ninja',
    "-DCMAKE_C_COMPILER=$clang","-DCMAKE_CXX_COMPILER=$clang",
    "-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=$scanner",'-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_PREFIX_PATH=$env:QT_ROOT_DIR",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake",
    "-DVCPKG_TARGET_TRIPLET=$triplet",'-DVCPKG_APPLOCAL_DEPS=OFF',
    "-DGENYCASTER_WINDOWS_PACKAGE_ARCH=$Architecture",
    '-DFARCASTER_BUILD_TESTS=OFF','-DGENYCASTER_FETCH_QTKEYCHAIN=ON',
    '-DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON')
& cmake @configure
CheckExit 'CMake configure'
& cmake --build $build --target qt6keychain --parallel $Jobs
CheckExit 'QtKeychain build'
$keychainBuilt = Get-ChildItem $build -Recurse -File -Filter 'qt6keychain.dll' | Select-Object -First 1
if (-not $keychainBuilt) { throw 'Missing built qt6keychain.dll' }
Copy-Item $keychainBuilt.FullName (Join-Path $env:QT_ROOT_DIR 'bin/qt6keychain.dll') -Force
& cmake --build $build --target genycaster --parallel $Jobs
CheckExit 'Genycaster build'
# NSIS reads dist/app by design. Remove stale DLLs from any prior architecture.
$stage = Join-Path $repo 'dist/app'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $build 'bin/Genycaster.exe') $stage
$deployArgs = @('--release','--skip-plugin-types','sqldrivers,qmltooling',
    '--qmldir','ui','--dir',$stage,(Join-Path $stage 'Genycaster.exe'))
& (Join-Path $env:QT_ROOT_DIR 'bin/windeployqt.exe') @deployArgs
CheckExit 'windeployqt'
$runtimeDlls = @(Get-ChildItem (Join-Path $vcpkgRoot "installed/$triplet/bin") -File -Filter '*.dll')
foreach ($pattern in @('libcrypto*.dll','libssl*.dll','*sqlite*.dll','*curl*.dll')) {
    if (@($runtimeDlls | Where-Object { $_.Name -like $pattern }).Count -eq 0) {
        throw "Required vcpkg runtime dependency missing: $pattern"
    }
}
foreach ($dll in $runtimeDlls) { Copy-Item $dll.FullName $stage }
$keychain = Get-ChildItem $build -Recurse -File -Filter '*keychain*.dll' | Select-Object -First 1
if (-not $keychain) { throw 'Missing QtKeychain runtime DLL' }
Copy-Item $keychain.FullName $stage
& python ci/audit_windows_stage.py $stage $Architecture
CheckExit 'Staged PE architecture/dependency audit'
if (-not $env:RUNNER_TEMP) { $env:RUNNER_TEMP = $env:TEMP }
& (Join-Path $repo 'ci/smoke_windows_stage.ps1') -Stage $stage
CheckExit 'Isolated Windows packaged QML smoke'
$vcArch = if ($Architecture -eq 'arm64') { 'arm64' } else { 'x64' }
$redist = Join-Path $stage "vc_redist.$vcArch.exe"
if (-not (Test-Path $redist)) { throw "Missing Microsoft VC redist: $redist" }
$sig = Get-AuthenticodeSignature -FilePath $redist
if ($sig.Status -ne 'Valid' -or -not $sig.SignerCertificate -or
    $sig.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'Microsoft VC redistributable signature is invalid.'
}
$outdir = Join-Path $repo "dist/local-windows-$Architecture"
New-Item -ItemType Directory -Force $outdir | Out-Null
& cpack --config (Join-Path $build 'CPackConfig.cmake') -G NSIS -B $outdir
CheckExit 'CPack NSIS build'
$installer = Join-Path $outdir "GenyCaster-v$version-windows-$Architecture.exe"
if (-not (Test-Path $installer)) { throw "Missing NSIS installer: $installer" }
& python ci/verify_platform_package.py windows $installer
CheckExit 'Windows installer verification'
$hash = (Get-FileHash -Algorithm SHA256 $installer).Hash.ToLowerInvariant()
"$hash  $(Split-Path $installer -Leaf)" | Set-Content -Encoding ascii (Join-Path $outdir 'SHA256SUMS')
& python ci/package_selfupdate.py --platform windows --arch $Architecture --version $version --source $stage --output (Join-Path $outdir 'selfupdate')
CheckExit 'Self-update snapshot'
& python ci/verify_selfupdate_snapshot.py (Join-Path $outdir "selfupdate/GenyCaster-selfupdate-v$version-windows-$Architecture.zip")
CheckExit 'Self-update audit'
Write-Host "Release-equivalent local Windows package: $installer"
Write-Host 'No GitHub Actions started.'
