#!/usr/bin/env bash
# Provision a separate official Qt 6.12.0 Android ARM64 SDK on macOS.
# Never overwrite the existing Qt installation or suppress TLS feature checks.
set -euo pipefail

qt_version="6.12.0"
qt_arch="android_arm64_v8a"
existing_qt="${HOME}/Qt"
staging="${GENYCASTER_ANDROID_QT_TLS_STAGING_DIR:-${HOME}/Qt-genycaster-tls}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv="${repo_root}/build/android-qt-tls-installer-venv"
host_path="${GENYCASTER_QT_HOST_PATH:-${existing_qt}/${qt_version}/host}"

usage() {
    cat <<'HELP'
Usage: bash tools/provision_android_qt_tls_macos.sh [--dry-run | --check <sdk>]

  (default) Install an isolated Qt 6.12.0 Android ARM64 SDK using official
            Qt distribution archives via aqtinstall; verify SSL support.
  --dry-run Show intended changes without installing anything.
  --check   Verify an existing Android SDK without modifying it.

Default staged SDK: ~/Qt-genycaster-tls/6.12.0/android_arm64_v8a
GENYCASTER_ANDROID_QT_TLS_STAGING_DIR overrides the staging directory.
GENYCASTER_QT_HOST_PATH overrides the existing host Qt toolchain.
The script NEVER patches feature headers or disables certificate checks.
HELP
}

locate_sdk() {
    local root="$1"
    if [[ -f "${root}/include/QtNetwork/qtnetwork-config.h" ]]; then
        printf '%s\n' "$root"
    elif [[ -f "${root}/${qt_version}/${qt_arch}/include/QtNetwork/qtnetwork-config.h" ]]; then
        printf '%s\n' "${root}/${qt_version}/${qt_arch}"
    elif [[ -f "${root}/${qt_version}/android-arm64-v8a/include/QtNetwork/qtnetwork-config.h" ]]; then
        printf '%s\n' "${root}/${qt_version}/android-arm64-v8a"
    else
        printf 'Qt Network feature header missing under: %s\n' "$root" >&2
        return 1
    fi
}

check_sdk() {
    local sdk="$1" config="${1}/include/QtNetwork/qtnetwork-config.h"
    if [[ ! -r "$config" ]]; then
        printf 'Missing Qt Network feature header: %s\n' "$config" >&2
        return 1
    fi
    local feature
    feature="$(grep -E '^#define[[:space:]]+QT_FEATURE_ssl[[:space:]]+(-1|1)([[:space:]]|$)' "$config" || true)"
    printf 'Qt SDK: %s\nSSL feature: %s\n' "$sdk" "${feature:-<missing>}"
    if [[ ! "$feature" =~ ^#define[[:space:]]+QT_FEATURE_ssl[[:space:]]+1([[:space:]]|$) ]]; then
        printf 'Qt Network was built without SSL; do not edit qtnetwork-config.h.\n' >&2
        return 2
    fi
    for required in \
        "lib/cmake/Qt6/Qt6Config.cmake" \
        "lib/cmake/Qt6Network/Qt6NetworkConfig.cmake" \
        "lib/cmake/Qt6Svg/Qt6SvgConfig.cmake" \
        "lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake"; do
        if [[ ! -f "${sdk}/${required}" ]]; then
            printf 'Incomplete Qt kit; missing: %s\n' "${sdk}/${required}" >&2
            return 3
        fi
    done
    printf '\nTLS-capable Qt 6.12.0 Android ARM64 SDK verified.\n'
    printf 'Use this Android Qt SDK in a separate Qt Creator Release kit:\n'
    printf 'CMAKE_PREFIX_PATH=%s\n' "$sdk"
    printf 'QT_HOST_PATH=%s\n' "$host_path"
    printf 'Keep GENYCASTER_ANDROID_OPENSSL_ROOT at build/android-deps.\n'
    printf 'Use a new Release build directory; do not reuse a stale CMake cache.\n'
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --check)
        if [[ "$#" -ne 2 ]]; then usage >&2; exit 2; fi
        check_sdk "$(locate_sdk "$2")"
        exit $?
        ;;
    --dry-run)
        printf 'Host Qt: %s\nTarget directory: %s\n' "$host_path" "$staging"
        printf 'Will run: aqt install-qt all_os android %s %s -m qtmultimedia -O "%s"\n' \
            "$qt_version" "$qt_arch" "$staging"
        printf 'The existing Qt install and CMake cache are untouched.\n'
        exit 0 ;;
    "") ;;
    *) usage >&2; exit 2 ;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
    printf 'This installer is for macOS; use --check for another host.\n' >&2
    exit 2
fi
if [[ ! -d "$host_path" ]]; then
    printf 'Qt 6.12.0 macOS host tools not found: %s\n' "$host_path" >&2
    printf 'Set GENYCASTER_QT_HOST_PATH to the correct prefix.\n' >&2
    exit 2
fi
if [[ "$staging" == "$existing_qt" || "$staging" == "${existing_qt}/" ]]; then
    printf 'Refusing to overwrite existing Qt installation: %s\n' "$staging" >&2
    exit 2
fi

if [[ -r "${staging}/${qt_version}/${qt_arch}/include/QtNetwork/qtnetwork-config.h" ]]; then
    check_sdk "${staging}/${qt_version}/${qt_arch}"
    exit $?
fi

command -v python3 >/dev/null || { echo 'Python 3 required.' >&2; exit 2; }
python3 -m venv "$venv"
"${venv}/bin/python" -m pip install --disable-pip-version-check 'aqtinstall>=3.1,<4'
"${venv}/bin/aqt" list-qt all_os android --arch "$qt_version"
"${venv}/bin/aqt" install-qt all_os android "$qt_version" "$qt_arch" \
    -m qtmultimedia -O "$staging"

sdk="$(locate_sdk "$staging")"
if ! check_sdk "$sdk"; then
    cat >&2 <<'WARN'
The official Qt package in this separate directory also lacks SSL.
Your existing Qt installation is unchanged. The remaining approach is
building Qt for Android from source with the SSL feature enabled:
https://doc.qt.io/qt-6.12/android-building.html
Do not swap headers or shared libraries between Qt SDKs.
WARN
    exit 2
fi
