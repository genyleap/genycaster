#!/usr/bin/env bash
# Resolve an Ubuntu mirror before invoking apt, and never wait through
# dozens of 20-second DNS retries on a broken hosted runner.
# All mirrors serve Ubuntu-signed Release files; apt verifies signatures
# against the runner's existing ubuntu-archive-keyring.gpg.
set -euo pipefail

. /etc/os-release
[[ "${ID:-}" == "ubuntu" && "${VERSION_CODENAME:-}" == "noble" ]] || {
  echo "::error::This release builder requires Ubuntu 24.04 (noble)" >&2
  exit 1
}
test -r /usr/share/keyrings/ubuntu-archive-keyring.gpg || {
  echo "::error::Ubuntu archive signing key unavailable" >&2
  exit 1
}
test -f /etc/apt/sources.list.d/ubuntu.sources || {
  echo "::error::Expected Ubuntu 24.04 deb822 sources missing" >&2
  exit 1
}

case "$(dpkg --print-architecture)" in
  arm64)
    # These are all official Ubuntu Ports mirrors with noble arm64 indices.
    mirrors=(
      https://us.ports.ubuntu.com/ubuntu-ports
      https://ports.ubuntu.com/ubuntu-ports
      https://ftp.ports.ubuntu.com/ubuntu-ports
    )
    ;;
  amd64)
    mirrors=(
      https://archive.ubuntu.com/ubuntu
      https://us.archive.ubuntu.com/ubuntu
      https://security.ubuntu.com/ubuntu
    )
    ;;
  *)
    echo "::error::Unsupported Ubuntu package architecture" >&2
    exit 1
    ;;
esac

for uri in "${mirrors[@]}"; do
  echo "::group::Checking Ubuntu signed archive mirror: $uri"
  # Probe an actual signed index, not DNS alone. TCP/DNS failures are
  # bounded to 8s so the CI job can be retried on a healthy runner.
  if ! curl --fail --location --silent --show-error \
      --connect-timeout 4 --max-time 8 \
      "$uri/dists/noble/InRelease" -o /dev/null; then
    echo "::warning::Ubuntu mirror unreachable: $uri"
    echo "::endgroup::"
    continue
  fi
  cat > /tmp/genycaster-ubuntu.sources <<APT_SOURCES
Types: deb
URIs: $uri
Suites: noble noble-updates noble-backports noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
APT_SOURCES
  sudo install -m 0644 /tmp/genycaster-ubuntu.sources \
    /etc/apt/sources.list.d/ubuntu.sources
  # Don't silently use stale indices after an APT warning. Fail or try
  # the next official mirror if even one index fetch is unavailable.
  if sudo timeout 100s apt-get update \
       -o APT::Update::Error-Mode=any \
       -o Acquire::Retries=0 \
       -o Acquire::http::Timeout=12 \
       -o Acquire::https::Timeout=12; then
    echo "Ubuntu package mirror ready: $uri"
    echo "::endgroup::"
    exit 0
  fi
  echo "::warning::APT update failed for $uri"
  echo "::endgroup::"
done

echo "::error::No reachable, signed Ubuntu package mirror. Runner DNS/network is unhealthy; stop instead of burning CI minutes." >&2
exit 1
