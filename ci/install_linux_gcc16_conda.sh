#!/usr/bin/env bash
# Fallback for hosted ARM64 runners that cannot reach Canonical PPA IPs.
# Keep GCC 16 and C++26; never replace it with an older GCC or disable
# native Mini Apps. Conda-forge's native ARM64 compiler targets a portable
# Linux sysroot and is downloaded only after the signed Ubuntu PPA fails.
set -euo pipefail

[[ "$(dpkg --print-architecture)" == "arm64" ]] || {
  echo "::error::Conda GCC16 fallback is currently restricted to ARM64" >&2
  exit 1
}
: "${RUNNER_TEMP:?Only the GitHub-hosted fallback supports this bootstrap}"

version=2.9.0-0
micromamba_sha=9f93b974adcb4d166996af969b6cd371287d1a3e52733704727884d9b74cb7a7
work="$RUNNER_TEMP/genycaster-toolchain-fallback"
prefix="$work/gcc16"
mkdir -p "$work"
curl --fail --location --silent --show-error --retry 2 \
  --connect-timeout 12 --max-time 150 \
  "https://github.com/mamba-org/micromamba-releases/releases/download/${version}/micromamba-linux-aarch64" \
  -o "$work/micromamba"
echo "$micromamba_sha  $work/micromamba" | sha256sum --check --status
chmod +x "$work/micromamba"

export MAMBA_ROOT_PREFIX="$work/mamba-root"
# Pin the compiler's major/minor series and use native ARM64 binaries rather
# than GCC from the Ubuntu 26.04 archive (which would raise the glibc floor).
timeout 480s "$work/micromamba" create -y -p "$prefix" \
  --override-channels -c conda-forge \
  'binutils_linux-aarch64' 'gcc_linux-aarch64=16.2.0' 'gxx_linux-aarch64=16.2.0'

compiler="$prefix/bin/aarch64-conda-linux-gnu-gcc"
cxx="$prefix/bin/aarch64-conda-linux-gnu-g++"
test -x "$compiler"
test -x "$cxx"

# The native release link uses -fuse-ld=lld. It is an Ubuntu package,
# available through the signed mirror even when Launchpad is blocked.
if ! command -v ld.lld >/dev/null; then
  sudo timeout 90s apt-get install -y --no-install-recommends \
    -o Acquire::Retries=1 -o Acquire::https::Timeout=12 lld
fi
ld.lld --version

# This GCC distribution expects its target binutils under a prefix. On
# GitHub's Ubuntu ARM runners, GNU collect2 otherwise reports "cannot find
# 'ld'" despite the system providing /usr/bin/ld.lld.
# GCC's COMPILER_PATH is honored by collect2 for external executables.
test -x "$prefix/bin/aarch64-conda-linux-gnu-ld"
# GCC 16.2 emits new AArch64 .aeabi_* directives which Ubuntu Noble's
# stock GNU as does not understand. Use the matching conda-forge
# binutils 2.46 assembler, never the system /usr/bin/as.
assembler="$prefix/bin/aarch64-conda-linux-gnu-as"
test -x "$assembler"
"$assembler" --version | head -1
shim="$work/compiler-binutils"
mkdir -p "$shim"
ln -s "$assembler" "$shim/as"
ln -s "$prefix/bin/aarch64-conda-linux-gnu-ld" "$shim/ld"
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
# Check the actual release linker path before downloading/building Qt.
# A compiler version check alone does not prove collect2 can find LLD.
printf 'int main() { return 0; }\n' > "$work/link-smoke.cpp"
g++-16 -std=c++26 -fuse-ld=lld "$work/link-smoke.cpp" -o "$work/link-smoke"
# Smoke-check an actual C++26 Qt-style module so the toolchain cannot
# pass a basic source file but later fail on C++ module assembly.
cat > "$work/module-smoke.cppm" <<'CXX_MODULE'
export module genycaster.toolchain.smoke;
export int answer() { return 42; }
CXX_MODULE
g++-16 -std=c++26 -fmodules-ts -x c++ -c "$work/module-smoke.cppm" \
  -o "$work/module-smoke.o"
test -x "$work/link-smoke"
"$work/link-smoke"
echo "::notice::Native GCC16 ARM64 and LLD link smoke test passed"
