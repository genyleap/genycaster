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
repo_url=https://ppa.launchpadcontent.net/ubuntu-toolchain-r/test/ubuntu
key_url="https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x${fingerprint}"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

curl --fail --location --silent --show-error --retry 4 --retry-all-errors \
  --connect-timeout 15 --max-time 90 "$key_url" -o "$work/key.asc"
test -s "$work/key.asc"
mapfile -t actual < <(gpg --batch --show-keys --with-colons "$work/key.asc" |
  awk -F: '$1 == "fpr" {print toupper($10)}')
if [[ "${#actual[@]}" -ne 1 || "${actual[0]}" != "$fingerprint" ]]; then
  echo "::error::Unexpected Ubuntu toolchain PPA signing key fingerprint" >&2
  exit 1
fi
gpg --batch --yes --dearmor -o "$work/key.gpg" "$work/key.asc"
sudo install -m 0644 "$work/key.gpg" \
  /usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg
printf 'deb [signed-by=/usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg] %s noble main\n' \
  "$repo_url" | sudo tee /etc/apt/sources.list.d/genycaster-toolchain-r.list >/dev/null

sudo apt-get update -o Acquire::Retries=4
sudo apt-get install -y --no-install-recommends gcc-16 g++-16 lld
gcc-16 --version | head -1
g++-16 --version | head -1
ld.lld --version
