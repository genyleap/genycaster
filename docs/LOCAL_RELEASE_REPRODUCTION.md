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

### Native Linux Mini Apps (separate WebEngine dependency)

Qt 6.12 does **not** ship Qt WebEngine 6.12 modules. The native in-app
Mini App browser requires the separately released, compatible **Qt WebEngine
6.140**, plus Qt WebChannel. The existing Linux ARM64 AppImage/DEB built
with the base Qt 6.12 Multimedia SDK has no Qt WebEngine renderer and must
**not** represent the Mini App as an active, wallet-capable session or
silently open Firefox instead.

The optional, origin-scoped native Linux browser source is
`ui/app/miniapp/browser_linux.cpp`. Once matching WebEngine developer
headers, libraries, native subprocess, resources, ICU data and translations
have been independently provisioned for the Qt 6.12 ARM64 SDK, it can be
configured explicitly with:

```bash
cmake -S . -B build/linux-miniapp-experimental -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DFARCASTER_ENABLE_MINIAPPS=ON \
  -DGENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON
```

**Do not use this command as a release recipe yet.** The experimental Linux
WebEngine adapter and subprocess/resource packaging have **not** passed a
native ARM64 compile, security, runtime or clean-install acceptance test.
The default proven local and GitHub Linux release paths deliberately keep
the optional browser disabled, preserving their previous compile and
packaging behavior. A normal `aqt install-qt ... --modules qtwebengine`
must not be added to the Qt 6.12 job: Qt WebEngine 6.140 is a separate
extension, not a regular 6.12 add-on. Never combine Ubuntu distro Qt
WebEngine libraries of a different Qt major/minor version with the Qt
6.12 application runtime.

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
The Ubuntu image also installs the OpenGL/GLX **development**
libraries and tests them with CMake before starting GenyCaster.
Qt6Gui/QtQuick cannot be configured when only the `libgl1` runtime
library is installed: the `WrapOpenGL` dependency requires headers and
development linker libraries. After updating the Dockerfile, the apt layer
will be rebuilt once; the existing downloaded Qt ARM64 SDK is reused.
The minimal container also explicitly includes PulseAudio's `libpulse0`
(`libpulse.so.0`) and ALSA's `libasound2t64`. Unlike Qt and GCC,
these runtime libraries are not supplied by the project source and must be
available while linuxdeploy resolves dependencies of the final ELF binary.
After this Dockerfile change, the Docker package layer is rebuilt; the
existing C++ object files in `build/local-linux-arm64` are preserved and
Ninja reuses them instead of recompiling the entire application.

The ARM64 Qt platform plugin also needs the X11 session-management libraries `libice6` (`libICE.so.6`) and `libsm6` (`libSM.so.6`). Qt Multimedia's FFmpeg plugin needs `libxrandr2` (`libXrandr.so.2`). The preflight rejects **any installed multimedia plugin** that cannot load, even when another installed plugin is usable, because Qt may select the broken backend at runtime.

Qt's XCB platform plugin also depends on multiple XCB extension libraries,
including `libxcb-shape0`, `libxcb-randr0`, `libxcb-shm0`,
`libxcb-xfixes0`, `libxcb-xkb1`, and `libxcb-xinput0`. The local
Ubuntu 24.04 image and hosted Linux workflow install the complete platform
dependency set. Before CMake configures the product, the local build now runs
`ci/preflight_linux_qt_runtime.py --qt-root "$QT_ROOT_DIR"` to collect **all**
unresolved `ldd` dependencies from Qt's required XCB, Quick Controls,
Basic style, and TLS plugins, and checks for a usable multimedia backend.
If it fails, fix that dependency list rather than repeatedly launching an
expensive C++26 compile. The previous Ninja object cache remains untouched.


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
docker run --rm --network host ubuntu:24.04 \
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
  libgl1 libegl1 libgl1-mesa-dev libopengl-dev libglx-dev libegl-dev libx11-dev \
  libxkbcommon-x11-0 libxcb-cursor0 \
  libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
  libxcb-render-util0 libxcb-xinerama0 \
  libxcb-shape0 libxcb-randr0 libxcb-shm0 libxcb-sync1 \
  libxcb-xfixes0 libxcb-xkb1 libxcb-glx0 libxcb-xinput0 \
  libxcb-render0 libxcb-util1 libx11-xcb1 libxcb-dri3-0 \
  libice6 libsm6 libxrandr2 \
  libboost-dev libsqlite3-dev libssl-dev libsecret-1-dev \
  libcurl4-openssl-dev libdbus-1-dev libfontconfig1 libfreetype6 \
  libgstreamer-plugins-bad1.0-0 libgstreamer-plugins-good1.0-0 \
  libpulse0 libasound2t64 desktop-file-utils
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
