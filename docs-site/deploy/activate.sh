#!/bin/sh
# Administrator-only site activation. The Remote Operations connector cannot
# modify /etc, so run this locally with appropriate permissions after review.
set -eu
mode="${1:-}"
case "$mode" in
  bootstrap|tls) ;;
  *) echo "Usage: sudo sh docs-site/deploy/activate.sh bootstrap|tls" >&2; exit 2 ;;
esac
[ "$(id -u)" -eq 0 ] || { echo "Root required" >&2; exit 1; }
ROOT=/opt/genycaster/repository/docs-site/deploy
AVAILABLE=/etc/nginx/sites-available/docs.genycaster.xyz
ENABLED=/etc/nginx/sites-enabled/docs.genycaster.xyz
DOCUMENTS=/var/www/docs.genycaster.xyz
[ -f "$DOCUMENTS/current/index.html" ] || { echo "Verified docs release not staged" >&2; exit 1; }
if [ "$mode" = "bootstrap" ]; then
  mkdir -p "$DOCUMENTS/acme/.well-known/acme-challenge"
  FILE="$ROOT/nginx-http-bootstrap.conf"
else
  [ -r /etc/letsencrypt/live/docs.genycaster.xyz/fullchain.pem ] || {
    echo "Dedicated docs.genycaster.xyz certificate not found" >&2; exit 1;
  }
  FILE="$ROOT/nginx-tls.conf"
fi
if [ -e "$ENABLED" ] && [ ! -L "$ENABLED" ]; then
  echo "Unexpected enabled Nginx site file; refusing overwrite" >&2; exit 1
fi
if [ -L "$ENABLED" ] && [ "$(readlink "$ENABLED")" != "$AVAILABLE" ]; then
  echo "Enabled symlink points elsewhere; refusing overwrite" >&2; exit 1
fi
if [ -f "$AVAILABLE" ]; then
  if ! cmp -s "$FILE" "$AVAILABLE"; then
    if [ "$mode" = "tls" ] && cmp -s "$ROOT/nginx-http-bootstrap.conf" "$AVAILABLE"; then
      cp "$FILE" "$AVAILABLE"
    else
      echo "Unknown existing vhost; refusing overwrite" >&2
      exit 1
    fi
  fi
else
  cp "$FILE" "$AVAILABLE"
fi
if [ ! -L "$ENABLED" ]; then ln -s "$AVAILABLE" "$ENABLED"; fi
nginx -t
systemctl reload nginx
echo "Activated docs Nginx mode: $mode"
