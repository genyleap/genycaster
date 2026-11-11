#!/usr/bin/env bash
# Install GCC 16 on Ubuntu Noble without calling Launchpad's REST API.
# ubuntu-toolchain-r/test PPA key fingerprint is published on Launchpad.
# Fail closed if a mirror/keyserver supplies an unexpected signing key.
set -euo pipefail

[[ -r /etc/os-release ]] || { echo "Cannot determine Ubuntu version" >&2; exit 1; }
. /etc/os-release
[[ "${ID:-}" == "ubuntu" && "${VERSION_CODENAME:-}" == "noble" ]] || {
  echo "GCC 16 bootstrap expects Ubuntu 24.04 Noble" >&2
  exit 1
}

fingerprint=C8EC952E2A0E1FBDC5090F6A2C277A0A352154E5
# This is a public key, not a secret. It is stored with the build sources to
# avoid reaching the network keyserver on every GitHub runner: the
# keyserver could not be resolved even when Ubuntu package mirrors worked.
key_source="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/keys/ubuntu-toolchain-r-test.asc"
test -s "$key_source" || { echo "::error::Pinned toolchain signing key missing" >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

# The vendored key is still untrusted until the primary fingerprint matches
# the independently pinned PPA signing identity. Refuse a key replacement.
mapfile -t actual < <(gpg --batch --show-keys --with-colons "$key_source" |
  awk -F: '$1 == "pub" {primary=1; next} primary && $1 == "fpr" {print toupper($10); exit}')
if [[ "${#actual[@]}" -ne 1 || "${actual[0]}" != "$fingerprint" ]]; then
  echo "::error::Unexpected Ubuntu toolchain PPA signing key fingerprint" >&2
  exit 1
fi
gpg --batch --yes --dearmor -o "$work/key.gpg" "$key_source"
sudo install -m 0644 "$work/key.gpg" \
  /usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg
# Try both Launchpad's current archive endpoint and its legacy official
# endpoint. Both must serve the exact same PPA-signed InRelease metadata:
# APT validates signatures with our fingerprint-checked public key.
# An unhealthy Canonical DNS/network must not strand an ARM64 runner for
# minutes, and it must never fall back to an unsigned compiler.
candidates=(
  https://ppa.launchpadcontent.net/ubuntu-toolchain-r/test/ubuntu
  https://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu
)
selected=""
for repo_url in "${candidates[@]}"; do
  echo "::group::Checking signed GCC16 PPA archive at $repo_url"
  if ! curl --fail --location --silent --show-error \
      --connect-timeout 4 --max-time 10 \
      "$repo_url/dists/noble/InRelease" -o /dev/null; then
    echo "::warning::GCC16 PPA endpoint unavailable: $repo_url"
    echo "::endgroup::"
    continue
  fi
  printf 'deb [signed-by=/usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg] %s noble main\n' \
    "$repo_url" | sudo tee /etc/apt/sources.list.d/genycaster-toolchain-r.list >/dev/null

  # First mirror indices were refreshed in prepare_ubuntu_apt.sh.
  # Fetch only this PPA index while keeping Ubuntu's normal signed cache.
  if sudo timeout 70s apt-get update \
      -o Dir::Etc::sourcelist=/etc/apt/sources.list.d/genycaster-toolchain-r.list \
      -o Dir::Etc::sourceparts=- \
      -o APT::Get::List-Cleanup=0 \
      -o APT::Update::Error-Mode=any \
      -o Acquire::Retries=0 \
      -o Acquire::https::Timeout=12; then
    selected="$repo_url"
    echo "::endgroup::"
    break
  fi
  echo "::warning::GCC16 PPA signature/index update failed: $repo_url"
  echo "::endgroup::"
done
if [[ -z "$selected" ]]; then
  echo "::error::No reachable, correctly signed GCC16 PPA archive. Refusing to build with the wrong C++26 toolchain." >&2
  exit 1
fi
sudo timeout 180s apt-get install -y --no-install-recommends \
  -o Acquire::Retries=1 \
  -o Acquire::https::Timeout=12 \
  gcc-16 g++-16 lld
gcc-16 --version | head -1
g++-16 --version | head -1
ld.lld --version
