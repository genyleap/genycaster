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
stage="${work}/installed"
export ANDROID_NDK_ROOT="${ndk}"
export ANDROID_NDK_HOME="${ndk}"
export PATH="${toolchain}:${PATH}"
cd "${work}/openssl-${version}"
./Configure android-arm64 shared no-tests -D__ANDROID_API__="${api_level}" \
    --prefix="${stage}" --openssldir="${stage}/ssl" --libdir=lib
make -j "${GENYCASTER_ANDROID_BUILD_JOBS:-4}" build_libs
make install_sw

mkdir -p "${output}/include" "${output}/lib"
cp -R "${stage}/include/openssl" "${output}/include/"
cp -L "${stage}/lib/libcrypto.so.3" "${output}/lib/libcrypto_3.so"
cp -L "${stage}/lib/libssl.so.3" "${output}/lib/libssl_3.so"
patchelf --set-soname libcrypto_3.so "${output}/lib/libcrypto_3.so"
patchelf --set-soname libssl_3.so "${output}/lib/libssl_3.so"
patchelf --replace-needed libcrypto.so.3 libcrypto_3.so "${output}/lib/libssl_3.so"

if command -v llvm-readelf >/dev/null 2>&1; then
    llvm-readelf -h "${output}/lib/libcrypto_3.so" | grep -E 'Class:|Machine:'
    llvm-readelf -d "${output}/lib/libssl_3.so" | grep NEEDED
fi
printf 'Android arm64-v8a OpenSSL %s installed at: %s\n' "${version}" "${output}"
printf 'CMake: -DGENYCASTER_ANDROID_OPENSSL_ROOT=%s\n' "${output}"
