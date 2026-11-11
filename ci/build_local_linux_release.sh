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
if [[ -z "${QT_ROOT_DIR:-}" ]] || [[ ! -x "$QT_ROOT_DIR/bin/qmake6" ]] ||
   [[ ! -f "$QT_ROOT_DIR/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]]; then
  echo "Qt 6.12.0 with Qt Multimedia is not installed at QT_ROOT_DIR=${QT_ROOT_DIR:-<unset>}" >&2
  echo "A git clone does not include the Qt SDK. Install aqtinstall into a venv:" >&2
  echo "  python3 -m venv build/local-release-tools" >&2
  echo "  build/local-release-tools/bin/python -m pip install 'cmake==4.4.4' aqtinstall" >&2
  if [[ "$arch" == arm64 ]]; then
    echo "  build/local-release-tools/bin/aqt install-qt linux_arm64 desktop 6.12.0 linux_gcc_arm64 --modules qtmultimedia --outputdir Qt" >&2
    echo '  export QT_ROOT_DIR="$PWD/Qt/6.12.0/gcc_arm64"' >&2
  else
    echo "  build/local-release-tools/bin/aqt install-qt linux desktop 6.12.0 linux_gcc_64 --modules qtmultimedia --outputdir Qt" >&2
    echo '  export QT_ROOT_DIR="$PWD/Qt/6.12.0/gcc_64"' >&2
  fi
  exit 2
fi
for command in cmake ninja g++-16 gcc-16 ld.lld curl patchelf zsync zsyncmake dpkg-deb desktop-file-validate docker node; do
  command -v "$command" >/dev/null || { echo "Missing: $command" >&2; exit 2; }
done
if [[ "$(uname -m)" == "aarch64" ]]; then
  echo "Native arm64 release path selected"
fi
# Preflight tests invoke CMake, too (including the portable NSIS
# configuration contract); export the release compiler before the gate.
# Ubuntu 24.04's minimal container does not install an unversioned c++ alias.
export CC=gcc-16 CXX=g++-16
# The Qt SDK's XCB platform, Controls, TLS and Multimedia plugins carry
# dependencies that CMake does not check and linuxdeploy needs later.
# Resolve all missing SONAMEs now, before the expensive incremental C++ build.
python3 ci/preflight_linux_qt_runtime.py --qt-root "$QT_ROOT_DIR"
python3 ci/preflight_linux_miniapp_host.py --qt-root "$QT_ROOT_DIR"
python3 ci/release_gate.py --ref refs/heads/main --event workflow_dispatch
version="$(python3 -c 'from pathlib import Path; from ci.release_gate import release_designation; print(release_designation(Path.cwd()))')"
python3 -m unittest ci.test_deployment_config ci.test_audit_windows_stage ci.test_native_runtime_audits ci.test_nsis_cpack_config ci.test_packaging ci.test_linux_cpp_runtime ci.test_linux_release_parity ci.test_linux_miniapp_preflight ci.test_linux_distro_assets ci.test_macos_otool_headers
python3 ci/check_deployment_boundaries.py
python3 ci/check_media_upload_contract.py
node --test services/test/deployment_config.test.mjs
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
  -DFARCASTER_ENABLE_MINIAPPS=ON \
  -DGENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON \
  -DGENYCASTER_FETCH_QTKEYCHAIN=ON \
  -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON
grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON before linuxdeploy scans it. Qt can
# resolve at configure time despite a missing desktop/audio runtime SONAME.
missing_runtime="$(LD_LIBRARY_PATH="$QT_ROOT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ldd "$build_dir/bin/Genycaster" | grep 'not found' || true)"
if [[ -n "$missing_runtime" ]]; then
  printf 'Unresolved Linux runtime dependencies before packaging:\n%s\n' "$missing_runtime" >&2
  echo 'Install matching Ubuntu 24.04 runtime libraries in the build container.' >&2
  exit 1
fi
# A fresh AppDir is necessary; reusing a prior stage masks missing libraries.
if [[ -d "$appdir" ]]; then rm -rf -- "$appdir"; fi
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" \
  "$appdir/usr/share/icons/hicolor/512x512/apps" "$appdir/usr/share/doc/genycaster" \
  "$deploy_dir" "$outdir"
cp "$build_dir/bin/Genycaster" "$appdir/usr/bin/"
python3 ci/stage_linux_webengine.py "$appdir" --qt-root "$QT_ROOT_DIR"
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
python3 ci/bundle_linux_webengine_runtime.py "$appdir" --qt-root "$QT_ROOT_DIR"
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
(
  cd "$deploy_dir"
  export OUTPUT="GenyCaster-v$version-linux-$arch.AppImage"
  ./linuxdeploy.AppImage --appdir "$appdir" --output appimage
  test -s "$OUTPUT"
  mv -- "$OUTPUT" "$outdir/$OUTPUT"
)
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
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
 "$build_dir/CMakeCache.txt"
grep -q '^GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST:BOOL=ON before linuxdeploy scans it. Qt can
# resolve at configure time despite a missing desktop/audio runtime SONAME.
missing_runtime="$(LD_LIBRARY_PATH="$QT_ROOT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ldd "$build_dir/bin/Genycaster" | grep 'not found' || true)"
if [[ -n "$missing_runtime" ]]; then
  printf 'Unresolved Linux runtime dependencies before packaging:\n%s\n' "$missing_runtime" >&2
  echo 'Install matching Ubuntu 24.04 runtime libraries in the build container.' >&2
  exit 1
