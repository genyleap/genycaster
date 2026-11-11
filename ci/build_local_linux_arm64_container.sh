#!/usr/bin/env bash
# Build the actual GenyCaster Linux ARM64 release on Ubuntu 25.10 (or other
# native ARM64 Linux) *inside Ubuntu 24.04*, without GitHub Actions minutes.
set -euo pipefail
cd "$(dirname "$0")/.."
repo="$PWD"

if [[ "$(uname -m)" != aarch64 ]]; then
    echo "Native aarch64 host required; got $(uname -m)" >&2
    exit 2
fi
if ! command -v docker >/dev/null 2>&1; then
    echo "Install Docker first: sudo apt update && sudo apt install -y docker.io" >&2
    exit 2
fi
if ! docker info >/dev/null 2>&1; then
    echo "Docker daemon is unavailable to this user." >&2
    echo "Start it with: sudo systemctl enable --now docker" >&2
    echo "Then run as a Docker-enabled user (see docs/LOCAL_RELEASE_REPRODUCTION.md)." >&2
    exit 2
fi
if [[ "$(git branch --show-current)" != main ]] ||
   [[ -n "$(git status --porcelain)" ]]; then
    echo "Checkout a clean, up-to-date main before building." >&2
    exit 2
fi
qt="$repo/Qt/6.12.0/gcc_arm64"
if [[ ! -x "$qt/bin/qmake6" ]] ||
   [[ ! -f "$qt/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]]; then
    echo "The official Qt 6.12.0 linux_arm64 SDK with qtmultimedia is required in Qt/." >&2
    exit 2
fi

# These directories are ignored by git. Use matching host/container absolute
# paths so the nested clean-install Docker smoke can resolve bind sources.
mkdir -p "$repo/build/arm64-container-home"
image=genycaster-local-linux-arm64-release:ubuntu24.04
docker build --platform linux/arm64 \
    --tag "$image" \
    --file ci/Dockerfile.linux-arm64-release \
    "$repo"

# This socket grants access to the host Docker daemon. Use it only for the
# trusted project's clean Ubuntu install/graphics/TLS acceptance tests.
# The numeric user and Docker socket group preserve ownership of outputs.
socket_gid="$(stat -c '%g' /var/run/docker.sock)"
docker run --rm --platform linux/arm64 \
    --mount "type=bind,source=$repo,target=$repo" \
    --mount "type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock" \
    --workdir "$repo" \
    --user "$(id -u):$(id -g)" \
    --group-add "$socket_gid" \
    --env "HOME=$repo/build/arm64-container-home" \
    --env "QT_ROOT_DIR=$qt" \
    --env "BUILD_JOBS=$(printenv BUILD_JOBS || echo 2)" \
    --env PYTHONDONTWRITEBYTECODE=1 \
    "$image" \
    bash ci/build_local_linux_release.sh

echo "Native ARM64 local release completed: dist/local-linux-arm64/"
