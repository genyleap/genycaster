# Genyleap Snapchain Gateway

A configured HTTPS origin (for example `https://node.example.com`) is the GenyCaster-facing protocol endpoint. The desktop client never knows which managed provider or self-hosted Snapchain node sits behind it and never receives the upstream credential.

## Responsibility

The gateway is deliberately small. It forwards only the protocol endpoints currently required by Genycaster:

- `GET /v1/signersByFid`
- `POST /v1/submitMessage`
- `GET /v1/castById`
- `GET /v1/reactionById`
- `GET /v1/linkById`

`GET /healthz` is local gateway health. Every other route is rejected.

The gateway never receives a user custody private key or Ed25519 private key. `submitMessage` receives an already signed canonical Farcaster protobuf envelope and forwards the exact bytes.

## Upstream configuration

Copy `.env.example` to `.env` on the server. The upstream is deployment data, not desktop-client code:

```text
GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL=https://writer.example.com
GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER=<optional HTTP header name>
GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_VALUE=<optional secret>
```

Changing those values moves the gateway to another managed service or to a self-hosted node without shipping a new Genycaster build.

## Run with Docker Compose

```bash
cp .env.example .env
# Edit .env and set the real upstream/credential.
docker compose up -d --build
curl -i http://127.0.0.1:8787/healthz
```

The compose file publishes the service only on loopback. Terminate public TLS in the origin reverse proxy. `deploy/nginx.conf.example` shows an example `node.example.com` vhost.

## Cloudflare

For a proxied DNS record:

1. Use **Full (strict)** TLS, not Flexible.
2. Install a Cloudflare Origin Certificate (or another valid origin certificate).
3. Keep API caching disabled (`Cache-Control: no-store`).
4. Restrict the origin firewall to Cloudflare ingress ranges or use Cloudflare Tunnel. Do not expose port 8787 publicly.
5. Apply Cloudflare WAF/rate limits in addition to the gateway's local fixed-window limits.
6. Set `GENYLEAP_SNAPCHAIN_TRUST_CLOUDFLARE=1` only after direct origin access is blocked; otherwise an attacker could spoof `CF-Connecting-IP`.

The service strips client credentials/cookies and injects only the configured upstream authorization header. Upstream redirects are rejected so a credential cannot be redirected to another origin.
