#!/usr/bin/env bash
# Fallback for hosted Ubuntu x86_64/ARM64 runners when Canonical's signed
# GCC16 PPA is unavailable. Keep C++26 and the actual native ABI, without
# disabling Mini Apps, skipping signature checks or substituting an old GCC.
# The compiler packages are pinned and a native Ubuntu sysroot is verified.
set -euo pipefail

case "$(dpkg --print-architecture)" in
  arm64)
    conda_arch=aarch64
    mamba_asset=micromamba-linux-aarch64
    micromamba_sha=9f93b974adcb4d166996af969b6cd371287d1a3e52733704727884d9b74cb7a7
    ;;
  amd64)
    conda_arch=64
    mamba_asset=micromamba-linux-64
    # GitHub release 2.9.0-0 asset SHA-256 (not ARM64's binary digest).
    micromamba_sha=366cd9cd8be14df1ab8ed50352a82111082a36686b2d389fdb79a92c3fafb3e3
    ;;
  *)
    echo "::error::Unsupported native architecture for pinned GCC16 fallback" >&2
    exit 1
    ;;
esac
: "${RUNNER_TEMP:?Only the GitHub-hosted fallback supports this bootstrap}"
version=2.9.0-0
work="$RUNNER_TEMP/genycaster-toolchain-fallback"
prefix="$work/gcc16"
mkdir -p "$work"
curl --fail --location --silent --show-error --retry 2 \
  --connect-timeout 12 --max-time 150 \
  "https://github.com/mamba-org/micromamba-releases/releases/download/${version}/${mamba_asset}" \
  -o "$work/micromamba"
echo "$micromamba_sha  $work/micromamba" | sha256sum --check --status
chmod +x "$work/micromamba"

export MAMBA_ROOT_PREFIX="$work/mamba-root"
# Keep native arch-specific conda-forge packages at GCC 16.2.0, never pull
# an unpinned replacement from another channel or a newer distro sysroot.
# Conda uses linux-64 for x86_64, linux-aarch64 for ARM64.
timeout 480s "$work/micromamba" create -y -p "$prefix" \
  --override-channels -c conda-forge \
  "binutils_linux-${conda_arch}" \
  "gcc_linux-${conda_arch}=16.2.0" "gxx_linux-${conda_arch}=16.2.0"

target_triplet=aarch64-conda-linux-gnu
if [[ "$conda_arch" == 64 ]]; then
  target_triplet=x86_64-conda-linux-gnu
fi
compiler="$prefix/bin/${target_triplet}-gcc"
cxx="$prefix/bin/${target_triplet}-g++"
test -x "$compiler"
test -x "$cxx"

# The native release link uses -fuse-ld=lld. It is an Ubuntu package,
# available through the signed mirror even when Launchpad is blocked.
if ! command -v ld.lld >/dev/null; then
  sudo timeout 90s apt-get install -y --no-install-recommends \
    -o Acquire::Retries=1 -o Acquire::https::Timeout=12 lld
fi
ld.lld --version

# Use matching native binutils for the pinned compiler on BOTH architectures.
# GCC 16.2 emits new AArch64 .aeabi_* directives that Noble's old as cannot
# parse; x86_64 must likewise not accidentally select mismatched host tools.
# GCC/collect2 honors COMPILER_PATH when locating the assembler and linker.
test -x "$prefix/bin/${target_triplet}-ld"
assembler="$prefix/bin/${target_triplet}-as"
test -x "$assembler"
"$assembler" --version | head -1
shim="$work/compiler-binutils"
mkdir -p "$shim"
ln -s "$assembler" "$shim/as"
ln -s "$prefix/bin/${target_triplet}-ld" "$shim/ld"
# Keep the requested -fuse-ld=lld explicit and ahead of GNU ld.
ln -s "$(command -v ld.lld)" "$shim/ld.lld"
# GCC/collect2 search COMPILER_PATH for as and ld. Include binutils and
# host's lld but do not allow the old assembler to win the search order.
export COMPILER_PATH="$shim:$prefix/bin:/usr/bin"
echo "COMPILER_PATH=$COMPILER_PATH" >> "${GITHUB_ENV:?}"

