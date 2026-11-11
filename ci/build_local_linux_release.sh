#!/usr/bin/env bash
# Offline-from-GitHub-Actions Linux release reproduction; no CI minutes used.
# Run on native Ubuntu 24.04 arm64 or x86_64 after installing CI prerequisites.
set -euo pipefail
cd "$(dirname "$0")/.."
repo="$PWD"
case "$(uname -m)" in
  aarch64) arch=arm64; ai_arch=aarch64 ;;
  x86_64) arch=x86_64; ai_arch=x86_64 ;;
  *) echo "Native Linux arm64/x86_64 host required" >&2; exit 2 ;;
esac
if [[ "$#" != 0 ]]; then
  echo "Usage: QT_ROOT_DIR=/path/to/Qt/6.12.0/gcc_arm64 bash ci/build_local_linux_release.sh" >&2
  exit 2
fi
if [[ "$(git branch --show-current)" != "main" ]] || [[ -n "$(git status --porcelain)" ]]; then
  echo "Checkout main and commit/stash local edits before release-equivalent builds" >&2
  exit 2
fi
if [[ -z "$(printenv QT_ROOT_DIR || true)" ]] || [[ ! -x "$QT_ROOT_DIR/bin/qmake6" ]]; then
  echo "Set QT_ROOT_DIR to Qt 6.12.0 with Qt Multimedia installed" >&2
  exit 2
fi
for command in cmake ninja g++-16 gcc-16 ld.lld curl patchelf zsync zsyncmake dpkg-deb desktop-file-validate docker node; do
  command -v "$command" >/dev/null || { echo "Missing: $command" >&2; exit 2; }
done
if [[ "$(uname -m)" == "aarch64" ]]; then
  echo "Native arm64 release path selected"
fi
python3 ci/release_gate.py --ref refs/heads/main --event workflow_dispatch
version="$(python3 -c 'from pathlib import Path; from ci.release_gate import release_designation; print(release_designation(Path.cwd()))')"
python3 -m unittest ci.test_deployment_config ci.test_audit_windows_stage ci.test_native_runtime_audits ci.test_nsis_cpack_config ci.test_packaging ci.test_linux_cpp_runtime
python3 ci/check_deployment_boundaries.py
python3 ci/check_media_upload_contract.py
node --test services/test/deployment_config.test.mjs
export CC=gcc-16 CXX=g++-16
build_dir="$repo/build/local-linux-$arch"
appdir="$repo/build/local-AppDir-$arch"
deploy_dir="$repo/build/local-linuxdeploy-$arch"
outdir="$repo/dist/local-linux-$arch"
jobs="$(printenv BUILD_JOBS || echo 2)"
cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_PREFIX_PATH=$QT_ROOT_DIR" \
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld \
  -DFARCASTER_BUILD_TESTS=OFF \
  -DGENYCASTER_FETCH_QTKEYCHAIN=ON \
  -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON
cmake --build "$build_dir" --target genycaster --parallel "$jobs"
test -x "$build_dir/bin/Genycaster"
# A fresh AppDir is necessary; reusing a prior stage masks missing libraries.
if [[ -d "$appdir" ]]; then rm -rf -- "$appdir"; fi
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" \
  "$appdir/usr/share/icons/hicolor/512x512/apps" "$appdir/usr/share/doc/genycaster" \
  "$deploy_dir" "$outdir"
cp "$build_dir/bin/Genycaster" "$appdir/usr/bin/"
install -m 755 /usr/bin/zsync "$appdir/usr/bin/zsync"
cp /usr/share/doc/zsync/copyright "$appdir/usr/share/doc/genycaster/zsync-copyright"
cp properties/Linux/genycaster.png "$appdir/usr/share/icons/hicolor/512x512/apps/genycaster.png"
cp properties/Linux/genycaster.desktop "$appdir/usr/share/applications/genycaster.desktop"
desktop-file-validate "$appdir/usr/share/applications/genycaster.desktop"
curl --fail --location --retry 3 -o "$deploy_dir/linuxdeploy.AppImage" \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ai_arch.AppImage"
curl --fail --location --retry 3 -o "$deploy_dir/linuxdeploy-plugin-qt.AppImage" \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ai_arch.AppImage"
chmod +x "$deploy_dir/"*.AppImage
cp "$deploy_dir/linuxdeploy-plugin-qt.AppImage" "$deploy_dir/linuxdeploy-plugin-qt"
chmod +x "$deploy_dir/linuxdeploy-plugin-qt"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$QT_ROOT_DIR/bin/qmake6"
export QML_SOURCES_PATHS="$repo/ui"
export PATH="$deploy_dir:$PATH"
"$deploy_dir/linuxdeploy.AppImage" --appdir "$appdir" \
  --desktop-file "$appdir/usr/share/applications/genycaster.desktop" \
  --icon-file "$appdir/usr/share/icons/hicolor/512x512/apps/genycaster.png" \
  --plugin qt
python3 ci/prune_linux_qt_plugins.py "$appdir"
python3 ci/bundle_linux_cpp_runtime.py "$appdir" --compiler g++-16
python3 ci/bundle_linux_tls_runtime.py "$appdir" --qt-root "$QT_ROOT_DIR"
python3 ci/audit_linux_appdir.py "$appdir"
(
  cd "$deploy_dir"
  export OUTPUT="GenyCaster-v$version-linux-$arch.AppImage"
  ./linuxdeploy.AppImage --appdir "$appdir" --output appimage
  test -s "$OUTPUT"
  mv -- "$OUTPUT" "$outdir/$OUTPUT"
)
python3 ci/audit_linux_appdir.py "$appdir"
appimage="$outdir/GenyCaster-v$version-linux-$arch.AppImage"
deb="$outdir/GenyCaster-v$version-linux-$arch.deb"
python3 ci/verify_platform_package.py linux "$appimage"
zsyncmake -u "https://github.com/genyleap/genycaster/releases/download/v$version/$(basename "$appimage")" \
  -o "$appimage.zsync" "$appimage"
python3 ci/package_linux_deb.py --appdir "$appdir" --output "$outdir" \
  --version "$version" --arch "$arch"
python3 ci/verify_platform_package.py linux-deb "$deb"
# This is the release gate: clean Ubuntu container, actual installed DEB,
# packaged AppImage, Qt/OpenSSL TLS preflight and nonblack rendered screenshots.
bash ci/smoke_linux_install.sh "$deb" "$appimage"
(cd "$outdir" && sha256sum ./*.AppImage ./*.deb > SHA256SUMS)
printf '\nLocal %s release-equivalent packages: %s\n' "$arch" "$outdir"
