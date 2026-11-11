# Local release-equivalent builds and device acceptance

These commands **do not start GitHub Actions** or publish a GitHub Release.
They reproduce the platform packaging and acceptance commands from
[the release workflow](../.github/workflows/ci.yml), using a clean `main`
checkout and the same Qt version and required C++26 compiler family.
Outputs are not guaranteed to be bit-for-bit identical to GitHub-hosted builds
because timestamps, system libraries and toolchain revisions can differ.

## Option 1: install actual GitHub-hosted artifacts (no new CI minutes)

The last successful ARM64 artifacts are already available, as of 11 Nov 2026:

| Platform | Run ID | Artifact name | Built commit |
|---|---|---|---|
| Ubuntu/Linux arm64 | 42898561552 | GenyCaster-linux-arm64 | 1681acd0 |
| Windows arm64 | 42898571393 | GenyCaster-windows-arm64 | 1681acd0 |

Both were created on 10 Nov 2026 with a 14-day retention window (until
24 Nov 2026). They **predate the subsequent TLS/QtKeychain fixes** and are
suitable for reproducing the original failures, **not** accepting a new
release.

With authenticated GitHub CLI:

```bash
gh run download 42898561552 -R genyleap/genycaster \
  -n GenyCaster-linux-arm64 -D dist/from-github-linux-arm64
gh run download 42898571393 -R genyleap/genycaster \
  -n GenyCaster-windows-arm64 -D dist/from-github-windows-arm64
```

This downloads *exactly* the packages uploaded from those runs, without
recompiling or consuming new hosted CI minutes.

## Option 2: reproduce the latest Linux ARM64 release locally

## Recommended for Ubuntu 25.10 native ARM64: Ubuntu 24.04 Docker build

Ubuntu 25.10 (Questing) does not provide GCC 16 in its normal package
sources. **Do not mix Ubuntu 24.04 PPA/deb repositories into Ubuntu 25.10**.
The local container wrapper reuses the already installed official
`Qt/6.12.0/gcc_arm64` directory, but builds and packages against **Ubuntu
24.04 arm64** and GCC 16, like the GitHub Actions runner.

On the **ARM64 host** (not the VPS), install/start Docker if needed:

```bash
sudo apt update
sudo apt install -y docker.io
sudo systemctl enable --now docker
```

The wrapper requires the current account to have access to the Docker
daemon. If `docker info` says permission denied, an administrator can add
the account to the Docker group:

```bash
sudo usermod -aG docker "$USER"
newgrp docker
docker info >/dev/null
```

**Security:** Membership in the Docker group (or granting access to the
Docker socket) provides effectively root-level control over the host.
Only enable this for trusted developer accounts.

Then:

```bash
cd ~/Desktop/genycaster
git switch main
git pull --ff-only origin main
test "$(uname -m)" = aarch64
test -x Qt/6.12.0/gcc_arm64/bin/qmake6
BUILD_JOBS=2 bash ci/build_local_linux_arm64_container.sh
```

The wrapper builds a reusable Ubuntu 24.04 image with GCC 16 and
CMake 4.4.4, then runs `ci/build_local_linux_release.sh` on the mounted
host checkout as the **same numeric user**, preserving artifact ownership.
The local ARM64 wrapper uses `docker build --network host` and
`docker run --network host`, and enables host networking for its nested
clean-install smoke test. This is specifically to avoid Docker bridge DNS
failures such as `Temporary failure resolving 'ports.ubuntu.com'`
on the developer's Ubuntu 25.10 host. It does **not** modify
`/etc/docker/daemon.json` or change GitHub-hosted CI. Host networking
reduces network isolation; only run trusted build scripts. If the
host itself cannot resolve the Ubuntu mirror, fix the host's Internet/DNS
first; this fallback cannot solve an upstream outage.

Quick check before rebuilding after a DNS failure:

```bash
getent hosts ports.ubuntu.com
docker run --rm --network host ubuntu:24.04 \\
  getent hosts ports.ubuntu.com
git pull --ff-only origin main
BUILD_JOBS=2 bash ci/build_local_linux_arm64_container.sh
```

It mounts the host Docker socket only for the nested fresh-install DEB
and AppImage acceptance tests. The same host checkout absolute path is used
inside the build container so nested `docker run --mount` paths resolve
against the host daemon. The Dockerfile has no source code build context:
no Qt SDK or private files are sent into an image build context.

If the build succeeds, find the official-format packages in
`dist/local-linux-arm64/` on the host. Test the **DEB on Ubuntu 25.10**
as a separate compatibility check using `sudo apt install ./...deb`,
`/opt/genycaster/AppRun --check-tls` and the actual Qt GUI flow. The
automated clean-install test itself uses Ubuntu 24.04; it does **not**
establish that every newer host distribution renders correctly.

