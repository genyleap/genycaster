# Genycaster public health endpoint

Public endpoint: GET https://genycaster.xyz/api/v1/cloud/health (HEAD supported).
The existing account API proxies this route to the private /api/health worker.
This worker is a small dependency-free Node.js service, bound to 127.0.0.1:8798.
It runs independently of the Qt/WASM build. The public bridge depends on the
account API and therefore returns HTTP 502 if that API itself is unreachable.

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
