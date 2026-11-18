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
# Some GitHub ARM runners cannot resolve Canonical-owned domains even
# though GitHub, Qt and independent Ubuntu mirrors are reachable. Query two
# HTTPS DNS-over-HTTPS services only after the normal TLS probe fails.
# /etc/hosts changes are ephemeral to the CI VM; curl still validates the
# hostname's TLS certificate and APT still verifies signed Release files.
resolve_ppa_hostname() {
  local hostname="$1" response="" address="" resolver=""
  for resolver in \
      "https://dns.google/resolve?name=${hostname}&type=A" \
      "https://cloudflare-dns.com/dns-query?name=${hostname}&type=A"; do
    response="$(curl --fail --location --silent --show-error \
      --connect-timeout 4 --max-time 10 \
      -H 'Accept: application/dns-json' "$resolver" || true)"
    [[ -n "$response" ]] || continue
    address="$(printf '%s' "$response" | python3 -c '
import ipaddress
import json
import sys
try:
    record = json.load(sys.stdin)
    if record.get("Status") == 0:
        for item in record.get("Answer", []):
            if item.get("type") != 1:
                continue
            addr = ipaddress.IPv4Address(item["data"])
            if addr.is_global:
                print(addr)
                break
except (KeyError, ValueError, TypeError):
    pass
' || true)"
    if [[ -n "$address" ]]; then
      printf '%s %s\n' "$address" "$hostname" | sudo tee -a /etc/hosts >/dev/null
      echo "::notice::Resolved PPA hostname through verified HTTPS DNS: $hostname"
      return 0
    fi
  done
  return 1
}

probe_ppa() {
  curl --fail --location --silent --show-error \
    --connect-timeout 4 --max-time 10 \
    "$1/dists/noble/InRelease" -o /dev/null
}

selected=""
for repo_url in "${candidates[@]}"; do
  echo "::group::Checking signed GCC16 PPA archive at $repo_url"
  if ! probe_ppa "$repo_url"; then
    ppa_host="${repo_url#https://}"
    ppa_host="${ppa_host%%/*}"
    if ! resolve_ppa_hostname "$ppa_host" || ! probe_ppa "$repo_url"; then
      echo "::warning::GCC16 PPA endpoint unavailable: $repo_url"
      echo "::endgroup::"
      continue
    fi
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
      -o Acquire::Retries=1 \
      -o Acquire::https::Timeout=12; then
    selected="$repo_url"
    echo "::endgroup::"
    break
  fi
  echo "::warning::GCC16 PPA signature/index update failed: $repo_url"
  echo "::endgroup::"
done
if [[ -z "$selected" ]]; then
  # The signing key and Ubuntu mirror are still mandatory. For transport
  # failures (HTTP 503 / bad hostname TLS) use a separately SHA256-pinned
  # conda-forge GCC16 on either native release runner. Never relax HTTPS or
  # accept unsigned PPA indices to paper over Canonical's outage.
  case "$(dpkg --print-architecture)" in
    amd64|arm64)
      if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
        echo "::warning::Canonical's GCC16 PPA is unreachable; verifying pinned native GCC16 from conda-forge"
        bash "$(dirname "${BASH_SOURCE[0]}")/install_linux_gcc16_conda.sh"
        exit 0
      fi
      ;;
  esac
  echo "::error::No reachable, correctly signed GCC16 PPA archive or verified GitHub runner fallback. Refusing the wrong C++26 toolchain." >&2
  exit 1
fi
sudo timeout 180s apt-get install -y --no-install-recommends \
  -o Acquire::Retries=1 \
  -o Acquire::https::Timeout=12 \
  gcc-16 g++-16 lld
gcc-16 --version | head -1
g++-16 --version | head -1
ld.lld --version