fi
# A fresh AppDir is necessary; reusing a prior stage masks missing libraries.
if [[ -d "$appdir" ]]; then rm -rf -- "$appdir"; fi
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" \
  "$appdir/usr/share/icons/hicolor/512x512/apps" "$appdir/usr/share/doc/genycaster" \
  "$deploy_dir" "$outdir"
cp "$build_dir/bin/Genycaster" "$appdir/usr/bin/"
python3 ci/stage_linux_webengine.py "$appdir" --qt-root "$QT_ROOT_DIR"
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
python3 ci/bundle_linux_webengine_runtime.py "$appdir" --qt-root "$QT_ROOT_DIR"
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
(
  cd "$deploy_dir"
  export OUTPUT="GenyCaster-v$version-linux-$arch.AppImage"
  ./linuxdeploy.AppImage --appdir "$appdir" --output appimage
  test -s "$OUTPUT"
  mv -- "$OUTPUT" "$outdir/$OUTPUT"
)
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
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
 "$build_dir/CMakeCache.txt"
cmake --build "$build_dir" --target farcaster_miniapp_browser --parallel "$jobs"
cmake --build "$build_dir" --target genycaster --parallel "$jobs"
test -x "$build_dir/bin/Genycaster"
# Inspect the real linked executable before linuxdeploy scans it. Qt can
# resolve at configure time despite a missing desktop/audio runtime SONAME.
missing_runtime="$(LD_LIBRARY_PATH="$QT_ROOT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ldd "$build_dir/bin/Genycaster" | grep 'not found' || true)"
if [[ -n "$missing_runtime" ]]; then
  printf 'Unresolved Linux runtime dependencies before packaging:\n%s\n' "$missing_runtime" >&2
  echo 'Install matching Ubuntu 24.04 runtime libraries in the build container.' >&2
  exit 1
fi
# A fresh AppDir is necessary; reusing a prior stage masks missing libraries.
if [[ -d "$appdir" ]]; then rm -rf -- "$appdir"; fi
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" \
  "$appdir/usr/share/icons/hicolor/512x512/apps" "$appdir/usr/share/doc/genycaster" \
  "$deploy_dir" "$outdir"
cp "$build_dir/bin/Genycaster" "$appdir/usr/bin/"
python3 ci/stage_linux_webengine.py "$appdir" --qt-root "$QT_ROOT_DIR"
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
python3 ci/bundle_linux_webengine_runtime.py "$appdir" --qt-root "$QT_ROOT_DIR"
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
(
  cd "$deploy_dir"
  export OUTPUT="GenyCaster-v$version-linux-$arch.AppImage"
  ./linuxdeploy.AppImage --appdir "$appdir" --output appimage
  test -s "$OUTPUT"
  mv -- "$OUTPUT" "$outdir/$OUTPUT"
)
python3 ci/audit_linux_appdir.py "$appdir" --require-miniapps
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
