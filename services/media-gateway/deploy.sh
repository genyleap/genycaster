#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [[ ! -f .env ]]; then
  echo "Missing services/media-gateway/.env (copy .env.example and fill server-only values)." >&2
  exit 1
fi

required=(
  GENYLEAP_MEDIA_TICKET_SECRET
  GENYLEAP_MEDIA_STORAGE_BUCKET
  GENYLEAP_MEDIA_STORAGE_ACCESS_KEY_ID
  GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY
)

for name in "${required[@]}"; do
  value="$(grep -E "^${name}=" .env | tail -n1 | cut -d= -f2- || true)"
  if [[ -z "$value" ]]; then
    echo "Missing required .env value: ${name}" >&2
    exit 1
  fi
done

node --check storage/quilibrium_sdk.mjs
node --check gateway.mjs
node --check server.mjs
node --test test/gateway.test.mjs test/quilibrium_sdk_adapter.test.mjs

docker compose build media-gateway
docker compose up -d --force-recreate media-gateway

docker compose exec -T media-gateway node --input-type=module - <<'NODE'
const response = await fetch('http://127.0.0.1:8790/healthz');
if (!response.ok) {
  throw new Error(`media gateway health check failed with HTTP ${response.status}`);
}
console.log(await response.text());
NODE

echo "Genycaster media gateway deployment completed successfully."
