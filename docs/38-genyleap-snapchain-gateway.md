# 38 — Genyleap Snapchain Gateway

## Responsibility

The desktop client uses one stable protocol origin:

```text
https://snapchain.genyleap.com
```

That origin is a deployment boundary, not a protocol implementation and not a provider identity. The gateway forwards only the Snapchain HTTP operations Genycaster currently requires and injects any upstream credential on the server.

```text
Genycaster Desktop
    │ signed/public protocol traffic only
    ▼
snapchain.genyleap.com
    │ deployment-owned forwarding
    ├── managed writable Snapchain origin today
    ├── another provider later
    └── self-hosted Snapchain later
```

Changing the upstream must not require changing canonical domain models, application services, the durable outbox, signing code, or a desktop release.

## Client contract

The Snapchain provider adapters receive only:

- `baseUrl`;
- optional generic HTTP authorization header name/value;
- timeout/body limits.

They do not inspect hostnames and contain no vendor-specific URL, credential name or branching logic. Shipping composition defaults to `https://snapchain.genyleap.com`; `GENYCASTER_SNAPCHAIN_BASE_URL` / `--snapchain-base-url=` remain advanced overrides.

Direct-to-node development may optionally set both:

```text
GENYCASTER_SNAPCHAIN_AUTH_HEADER=<header-name>
GENYCASTER_SNAPCHAIN_AUTH_VALUE=<secret-value>
```

The release path does not set either value.

## Gateway contract

The service under `services/snapchain-gateway/` owns the current upstream configuration:

```text
GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL
GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER
GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_VALUE
```

Only these paths are forwarded:

- `GET /v1/signersByFid`
- `POST /v1/submitMessage`
- `GET /v1/castById`
- `GET /v1/reactionById`
- `GET /v1/linkById`

Inbound cookies/credentials are not forwarded. The upstream authorization header is created from server-side configuration. Redirects are rejected so that an upstream secret cannot be redirected to another origin. Request/response sizes, timeout, and GET/POST rate limits are bounded.

The gateway never possesses a user custody private key or the device Ed25519 private key. `submitMessage` forwards an already signed Farcaster message envelope.

## Cloudflare/origin policy

The public DNS record may remain proxied through Cloudflare. Production origin policy is:

- Full (strict) TLS;
- no API caching;
- WAF/rate limits at the edge;
- gateway bound to loopback behind the origin reverse proxy;
- origin firewall restricted to Cloudflare ingress, or Cloudflare Tunnel;
- trust `CF-Connecting-IP` only after direct origin access is blocked.

## Legacy session upgrade

Persisted sessions created before custody-address verification are upgraded during `SessionService::restore()`. The compatibility authentication boundary resolves `IdRegistry.custodyOf(fid)` through the configured Optimism RPC, persists the normalized custody address and continues the authenticated session. A new SIWF ceremony is no longer required solely because the local row predates the custody field.
