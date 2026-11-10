#!/usr/bin/env bash
# A clean Ubuntu 24.04 container catches accidental dependence on a runner's
# Qt installation, linker search path, or developer packages. This is not a
# universal test of every Linux distro; Ubuntu 24.04 is the baseline contract.
set -euo pipefail

[[ $# -eq 1 ]] || { echo "Usage: $0 path/to/package.deb" >&2; exit 2; }
PACKAGE="$(realpath "$1")"
test -s "$PACKAGE"
command -v docker >/dev/null || {
  echo "::error::Docker unavailable for fresh-install smoke test" >&2
  exit 1
}
DIRECTORY="$(dirname "$PACKAGE")"
FILENAME="$(basename "$PACKAGE")"

docker run --rm \
  --mount "type=bind,src=$DIRECTORY,dst=/release,readonly" \
  --env "GENYCASTER_DEB=$FILENAME" \
  ubuntu:24.04 bash -euc '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      "/release/$GENYCASTER_DEB" xvfb xauth >/dev/null
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
    QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=software \
      QML_DISABLE_DISK_CACHE=1 \
      timeout 45s xvfb-run -a env \
        -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH \
        -u QML_IMPORT_PATH -u QML2_IMPORT_PATH -u LD_LIBRARY_PATH \
        /opt/genycaster/AppRun --smoke-test \
        --settings-path=/tmp/genycaster-smoke-settings.ini
    echo "Clean Ubuntu 24.04 Qt/QML installer smoke: PASS"
  '
