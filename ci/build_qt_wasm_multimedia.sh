#!/usr/bin/env bash
# Qt 6.12 WASM Multimedia is technology preview and absent from aqt Qt addons.
# Compile the official checksum-verified module against installed Qt WASM.
set -euo pipefail
: "${QT_ROOT_DIR:?Qt WebAssembly SDK root missing}"
: "${QT_HOST_PATH:?Matching native Qt host tools missing}"

version=6.12.1
expected=5a125bc2bede8c6512296e0f3204a9922741fd144fcae7d4a66b3c007bb125e3
archive="${RUNNER_TEMP:-/tmp}/qtmultimedia-${version}.tar.xz"
url="https://download.qt.io/official_releases/qt/6.12/${version}/submodules/qtmultimedia-everywhere-src-${version}.tar.xz"

curl --fail --location --retry 3 --silent --show-error "$url" --output "$archive"
actual="$(sha256sum "$archive" | awk '{print $1}')"
if [[ "$actual" != "$expected" ]]; then
    echo "::error::Qt Multimedia source checksum mismatch" >&2
    exit 1
fi

mkdir -p build/qt-wasm-multimedia-source
tar -xJf "$archive" -C build/qt-wasm-multimedia-source --strip-components=1

# Qt Multimedia's shader packaging helper belongs to Qt ShaderTools. The
# 6.12 WebAssembly prebuilt SDK omits it; build the matching module first.
shader_archive="${RUNNER_TEMP:-/tmp}/qtshadertools-${version}.tar.xz"
shader_url="https://download.qt.io/official_releases/qt/6.12/${version}/submodules/qtshadertools-everywhere-src-${version}.tar.xz"
shader_expected=f8c5b9c91eca0927198918c537d1b4d515f96dc9a3b178a88a1ff1d2f85c7877
curl --fail --location --retry 3 --silent --show-error "$shader_url" --output "$shader_archive"
shader_actual="$(sha256sum "$shader_archive" | awk '{print $1}')"
if [[ "$shader_actual" != "$shader_expected" ]]; then
    echo "::error::Qt ShaderTools source checksum mismatch" >&2
    exit 1
fi
mkdir -p build/qt-wasm-shadertools-source
tar -xJf "$shader_archive" -C build/qt-wasm-shadertools-source --strip-components=1
cmake -S build/qt-wasm-shadertools-source \
    -B build/qt-wasm-shadertools -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_TOOLCHAIN_FILE=${QT_ROOT_DIR}/lib/cmake/Qt6/qt.toolchain.cmake" \
    "-DQT_HOST_PATH=${QT_HOST_PATH}" \
    "-DCMAKE_INSTALL_PREFIX=${QT_ROOT_DIR}" \
    -DQT_BUILD_TESTS=OFF \
    -DQT_BUILD_EXAMPLES=OFF
cmake --build build/qt-wasm-shadertools --parallel 3
cmake --install build/qt-wasm-shadertools
test -s "${QT_ROOT_DIR}/lib/cmake/Qt6ShaderTools/Qt6ShaderToolsConfig.cmake" || {
    echo "::error::Qt6ShaderTools not installed for WebAssembly" >&2
    exit 1
}

# WebAssembly uses its browser media backend, not FFmpeg/native devices.
cmake -S build/qt-wasm-multimedia-source \
    -B build/qt-wasm-multimedia -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_TOOLCHAIN_FILE=${QT_ROOT_DIR}/lib/cmake/Qt6/qt.toolchain.cmake" \
    "-DQT_HOST_PATH=${QT_HOST_PATH}" \
    "-DCMAKE_INSTALL_PREFIX=${QT_ROOT_DIR}" \
    -DFEATURE_ffmpeg=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DQT_BUILD_EXAMPLES=OFF
cmake --build build/qt-wasm-multimedia --parallel 3
cmake --install build/qt-wasm-multimedia

test -s "${QT_ROOT_DIR}/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" || {
    echo "::error::Qt6Multimedia not installed for WebAssembly" >&2
    exit 1
}
echo "Qt Multimedia 6.12 WebAssembly module installed from verified source"
