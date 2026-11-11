#!/usr/bin/env bash
# A clean Ubuntu 24.04 container catches accidental dependence on a runner's
# Qt installation, linker search path, or developer packages. This is not a
# universal test of every Linux distro; Ubuntu 24.04 is the baseline contract.
set -euo pipefail

[[ $# -eq 2 ]] || { echo "Usage: $0 path/to/package.deb path/to/package.AppImage" >&2; exit 2; }
PACKAGE="$(realpath "$1")"
APPIMAGE="$(realpath "$2")"
test -s "$PACKAGE"
test -s "$APPIMAGE"
[[ "$(dirname "$PACKAGE")" == "$(dirname "$APPIMAGE")" ]] || {
  echo "AppImage and DEB must be in the same release directory" >&2
  exit 2
}
command -v docker >/dev/null || {
  echo "::error::Docker unavailable for fresh-install smoke test" >&2
  exit 1
}
DIRECTORY="$(dirname "$PACKAGE")"
FILENAME="$(basename "$PACKAGE")"
APPIMAGE_FILE="$(basename "$APPIMAGE")"

# Keep diagnostics outside dist/: they must never enter the release artifacts.
DIAGNOSTICS="$PWD/build/ci-linux-render-diagnostics"
mkdir -p "$DIAGNOSTICS"
docker run --rm \
  --mount "type=bind,src=$DIRECTORY,dst=/release,readonly" \
  --mount "type=bind,src=$DIAGNOSTICS,dst=/diagnostics" \
  --env "GENYCASTER_DEB=$FILENAME" \
  --env "GENYCASTER_APPIMAGE=$APPIMAGE_FILE" \
  ubuntu:24.04 bash -euc '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      "/release/$GENYCASTER_DEB" xvfb xauth python3-pil libgl1-mesa-dri >/dev/null
    test -x /opt/genycaster/AppRun
    # Qt must come from the installer, not from an independently installed
    # distro package. Test the full QML startup on a blank user environment.
    if dpkg -l libqt6quick6 2>/dev/null | grep -q "^ii "; then
      echo "Fresh container unexpectedly installed a separate Qt runtime" >&2
      exit 1
    fi
    export XDG_RUNTIME_DIR=/tmp/genycaster-xdg
    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 700 "$XDG_RUNTIME_DIR"
    QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
      QML_DISABLE_DISK_CACHE=1 \
      timeout 45s xvfb-run -a env \
        -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
        -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
        /opt/genycaster/AppRun --smoke-test \
        --settings-path=/tmp/genycaster-smoke-settings.ini
    echo "Clean Ubuntu 24.04 Qt/QML installer smoke: PASS"
    # Loading the QML root alone cannot detect a broken OpenSSL dlopen pair.
    # Verify that the installed app actually has a usable Qt TLS socket.
    QT_QPA_PLATFORM=xcb timeout 20s xvfb-run -a env \
      -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
      -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
      /opt/genycaster/AppRun --check-tls
    echo "Clean Ubuntu 24.04 Debian Qt TLS preflight: PASS"
    # AppImage must work independently of system Qt, too. The type-2
    # extractor avoids FUSE requirements in unprivileged containers.
    APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
      QML_DISABLE_DISK_CACHE=1 \
      timeout 45s xvfb-run -a env \
        -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
        -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
        "/release/$GENYCASTER_APPIMAGE" --smoke-test \
        --settings-path=/tmp/genycaster-appimage-smoke.ini
    echo "Clean Ubuntu 24.04 AppImage Qt/QML startup smoke: PASS"
    APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=xcb timeout 20s xvfb-run -a env \
      -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
      -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
      "/release/$GENYCASTER_APPIMAGE" --check-tls
    echo "Clean Ubuntu 24.04 AppImage Qt TLS preflight: PASS"
    # QML object creation can pass with an entirely black Qt Quick surface.
    # Render a real offscreen frame and reject uniform/blank captures.
    for target in deb appimage; do
      if [ "$target" = deb ]; then
        program=/opt/genycaster/AppRun
      else
        program="/release/$GENYCASTER_APPIMAGE"
      fi
      QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
        QML_DISABLE_DISK_CACHE=1 APPIMAGE_EXTRACT_AND_RUN=1 \
        timeout 45s xvfb-run -a env \
          -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
          -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
          "$program" --visual-test --snapshot "/diagnostics/genycaster-$target.png" \
          --snapshot-delay-ms=2800 \
          --settings-path="/tmp/genycaster-$target-visual.ini"
      python3 - "$target" <<'"'"'PY'"'"'
from PIL import Image
from pathlib import Path
import sys
path = Path("/diagnostics/genycaster-" + sys.argv[1] + ".png")
with Image.open(path) as image:
    rgb = image.convert("RGB")
    width, height = rgb.size
    if width < 320 or height < 240:
        raise SystemExit(f"Invalid rendered window size: {rgb.size}")
    samples = {rgb.getpixel((x * (width - 1) // 31,
                             y * (height - 1) // 23))
               for y in range(24) for x in range(32)}
    if len(samples) < 12:
        raise SystemExit(f"Black/blank Qt Quick frame: {sys.argv[1]} ({len(samples)} colors, RGB={next(iter(samples))})")
    print(f"Linux {sys.argv[1]} software-rendered frame: PASS ({len(samples)} sampled colors)")
PY
    done
  '
