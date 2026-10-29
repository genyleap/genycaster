#!/usr/bin/env bash
# Build a current OpenSSL LTS for the Android arm64-v8a ABI used by Genycaster.
# This script deliberately never uses host/Homebrew OpenSSL libraries.
set -euo pipefail

version="3.5.9"
api_level="28"
ndk="${ANDROID_NDK_ROOT:-${ANDROID_NDK:-${HOME}/Library/Android/sdk/ndk/27.2.12479018}}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${GENYCASTER_ANDROID_OPENSSL_ROOT:-${repo_root}/build/android-deps}"
url="https://github.com/openssl/openssl/releases/download/openssl-${version}/openssl-${version}.tar.gz"

for tool in curl perl make tar shasum patchelf; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\n' "${tool}" >&2
        exit 1
    fi
done

toolchain=""
for host in darwin-x86_64 darwin-arm64 linux-x86_64; do
    if [[ -x "${ndk}/toolchains/llvm/prebuilt/${host}/bin/clang" ]]; then
        toolchain="${ndk}/toolchains/llvm/prebuilt/${host}/bin"
        break
    fi
done
if [[ -z "${toolchain}" ]]; then
    printf 'Android NDK LLVM toolchain not found under: %s\n' "${ndk}" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
archive="${work}/openssl-${version}.tar.gz"
checksum="${archive}.sha256"
curl --fail --location --retry 3 --silent --show-error "${url}" --output "${archive}"
curl --fail --location --retry 3 --silent --show-error "${url}.sha256" --output "${checksum}"
expected="$(awk '{print $1; exit}' "${checksum}")"
actual="$(shasum -a 256 "${archive}" | awk '{print $1}')"
if [[ ! "${expected}" =~ ^[0-9a-fA-F]{64}$ ]] || [[ "${expected}" != "${actual}" ]]; then
    printf 'OpenSSL source SHA-256 verification failed.\n' >&2
    exit 1
fi

tar -xzf "${archive}" -C "${work}"
export ANDROID_NDK_ROOT="${ndk}"
export ANDROID_NDK_HOME="${ndk}"
export PATH="${toolchain}:${PATH}"
cd "${work}/openssl-${version}"
./Configure android-arm64 shared no-tests -D__ANDROID_API__="${api_level}"

# Qt 6's documented Android build produces *unversioned* shared objects in
# the OpenSSL source/build directory. Do not use 'make install_sw' here: its
# installed paths and sonames do not match the Android APK's required names.
printf 'Compiling OpenSSL %s for Android arm64-v8a (API %s)...\n' "${version}" "${api_level}"
make -s -j "${GENYCASTER_ANDROID_BUILD_JOBS:-4}" SHLIB_VERSION_NUMBER= build_libs
for library in libcrypto.so libssl.so; do
    if [[ ! -f "${library}" ]]; then
        printf 'Missing cross-built OpenSSL library: %s\n' "${PWD}/${library}" >&2
        exit 1
    fi
done

# Configure generated the target-specific OpenSSL headers in include/openssl.
# Copy those headers and the just-built ELF binaries without installing the
# unused OpenSSL command-line application onto the build host.
mkdir -p "${output}/include" "${output}/lib"
cp -R include/openssl "${output}/include/"
cp -L libcrypto.so "${output}/lib/libcrypto_3.so"
cp -L libssl.so "${output}/lib/libssl_3.so"
patchelf --set-soname libcrypto_3.so "${output}/lib/libcrypto_3.so"
patchelf --set-soname libssl_3.so "${output}/lib/libssl_3.so"

# OpenSSL 3's build can refer to either filename depending on its shared-
# library version configuration. Rebind libssl to the packaged libcrypto.
for needed in libcrypto.so libcrypto.so.3; do
    if patchelf --print-needed "${output}/lib/libssl_3.so" | grep -Fqx "${needed}"; then
        patchelf --replace-needed "${needed}" libcrypto_3.so "${output}/lib/libssl_3.so"
    fi
done
if ! patchelf --print-needed "${output}/lib/libssl_3.so" | grep -Fqx libcrypto_3.so; then
    printf 'Android libssl is not linked to packaged libcrypto_3.so\n' >&2
    exit 1
fi

# Verify the output is Android AArch64 ELF, not a macOS/Homebrew binary.
if [[ ! -x "${toolchain}/llvm-readelf" ]]; then
    printf 'Android NDK llvm-readelf not found: %s\n' "${toolchain}/llvm-readelf" >&2
    exit 1
fi
for library in libcrypto_3.so libssl_3.so; do
    if ! "${toolchain}/llvm-readelf" -h "${output}/lib/${library}" | grep -Eq 'Machine:[[:space:]]+AArch64'; then
        printf 'Not an Android AArch64 ELF library: %s\n' "${library}" >&2
        exit 1
    fi
done
printf 'Built Android AArch64 ELF libraries:\n'
"${toolchain}/llvm-readelf" -d "${output}/lib/libssl_3.so" | grep NEEDED
printf 'Android arm64-v8a OpenSSL %s installed at: %s\n' "${version}" "${output}"
printf 'CMake: -DGENYCASTER_ANDROID_OPENSSL_ROOT=%s\n' "${output}"
