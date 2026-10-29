#!/usr/bin/env bash
# Build a TLS-capable Qt 6.12.0 Android ARM64 SDK from the user's existing Qt
# source checkout, in an isolated shadow build and install prefix.
set -euo pipefail

qt_root="${GENYCASTER_QT_ROOT:-${HOME}/Qt}"
source_dir="${GENYCASTER_QT_SOURCE:-${qt_root}/Src/qt-everywhere-src-6.12.0}"
host_qt="${GENYCASTER_QT_HOST_PATH:-${qt_root}/6.12.0/host}"
existing_qt="${qt_root}/6.12.0/android-arm64-v8a"
build_dir="${GENYCASTER_QT_ANDROID_TLS_BUILD_DIR:-${qt_root}/Build/android-arm64-v8a-tls}"
install_dir="${GENYCASTER_QT_ANDROID_TLS_PREFIX:-${qt_root}/6.12.0/android-arm64-v8a-tls}"
android_sdk="${ANDROID_SDK_ROOT:-${HOME}/Library/Android/sdk}"
android_ndk="${ANDROID_NDK_ROOT:-${android_sdk}/ndk/27.2.12479018}"
openssl_root="${GENYCASTER_ANDROID_OPENSSL_ROOT:-${HOME}/Developer/genycaster/build/android-deps}"
jobs="${GENYCASTER_QT_BUILD_JOBS:-4}"

usage() {
    cat <<'EOF'
Build TLS-enabled Qt 6.12.0 Android arm64-v8a from an EXISTING source checkout.

Usage:
  bash tools/rebuild_android_qt_tls_macos.sh --inspect
  bash tools/rebuild_android_qt_tls_macos.sh --configure
  bash tools/rebuild_android_qt_tls_macos.sh --build
  bash tools/rebuild_android_qt_tls_macos.sh --install
  bash tools/rebuild_android_qt_tls_macos.sh --check
  bash tools/rebuild_android_qt_tls_macos.sh --all

--inspect   Print existing build cache settings and required locations.
--configure Create a separate Qt shadow build, with OpenSSL runtime TLS.
--build     Build the configured Qt SDK from sources (default 4 jobs).
--install   Install only after successful build, then check SSL support.
--check     Confirm the installed Qt has QtNetwork SSL and required modules.
--all       Configure, build and install.

Override source/build/installed paths through:
  GENYCASTER_QT_SOURCE, GENYCASTER_QT_ANDROID_TLS_BUILD_DIR,
  GENYCASTER_QT_ANDROID_TLS_PREFIX, GENYCASTER_QT_HOST_PATH,
  GENYCASTER_ANDROID_OPENSSL_ROOT, GENYCASTER_QT_BUILD_JOBS.

The existing Qt 6.12.0 Android SDK, source checkout and previous build trees
will not be modified. Do not patch QT_FEATURE_ssl in a generated header.
EOF
}

inspect() {
    printf 'Qt source:        %s\n' "$source_dir"
    printf 'Qt host:          %s\n' "$host_qt"
    printf 'Old Qt target:    %s\n' "$existing_qt"
    printf 'New shadow build: %s\n' "$build_dir"
    printf 'New Qt install:   %s\n' "$install_dir"
    printf 'Android SDK:      %s\n' "$android_sdk"
    printf 'Android NDK:      %s\n' "$android_ndk"
    printf 'Android OpenSSL:  %s\n' "$openssl_root"
    for cache in \
        "${qt_root}/Build/android-arm64-v8a/CMakeCache.txt" \
        "${qt_root}/Build/6.12.0/android-arm64-v8a/CMakeCache.txt"; do
        if [[ -f "$cache" ]]; then
            printf '\nPrevious Qt configuration: %s\n' "$cache"
            grep -E '^(CMAKE_INSTALL_PREFIX|QT_HOST_PATH|ANDROID_ABI|QT_ANDROID_ABIS|FEATURE_ssl|FEATURE_openssl|OPENSSL_ROOT_DIR|OPENSSL_INCLUDE_DIR|OPENSSL_SSL_LIBRARY|OPENSSL_CRYPTO_LIBRARY):' "$cache" || true
        fi
    done
    if [[ -f "${existing_qt}/include/QtNetwork/qtnetwork-config.h" ]]; then
        grep '^#define QT_FEATURE_ssl ' \
            "${existing_qt}/include/QtNetwork/qtnetwork-config.h" || true
    fi
}

