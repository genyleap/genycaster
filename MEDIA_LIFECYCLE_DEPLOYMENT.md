# Genycaster Managed Media Lifecycle Deployment

Date: 2026-08-31

This revision adds authenticated deletion of Genycaster-managed QStorage objects after a confirmed Farcaster Cast removal.

## Server deployment

Preserve the production `.env`; it contains QStorage credentials and the durable ticket/management-token HMAC secret.

From the Mac/project checkout:

```bash
rsync -av --delete \
  --exclude='.env' \
  --exclude='build/' \
  --exclude='node_modules/' \
  services/media-gateway/ \
  root@173.214.160.36:/opt/genycaster/media-gateway/
```

On the VPS, build before replacing the running container:

```bash
cd /opt/genycaster/media-gateway

df -h /
grep '^GENYLEAP_MEDIA_STORAGE_BUCKET=' .env
grep '^GENYLEAP_MEDIA_TICKET_SECRET=' .env | sed 's/=.*/=<configured>/'

docker compose build
docker compose up -d --force-recreate

docker compose exec -T media-gateway grep -n "deleteAssetRoute" /app/gateway.mjs
curl -s http://127.0.0.1:8790/healthz
docker compose logs --tail=50 media-gateway
```

Do not rotate `GENYLEAP_MEDIA_TICKET_SECRET` during this deployment. Existing managed-media tokens are HMAC-protected by that durable secret.

## Nginx requirement

The multipart relay uses 8 MiB parts by default, so the active server block for `storage-farcaster.genyleap.com` / `media-farcaster.genyleap.com` must retain:

```nginx
client_max_body_size 9m;
proxy_request_buffering off;
proxy_buffering off;
```

Validate/reload with:

```bash
nginx -t && systemctl reload nginx
```

## Live QStorage lifecycle acceptance

The included `test/live_qstorage_multipart_smoke.mjs` verifies:

```text
CreateMultipartUpload
→ UploadPart
→ CompleteMultipartUpload
→ HEAD
→ GET + SHA-256
→ DeleteObject
→ HEAD 404
```

The runtime image intentionally stays small and does not copy tests. To execute the smoke test with the production container's native addon and environment, copy a container-adjusted copy as done during deployment diagnostics, then run it inside `media-gateway`.

## Desktop acceptance

Build the final Genycaster source on the normal macOS Qt 6.11.1/Clang toolchain. Then verify with a newly uploaded asset (pre-lifecycle uploads do not have a management token):

```text
Upload new image
→ publish Cast
→ wait for authoritative Confirmed state
→ confirm object exists through stable media URL/QStorage console
→ delete Cast
→ wait for authoritative RemoveCast Confirmed state
→ gateway DELETE /v1/assets
→ object disappears from QStorage
```

Cleanup intent is durable in QSettings. A temporary network/gateway failure leaves the token queued and retries when network/signer readiness returns.

## Bucket privacy

The stable public media URL is `https://media-farcaster.genyleap.com/m/...`, which redirects to a short-lived presigned QStorage read. The QStorage bucket should therefore be private. After confirming stable media reads, disable QConsole's **Public Bucket** setting so direct provider URLs cannot bypass the gateway.