# Existing CMake / release scripts select gcc-16 and g++-16 by name.
# Resolve these only to the verified native GCC16 compiler, never an older
# system toolchain. Do not modify application features or build flags.
for spec in "gcc-16:$compiler" "g++-16:$cxx"; do
  name="${spec%%:*}"
  source="${spec#*:}"
  sudo ln -sfn "$source" "/usr/local/bin/$name"
done
# GCC locates its own binutils and runtime through the configured prefix.
# Put that prefix first for later workflow steps and verify actual binaries.
echo "$prefix/bin" >> "${GITHUB_PATH:?}"
export PATH="/usr/local/bin:$prefix/bin:$PATH"
gcc-16 --version | head -1
g++-16 --version | head -1
[[ "$(gcc-16 -dumpfullversion)" == 16.2.0* ]] || {
  echo "::error::Conda fallback did not provision GCC 16.2.0" >&2
  exit 1
}
[[ "$(g++-16 -dumpfullversion)" == 16.2.0* ]] || {
  echo "::error::Conda fallback did not provision G++ 16.2.0" >&2
  exit 1
}
# The conda-forge compiler defaults to its bundled glibc/sysroot while
# the official Qt SDK and Ubuntu development packages target THIS native
# Ubuntu 24.04 machine. Repeated missing OpenGL/Boost/SQLite
# headers were symptoms of mixing two sysroots, not missing apt packages.
# Use the Ubuntu sysroot consistently for compilation AND linking.
# This Linux CI release intentionally targets glibc <= 2.39 (Ubuntu 24.04);
# do not use these flags for other architectures or cross-compilation.
for flag_name in CFLAGS CXXFLAGS; do
  old_flags="${!flag_name:-}"
  flag_value="${old_flags:+$old_flags }--sysroot=/"
  export "$flag_name=$flag_value"
  printf '%s=%s\n' "$flag_name" "$flag_value" >> "${GITHUB_ENV:?}"
done

# Fail BEFORE the costly Qt download if GCC still picks conda's older glibc.
native_glibc="$(getconf GNU_LIBC_VERSION)"
compiler_glibc="$(printf '#include <features.h>\n__GLIBC__ __GLIBC_MINOR__\n' |
  g++-16 --sysroot=/ -std=c++26 -E -P -x c++ - | tail -1)"
expected_glibc="${native_glibc#glibc }"
actual_glibc="${compiler_glibc// /.}"
if [[ "$actual_glibc" != "$expected_glibc" ]]; then
  echo "::error::GCC16 sysroot glibc $actual_glibc differs from Ubuntu $expected_glibc" >&2
  exit 1
fi

# Compile the external development headers required by the full release.
# A successful GCC version/module smoke does NOT prove these are reachable.
cat > "$work/native-headers-smoke.cpp" <<'CPP'
#include <sqlite3.h>
#include <boost/json.hpp>
#include <openssl/ssl.h>
#include <curl/curl.h>
#include <GL/gl.h>
#include <EGL/egl.h>
static_assert(SQLITE_VERSION_NUMBER >= 3000000);
CPP
g++-16 --sysroot=/ -std=c++26 -fsyntax-only "$work/native-headers-smoke.cpp"

# Check native library search as well, not only header preprocessing.
cat > "$work/link-smoke.cpp" <<'CPP'
#include <sqlite3.h>
int main() {
    if (sqlite3_initialize() != SQLITE_OK) return 1;
    return sqlite3_shutdown() == SQLITE_OK ? 0 : 2;
}
CPP
g++-16 --sysroot=/ -std=c++26 -fuse-ld=lld \
  "$work/link-smoke.cpp" -lsqlite3 -o "$work/link-smoke"
# Smoke-check an actual C++26 Qt-style module so the toolchain cannot
# pass a basic source file but later fail on C++ module assembly.
cat > "$work/module-smoke.cppm" <<'CXX_MODULE'
export module genycaster.toolchain.smoke;
export int answer() { return 42; }
CXX_MODULE
g++-16 --sysroot=/ -std=c++26 -fmodules-ts -x c++ -c "$work/module-smoke.cppm" \
  -o "$work/module-smoke.o"
test -x "$work/link-smoke"
"$work/link-smoke"
echo "::notice::Native Ubuntu sysroot glibc $actual_glibc, GCC16, Linux headers, SQLite link and C++26 module smoke passed"
