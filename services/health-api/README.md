# Genycaster public health endpoint

Target canonical endpoint: GET https://status.genycaster.xyz/api/health (HEAD supported).
It requires the dedicated Nginx virtual host and an origin TLS certificate described
below; do not advertise it until public GET and HEAD tests succeed.

Currently deployed compatibility endpoint: GET https://genycaster.xyz/api/v1/cloud/health.
The existing account API proxies this route to the private /api/health worker.
This worker is a small dependency-free Node.js service, bound to 127.0.0.1:8798.
It runs independently of the Qt/WASM build. The compatibility bridge depends on the
account API and therefore may return HTTP 502 if that API itself is unreachable.
Once deployed, the dedicated status host accesses the worker directly.

Checks (all read-only, no user credentials):
- web: deployed genycaster.html, genycaster.js and genycaster.wasm exist and are nonempty
- backend: the local account API returns a nonempty provider catalog
- snapchain: the local gateway can fetch signer data for FID 1 from its upstream
- hypersnap: the configured public node returns valid /v1/info JSON

Response: JSON with service, status, checked_at and checks. Component statuses are
operational/unavailable. Overall operational -> HTTP 200, one protocol unavailable
-> degraded/HTTP 200, and either critical web/backend unavailable or both protocol
checks unavailable -> unavailable/HTTP 503. This measures *read-only reachability*;
it does not guarantee message submission, consensus progress or account writes.

Probes run concurrently, time out after 4 seconds, restrict response size and
coalesce/cache results in-process for 30 seconds. Public responses say no-store.
No raw upstream error messages, secrets, private endpoint URLs, network credentials
or per-user state are returned. The deployed service is intentionally independent
of the main account API so a failed account API remains observable.

Test:
  node --test services/health-api/test/server.test.mjs

Deployment on current Genycaster origin:
  install -d /opt/genycaster/caster-deployment/health-api
  cp services/health-api/server.mjs /opt/genycaster/caster-deployment/health-api/server.mjs
  cp services/health-api/start.mjs /opt/genycaster/caster-deployment/health-api/start.mjs
  pm2 start /opt/genycaster/caster-deployment/health-api/start.mjs --name genycaster-health-api
  pm2 save
  # Existing /api/v1/cloud/ proxy already exposes the account API bridge.
  # No Nginx modification is needed. nginx-location.conf.example documents
  # an optional shorter /api/health alias if Nginx config is writable later.

Health API port can be overridden via GENYCASTER_HEALTH_PORT. The HTTP listener is
always loopback-only. Keep the public upstream and gateway URL constants aligned with
the deployed Genycaster web configuration if they are changed in future.

## Dedicated status domain deployment

The hostname already resolves through Cloudflare to the same server, but currently
falls through to the separate Genyleap status virtual host and returns HTTP 404 on
/api/health. The existing Genycaster Let's Encrypt certificate does **not** cover
status.genycaster.xyz; use a new certificate for this hostname, never a mismatched
wildcard or Cloudflare edge-only certificate.

Run on the real VPS as root (requires writable /etc/nginx and /etc/letsencrypt):

    sudo env CERTBOT_EMAIL=your-admin-email@example.com \
      bash /opt/genycaster/repository/services/health-api/deploy/install_status_domain.sh

The installer refuses to overwrite an existing status vhost, verifies the running
health worker, creates only the status.genycaster.xyz Nginx vhost, performs an
ACME HTTP-01 challenge with Certbot, installs the dedicated HTTPS reverse proxy,
runs nginx -t before each reload and rolls back a new vhost on failure.
It does not change genycaster.xyz or status.genyleap.com. A Cloudflare proxied
wildcard DNS record currently routes this hostname to the Genycaster VPS.
If DNS routing changes, check the Cloudflare hostname before running the installer.

After installation, verify from outside and through the origin:

    curl -i https://status.genycaster.xyz/api/health
    curl -I https://status.genycaster.xyz/api/health
    curl --resolve status.genycaster.xyz:443:127.0.0.1 \
      https://status.genycaster.xyz/api/health

The compatibility API on genycaster.xyz remains available. Nginx returns the
actual worker status, including HTTP 503 for critical outages, and preserves JSON.
For Snapdiff, publish the new URL only after these live tests pass.

### Recovery from an initial TLS-check rollback

If Certbot has already issued the certificate but the installer rolled back its
newly created Nginx virtual host after a curl certificate verification error,
rerun the same installer. It detects and reuses the existing
status.genycaster.xyz certificate without requesting another certificate.
The updated installer retries the origin HTTPS test with full hostname and
certificate-chain validation, allowing old Nginx workers to finish serving
connections after a reload.

The fallback status.genyleap.com virtual host uses a Cloudflare Origin CA
certificate that plain curl should reject. Never use insecure TLS flags to
make the health check pass. Once the new virtual host becomes active, it serves
the dedicated Let's Encrypt status.genycaster.xyz certificate.

If a placeholder email was used for Certbot registration, update the ACME account
contact to a monitored administrative email. Certificate reissuance is not needed.
