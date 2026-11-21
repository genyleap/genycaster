#!/usr/bin/env bash
# Run on the host as root (not inside the read-only Remote Operations sandbox).
# Provision only status.genycaster.xyz; preserve all existing virtual hosts.
set -Eeuo pipefail

DOMAIN="status.genycaster.xyz"
BASE="$(cd "$(dirname "$0")" && pwd)"
SITE="/etc/nginx/sites-available/$DOMAIN"
ENABLED="/etc/nginx/sites-enabled/$DOMAIN"
ACME_ROOT="/var/www/letsencrypt"
CERT_DIR="/etc/letsencrypt/live/$DOMAIN"
RESPONSE_FILE="$(mktemp)"
CREATED=0

cleanup() {
  rc=$?
  rm -f "$RESPONSE_FILE"
  if [[ "$rc" -ne 0 && "$CREATED" -eq 1 ]]; then
    printf 'Rolling back newly created Nginx host after failure.\n' >&2
    rm -f "$ENABLED" "$SITE"
    nginx -t && systemctl reload nginx || true
  fi
}
trap cleanup EXIT

if [[ "$EUID" -ne 0 ]]; then
  echo "Run as root: sudo env CERTBOT_EMAIL=you@example.com bash $0" >&2
  exit 2
fi
if [[ -e "$SITE" || -L "$ENABLED" ]]; then
  echo "Refusing to replace an existing Nginx host: $SITE" >&2
  exit 2
fi
if [[ ! -f "$BASE/status.genycaster.xyz.conf" || ! -f "$BASE/status.genycaster.xyz-acme.conf" ]]; then
  echo "Nginx template files are missing" >&2
  exit 2
fi
if [[ ! -s "$CERT_DIR/fullchain.pem" || ! -s "$CERT_DIR/privkey.pem" ]]; then
  if [[ ! -v CERTBOT_EMAIL ]] || [[ -z "$CERTBOT_EMAIL" ]]; then
    echo "CERTBOT_EMAIL is required to request a dedicated TLS certificate" >&2
    exit 2
  fi
  if ! command -v certbot >/dev/null; then
    echo "Certbot must be installed to issue the dedicated TLS certificate" >&2
    exit 2
  fi
fi

# This worker already runs independently via PM2.
WORKER_STATUS="$(curl --silent --show-error --max-time 8 --output "$RESPONSE_FILE" \
  --write-out '%{http_code}' http://127.0.0.1:8798/api/health)"
if [[ "$WORKER_STATUS" != 200 && "$WORKER_STATUS" != 503 ]]; then
  echo "Health worker is unavailable on loopback" >&2
  exit 1
fi
nginx -t
mkdir -p "$ACME_ROOT"
install -m 0644 "$BASE/status.genycaster.xyz-acme.conf" "$SITE"
ln -s "$SITE" "$ENABLED"
CREATED=1
nginx -t
systemctl reload nginx

# DNS already resolves through Cloudflare to this origin. HTTP-01 can validate
# through the temporary port-80 ACME location without disabling the proxy.
if [[ ! -s "$CERT_DIR/fullchain.pem" || ! -s "$CERT_DIR/privkey.pem" ]]; then
  certbot certonly --webroot --webroot-path "$ACME_ROOT" \
    --cert-name "$DOMAIN" --domains "$DOMAIN" \
    --non-interactive --agree-tos --email "$CERTBOT_EMAIL" \
    --keep-until-expiring
fi

install -m 0644 "$BASE/status.genycaster.xyz.conf" "$SITE"
nginx -t
systemctl reload nginx

# Check actual TLS name validation at the origin, not just Cloudflare's edge.
HEALTH_STATUS="$(curl --silent --show-error --max-time 10 \
  --resolve "$DOMAIN:443:127.0.0.1" \
  --output "$RESPONSE_FILE" --write-out '%{http_code}' \
  "https://$DOMAIN/api/health")"
if [[ "$HEALTH_STATUS" != 200 && "$HEALTH_STATUS" != 503 ]]; then
  echo "Origin health endpoint returned unexpected HTTP $HEALTH_STATUS" >&2
  exit 1
fi
python3 -c 'import json,sys
o=json.load(open(sys.argv[1]))
assert o.get("service") == "genycaster"
assert o.get("status") in ("operational","degraded","unavailable")
assert set(o.get("checks",{})) == {"web","backend","snapchain","hypersnap"}
' "$RESPONSE_FILE"

echo "Origin: https://$DOMAIN/api/health (HTTP $HEALTH_STATUS)"
echo "Public: https://$DOMAIN/api/health"
echo "The existing https://genycaster.xyz/api/v1/cloud/health remains available."
