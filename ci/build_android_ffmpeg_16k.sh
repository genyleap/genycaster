#!/usr/bin/env bash
# Replace Qt 6.12's vendor-built Android FFmpeg shared objects with a
# deterministic FFmpeg 9.0.1 build linked for 16 KiB Android pages.
# Never patch ELF PT_LOAD headers or weaken the final APK alignment gate.
# This script runs only on the isolated GitHub Actions runner, not on devices.
set -euo pipefail

: "${QT_ROOT_DIR:?Qt 6.12 Android arm64 SDK is required}"
: "${ANDROID_NDK_ROOT:?Android NDK r27 is required}"
: "${RUNNER_TEMP:?GitHub Actions temporary directory is required}"
: "${ANDROID_API:?Android API target is required}"

[[ "$ANDROID_API" == "28" ]] || { echo "::error::FFmpeg Android target API differs"; exit 1; }
[[ -f "$QT_ROOT_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
  echo "::error::No official Qt Android SDK"; exit 1;
}
toolchain="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin"
cc="$toolchain/aarch64-linux-android${ANDROID_API}-clang"
cxx="$toolchain/aarch64-linux-android${ANDROID_API}-clang++"
readelf="$toolchain/llvm-readelf"
[[ -x "$cc" && -x "$cxx" && -x "$readelf" ]] || {
  echo "::error::Missing Android arm64 NDK compiler/ELF audit tools"; exit 1;
}
for tool in curl tar sha256sum make python3; do command -v "$tool" >/dev/null; done

# Qt 6.12.0's FFmpeg media backend is linked against FFmpeg 9.0.1.
# Pin the source tarball to its independently published SHA-256; do not
# download or pick whichever FFmpeg happens to be the latest this week.
version=9.0.1
expected=cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635
work="$RUNNER_TEMP/genycaster-ffmpeg-16k"
[[ ! -e "$work" ]] || { echo "::error::FFmpeg staging directory already exists"; exit 1; }
mkdir -p "$work"
archive="$work/ffmpeg-$version.tar.xz"
curl --proto '=https' --tlsv1.2 --fail --location --retry 3 --silent --show-error \
  "https://ffmpeg.org/releases/ffmpeg-$version.tar.xz" -o "$archive"
printf '%s  %s\n' "$expected" "$archive" | sha256sum -c --status || {
  echo "::error::Untrusted FFmpeg source digest"; exit 1;
}
tar -xJf "$archive" -C "$work"

# Link FFmpeg HTTPS support to the same OpenSSL 3.5.9 already verified
# for the Genycaster Android APK, without introducing a second TLS ABI.
openssl_include="$GITHUB_WORKSPACE/build/android-deps/include"
openssl_libs="$GITHUB_WORKSPACE/build/android-deps/lib"
[[ -f "$openssl_include/openssl/opensslv.h" &&
   -s "$openssl_libs/libssl_3.so" &&
   -s "$openssl_libs/libcrypto_3.so" ]] || {
  echo "::error::Android OpenSSL dependency not staged"; exit 1;
}
mkdir -p "$work/openssl-link"
ln -s "$openssl_libs/libssl_3.so" "$work/openssl-link/libssl.so"
ln -s "$openssl_libs/libcrypto_3.so" "$work/openssl-link/libcrypto.so"

(
  cd "$work/ffmpeg-$version"
  ./configure \
    --prefix="$work/install" \
    --libdir="$work/install/lib" \
    --disable-doc --disable-programs --disable-debug \
    --enable-shared --disable-static --enable-pic \
    --enable-network --enable-pthreads --enable-cross-compile \
    --target-os=android --arch=aarch64 --cpu=armv8-a \
    --enable-jni --enable-mediacodec --enable-neon \
    --enable-openssl \
    --cc="$cc" --cxx="$cxx" \
    --strip="$toolchain/llvm-strip" \
    --sysroot="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot" \
    --extra-cflags="-I$openssl_include" \
    --extra-ldflags="-L$work/openssl-link -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"
  make -j 4
  make install
)

# Verify the replacement is ABI-compatible with the SDK BEFORE changing it.
# In particular, never install Android libraries whose SONAME differs from
# the Qt Multimedia FFmpeg media plugin's originally selected ABI.
libs=(libavutil libavcodec libavformat libavfilter libavdevice libswscale libswresample)
count=0
for base in "${libs[@]}"; do
  mapfile -t original < <(find "$QT_ROOT_DIR" \( -type f -o -type l \) -name "$base.so" -print)
  [[ ${#original[@]} -le 1 ]] || {
    echo "::error::Ambiguous Qt-provided $base libraries"; exit 1;
  }
  [[ ${#original[@]} == 1 ]] || continue
  candidate="$work/install/lib/$base.so"
  [[ -s "$candidate" ]] || { echo "::error::FFmpeg replacement missing $base"; exit 1; }
  original_real="$(realpath -e "${original[0]}")"
  [[ "$original_real" == "$QT_ROOT_DIR/"* ]] || {
    echo "::error::Qt FFmpeg symlink points outside SDK"; exit 1;
  }
  old_soname="$("$readelf" -d "$original_real" | sed -n 's/^.*SONAME.*\[\([^]]*\)\].*$/\1/p')"
  new_soname="$("$readelf" -d "$candidate" | sed -n 's/^.*SONAME.*\[\([^]]*\)\].*$/\1/p')"
  if [[ -z "$old_soname" || "$old_soname" != "$new_soname" ]]; then
    echo "::error::Qt FFmpeg SONAME incompatibility in $base: old=$old_soname new=$new_soname"
    exit 1
  fi
  # Check the actual rebuilt ELF PT_LOAD geometry BEFORE replacing any Qt
  # binary. A 16 KiB max-page-size link flag alone is not sufficient.
  python3 - "$candidate" "$base" <<'PY'
import sys
from pathlib import Path
from ci.verify_platform_package import validate_android_elf_alignment

lib = Path(sys.argv[1])
validate_android_elf_alignment(lib.name, lib.read_bytes())
PY
  ((count+=1))
done
[[ "$count" -ge 3 ]] || { echo "::error::Qt SDK has no recognized FFmpeg ABI"; exit 1; }

# Replace each verified shared library only after all checks pass.
# Its final Android APK is independently inspected, not trusted by name.
for base in "${libs[@]}"; do
  mapfile -t original < <(find "$QT_ROOT_DIR" \( -type f -o -type l \) -name "$base.so" -print)
  if [[ ${#original[@]} == 1 ]]; then
    original_real="$(realpath -e "${original[0]}")"
    install -m 0755 "$work/install/lib/$base.so" "$original_real"
  fi
done
echo "Rebuilt ${count} Qt FFmpeg shared libraries with 16 KiB linker alignment"
