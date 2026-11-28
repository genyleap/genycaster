#!/usr/bin/env bash
# Run ONCE on the maintainer's trusted Mac, outside the repository.
# No unencrypted private key, network upload, or automatic trust changes.
set -euo pipefail
umask 077
destination="${1:?Provide a NEW directory outside the repository for the identity backup}"
parent="$(cd "$(dirname "$destination")" && pwd -P)"
destination="$parent/$(basename "$destination")"
repo="$(cd "$(dirname "$0")/.." && pwd -P)"
case "$destination/" in "$repo/"*) echo "Refusing to store a private identity in the repository" >&2; exit 1;; esac
mkdir -m 700 "$destination"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
cat > "$scratch/certificate.cnf" <<'CONF'
[req]
prompt = no
distinguished_name = subject
x509_extensions = code_signing
[subject]
CN = Genycaster Release Code Signing
[code_signing]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature
extendedKeyUsage = critical,codeSigning
subjectKeyIdentifier = hash
CONF
echo "Choose an encryption passphrase for the temporary private key."
openssl req -new -x509 -newkey rsa:3072 -sha256 -days 3650 \
  -config "$scratch/certificate.cnf" -keyout "$scratch/key.pem" \
  -out "$destination/Genycaster-release.pem"
echo "Export the encrypted P12. Keep its export password in your password manager."
openssl pkcs12 -export -name "Genycaster Release Code Signing" \
  -inkey "$scratch/key.pem" -in "$destination/Genycaster-release.pem" \
  -out "$destination/Genycaster-release.p12"
test -s "$destination/Genycaster-release.p12"
openssl x509 -in "$destination/Genycaster-release.pem" -outform DER \
  -out "$destination/Genycaster-release.cer"
shasum -a 1 "$destination/Genycaster-release.cer" | awk '{print toupper($1)}' \
  > "$destination/certificate-sha1.txt"
shasum -a 256 "$destination/Genycaster-release.cer" | awk '{print toupper($1)}' \
  > "$destination/certificate-sha256.txt"
echo "Public certificate SHA-1 fingerprint:"
cat "$destination/certificate-sha1.txt"
echo "Back up the encrypted P12 and password separately. Never regenerate it for a release."
echo "No certificate has been trusted or uploaded by this script."