preflight() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo 'This helper targets macOS; Qt for Android uses a macOS host toolchain.' >&2
        exit 2
    fi
    if [[ ! -x "${source_dir}/configure" || ! -f "${source_dir}/qtbase/CMakeLists.txt" ]]; then
        printf 'Qt 6.12.0 source tree not found: %s\n' "$source_dir" >&2
        exit 2
    fi
    if [[ ! -f "${host_qt}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        printf 'Matching Qt 6.12.0 host installation missing: %s\n' "$host_qt" >&2
        exit 2
    fi
    if [[ ! -d "$android_sdk" || ! -f "$android_ndk/build/cmake/android.toolchain.cmake" ]]; then
        echo 'Android SDK / NDK 27.2.12479018 not found.' >&2
        exit 2
    fi
    for file in \
        "${openssl_root}/include/openssl/opensslv.h" \
        "${openssl_root}/lib/libcrypto_3.so" \
        "${openssl_root}/lib/libssl_3.so"; do
        if [[ ! -s "$file" ]]; then
            printf 'Missing Android OpenSSL file: %s\n' "$file" >&2
            printf 'Build it first: bash tools/build_android_openssl.sh\n' >&2
            exit 2
        fi
    done
    for utility in cmake ninja perl python3; do
        if ! command -v "$utility" >/dev/null 2>&1; then
            printf 'Missing required tool: %s\n' "$utility" >&2
            exit 2
        fi
    done
    if [[ "$install_dir" == "$existing_qt" || "$build_dir" == "$install_dir" ||
          "$build_dir" == "$source_dir" || "$install_dir" == "$source_dir" ]]; then
        echo 'Refusing to overwrite existing Qt SDK/source or combine build and install.' >&2
        exit 2
    fi
    if [[ -f "$build_dir/CMakeCache.txt" ]]; then
        printf 'Shadow build already configured: %s\n' "$build_dir"
        printf 'Use --build/--install, or choose a fresh GENYCASTER_QT_ANDROID_TLS_BUILD_DIR.\n' >&2
        exit 2
    fi
}

configure_qt() {
    preflight
    mkdir -p "$build_dir"
    (
        cd "$build_dir"
        "$source_dir/configure" \
            -prefix "$install_dir" \
            -qt-host-path "$host_qt" \
            -android-sdk "$android_sdk" \
            -android-ndk "$android_ndk" \
            -android-abis arm64-v8a \
            -release -shared -nomake tests -nomake examples \
            -submodules qtbase,qtdeclarative,qtsvg,qtmultimedia,qtshadertools,qttools,qttranslations,qtimageformats \
            -openssl-runtime -feature-ssl \
            -- \
            "-DOPENSSL_ROOT_DIR=$openssl_root" \
            "-DOPENSSL_INCLUDE_DIR=$openssl_root/include" \
            "-DOPENSSL_CRYPTO_LIBRARY=$openssl_root/lib/libcrypto_3.so" \
            "-DOPENSSL_SSL_LIBRARY=$openssl_root/lib/libssl_3.so" \
            -DQT_INSTALL_CONFIG_INFO_FILES=ON
    )
    printf '\nConfigured Qt with explicit Android OpenSSL headers and AArch64 libraries.\n'
    printf 'Next: bash tools/rebuild_android_qt_tls_macos.sh --build\n'
}

build_qt() {
    if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
        echo 'No configured shadow build. Run --configure first.' >&2
        exit 2
    fi
    cmake --build "$build_dir" --parallel "$jobs"
}

check_qt() {
    local feature_file="${install_dir}/include/QtNetwork/qtnetwork-config.h"
    if [[ ! -r "$feature_file" ]]; then
        printf 'Qt Network feature header missing: %s\n' "$feature_file" >&2
        exit 2
    fi
    local feature
    feature="$(grep '^#define QT_FEATURE_ssl ' "$feature_file" || true)"
    printf 'Qt target: %s\nQt Network: %s\n' "$install_dir" "$feature"
    if [[ "$feature" != '#define QT_FEATURE_ssl 1' ]]; then
        echo 'SSL remains disabled. Do not edit feature headers or use this kit.' >&2
        exit 2
    fi
    for module in Qt6 Qt6Network Qt6Quick Qt6Qml Qt6QuickControls2 Qt6Multimedia Qt6Svg Qt6LinguistTools; do
        if [[ ! -r "$install_dir/lib/cmake/$module/$module""Config.cmake" ]]; then
            printf 'Missing required Qt module: %s\n' "$module" >&2
            exit 2
        fi
    done
    printf '\nVerified TLS-enabled Qt Android ARM64 SDK: %s\n' "$install_dir"
    printf 'Add its bin/qmake6 in Qt Creator > Preferences > Kits > Qt Versions.\n'
    printf 'Create a separate Android Release kit/build directory; keep Android OpenSSL prefix unchanged.\n'
}

action="${1:---inspect}"
case "$action" in
    --inspect) inspect ;;
    --configure) configure_qt ;;
    --build) build_qt ;;
    --install)
        if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
            echo 'No configured shadow build. Run --configure first.' >&2; exit 2
        fi
        cmake --install "$build_dir"
        check_qt ;;
    --check) check_qt ;;
    --all)
        configure_qt
        build_qt
        cmake --install "$build_dir"
        check_qt ;;
    --help|-h) usage ;;
    *) usage >&2; exit 2 ;;
esac