**Host:** Native Ubuntu 24.04 **aarch64** (not x64 + QEMU), sufficient disk
space for Qt/LLVM/C++26, and Docker usable without sudo. Run:

```bash
uname -m                    # Must print aarch64
git switch main
git pull --ff-only origin main
git status --short           # Must be empty
sudo apt-get update
sudo apt-get install -y software-properties-common python3-venv \
  ninja-build curl file patchelf libfuse2 zsync docker.io nodejs \
  libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 \
  libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
  libxcb-render-util0 libxcb-xinerama0 \
  libboost-dev libsqlite3-dev libssl-dev libsecret-1-dev \
  libcurl4-openssl-dev libdbus-1-dev libfontconfig1 libfreetype6 \
  libgstreamer-plugins-bad1.0-0 libgstreamer-plugins-good1.0-0 \
  desktop-file-utils
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -y gcc-16 g++-16 lld
python3 -m venv build/local-release-tools
build/local-release-tools/bin/python -m pip install 'cmake==4.4.4' aqtinstall
build/local-release-tools/bin/aqt install-qt linux_arm64 desktop 6.12.0 \
  linux_gcc_arm64 --modules qtmultimedia --outputdir Qt
export PATH="$PWD/build/local-release-tools/bin:$PATH"
export QT_ROOT_DIR="$PWD/Qt/6.12.0/gcc_arm64"
BUILD_JOBS=2 bash ci/build_local_linux_release.sh
```

The script reproduces the **release** CMake configuration, Qt deployment,
GCC 16 runtime, Qt OpenSSL 3 TLS plugin/pair, AppImage, DEB, zsync, manifest
and SHA-256 validation. Finally it runs **the same**
`ci/smoke_linux_install.sh` as GitHub: fresh Ubuntu 24.04 Docker install,
actual packaged Qt startup, TLS preflight and non-black-frame screenshots
of both DEB and AppImage. If Docker is unavailable, the script fails rather
than claiming release equivalence.

After success, deliverables are in `dist/local-linux-arm64/`. To test the
**installed** output (rather than the build-tree executable), on the same
ARM64 machine:

```bash
cd dist/local-linux-arm64
sha256sum -c SHA256SUMS
sudo apt install "./$(find . -maxdepth 1 -name '*.deb' -printf '%f\n' | head -1)"
/opt/genycaster/AppRun --check-tls
/opt/genycaster/AppRun
```

Test the actual login, provider selection, account verification, New Cast,
Enable interactions and restart on a personal non-production test account.
The graphical acceptance still requires human testing of those native flows;
offscreen smoke tests are not a substitute.

If Wayland is misdetected, test the shipped XCB fallback explicitly:

```bash
QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=opengl \
  LIBGL_ALWAYS_SOFTWARE=1 /opt/genycaster/AppRun
```

## Option 3: reproduce the latest Windows ARM64 release locally

**Host:** Native Windows 11 **ARM64** with Visual Studio 2022 C++ ARM64 SDK
and **Developer PowerShell configured for arm64**; for x64 use a native x64
Windows host and `-Architecture x86_64`. Required:
Clang-CL and `clang-scan-deps.exe` in `C:\Program Files\LLVM\bin`,
Ninja, CMake **4.4.4**, Python 3.13, 7-Zip, NSIS, and a bootstrapped
[vcpkg](https://github.com/microsoft/vcpkg).

The pinned official Qt 6.12.0 MSVC archive installer
(`ci/install_qt_windows.py`) is invoked automatically. Inside the
correct Visual Studio Developer PowerShell:

```powershell
git switch main
git pull --ff-only origin main
git status --short
$env:VCPKG_INSTALLATION_ROOT = 'C:\vcpkg'
& .\ci\build_local_windows_release.ps1 -Architecture arm64 -Jobs 3
```

This follows the CI's Clang-CL/MSVC ABI, vcpkg native packages,
QtKeychain, `windeployqt`, PE architecture/dependency audit, isolated
packaged Qt/QML smoke, Microsoft-authenticated VC runtime verification,
NSIS installer, checksums, and self-update ZIP snapshot. Install the
resulting `dist/local-windows-arm64/GenyCaster-v*-windows-arm64.exe` on
your Windows ARM64 device.

**Native acceptance:** open the installed app, verify Farcaster account,
try New Cast and Enable interactions, check expected custody approval or
recovery affordance, cancel and retry, restart and verify no process crash.
Do not submit a recovery phrase as a bug report.

If Windows exits unexpectedly, inspect Event Viewer > Windows Logs >
Application for the Genycaster.exe faulting module and exception code.
The clean-start smoke check alone cannot reproduce a signer interaction.

## Release decision

A successful compiler/link, installer hash or screenshot is *necessary*,
not sufficient. Keep the public release blocked until the **new** locally
built Windows and Linux packages pass the device acceptance paths. The
downloaded older GitHub artifacts do **not** validate the recent fixes.
