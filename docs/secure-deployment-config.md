# Secure deployment configuration

GenyCaster uses **CMake-validated deployment constants** for its native Qt and
WebAssembly clients. Do not edit URLs in application source files to move a
deployment. Keep identities and infrastructure roles separate: an RPC URL is
not an identity issuer, and a media gateway is not a billing authority.

## Configure the deployment

All public values below are **non-secret**, require canonical, credential-free
HTTPS DNS origins (without query, fragment, port, or embedded credentials), and
are validated before compilation.

| CMake cache variable | Role |
| --- | --- |
| `GENYCASTER_PUBLIC_ORIGIN` | Application origin; browser session/CORS and native account proof `Origin` |
| `GENYCASTER_SIWF_ORIGIN` | Separate trusted relying-party SIWF origin (may differ from hosting origin) |
| `GENYCASTER_OPENPROOF_ISSUER` | Trusted identity issuer; independently verified in native recovery |
| `GENYCASTER_OPENPROOF_BROWSER_BASE` | Browser OAuth handoff, exactly `https://host/openproof` |
| `GENYCASTER_BROWSER_GATEWAY_ORIGIN` | Public image, upload, catalog, reaction and recovery gateway |
| `GENYCASTER_MEDIA_PUBLIC_ORIGIN` | Generated managed-media delivery URL authority |
| `GENYCASTER_SNAPCHAIN_ORIGIN` | Snapchain read fallback; not permission to cross-post |
| `GENYCASTER_HYPERSNAP_ORIGIN` | Default HyperSnap node; preferred protocol remains HyperSnap |
| `GENYCASTER_COMPANY_ORIGIN` | Public legal/company links; not identity authority |

Example local configure:

```sh
cmake -S . -B build/web -G Ninja \
  -DGENYCASTER_PUBLIC_ORIGIN=https://app.example.org \
  -DGENYCASTER_SIWF_ORIGIN=https://signin.example.org \
  -DGENYCASTER_OPENPROOF_ISSUER=https://identity.example.org \
  -DGENYCASTER_OPENPROOF_BROWSER_BASE=https://example.org/openproof \
  -DGENYCASTER_BROWSER_GATEWAY_ORIGIN=https://gateway.example.org \
  -DGENYCASTER_MEDIA_PUBLIC_ORIGIN=https://media.example.org \
  -DGENYCASTER_SNAPCHAIN_ORIGIN=https://snap.example.org \
  -DGENYCASTER_HYPERSNAP_ORIGIN=https://hyper.example.org \
  -DGENYCASTER_COMPANY_ORIGIN=https://example.org
```

Add the usual Qt SDK, host toolchain and packaging flags for the platform.
The WASM server script accepts the same variables as repeated
`--define GENYCASTER_...=https://...` arguments. No arbitrary CMake variable
or Shell program is accepted through this interface.

CMake invokes `ci/generate_deployment_config.py` and writes to the **build
directory**, not the source tree:

- `generated/genycaster_deployment.hpp` — type-safe `inline constexpr` C++
  deployment values.
- `generated/public_endpoints.json` — Qt resource consumed by connections.
- `generated/deployment_config.mjs` — non-secret WASM module copied to the
  published public assets and included in their hash manifest.

The `ui/wasm/deployment_config.mjs` tracked file is a test/source-tree
fallback only. Production builds must ship the CMake-generated module.
The old public endpoints source JSON has moved to
`tests/fixtures/public_endpoints.json` for independent source-level tests.
It is **not** the application's deployed configuration.

The generated `deployment.env` contains **public endpoint variables only**,
including `GENYCASTER_DEPLOYMENT_STRICT=1`. Import it into the matching server
deployment, then load secrets independently from the server secret store.
For self-hosted origins, missing security-critical server endpoint variables
cause startup to fail closed instead of silently talking to the Genyleap
production issuer. Serve `deployment_config.mjs` with
`Cache-Control: no-store` (including at the CDN) to avoid stale authority
information in older browser tabs. This module is excluded from the service
worker's offline cache.

## Canonical Genycaster application domain

Genycaster's dedicated first-party origin is **https://genycaster.xyz**.
Both the CMake-default public Origin and the shipping SIWF relying-party
authority now use this host. Existing Qt CMake caches carrying the two
former Genycaster-default values are migrated at configure time; deliberate
self-hosted overrides remain intact. Reconfigure and rebuild **every native
platform and WebAssembly** so the generated `genycaster_deployment.hpp`,
`public_endpoints.json` and browser `deployment_config.mjs` agree.

After the reviewed canonical WASM migration, `https://genycaster.xyz/`
serves the browser application. The former standalone landing page is
preserved at `/about/`, with the release selector at `/download` redirecting
to its download section. The exact cutover procedure, backup and rollback
requirements are in `ci/PRIMARY_WASM_MIGRATION.md`.

The Account API stays on the SAME first-party domain under `/api/v1/cloud/`;
do not redirect it, move its cookies to another domain or replace its
canonical issuer. The earlier landing-only Nginx helper is
`ci/enable_genycaster_xyz_cloud_api.py`; the primary WASM vhost retains
the same proxy contract. Verify public HTTPS:
`GET /api/v1/cloud/pro/plan` -> 200,
unauthenticated `GET /api/v1/cloud/session` -> 401,
a same-origin `POST /api/v1/cloud/session/device/challenge` with valid
protocol/FID/public-key/purpose fields -> 200 (challenge is not authorization),
and `GET /` -> 200. The backend must already trust
`Origin: https://genycaster.xyz` before applying the proxy.

**Never rewrite origin-specific cookies.** The application account cookie
is host-only, Secure and SameSite=Strict. Migrating the origin requires a fresh verified enrollment for the new
origin-scoped cloud-device key when the prior key is unavailable. An existing
social signer is not enrollment authority; it does not require a new FID or
change Pro/Streak ownership. Do not
treat any old session on `caster.genyleap.com` as reusable on the new host.

OpenProof remains the distinct identity issuer and OAuth broker; Snapchain,
HyperSnap and media gateway remain separately trusted protocol/service hosts.
Do not replace those origins with the landing hostname unless those services
are actually hosted there. Their browser CORS allowlists must independently
accept **only** the new exact application Origin (plus explicit development
origins where already permitted). This is especially important for the
Snapchain signer-protobuf gateway and the media upload/browsing gateway.
Verify the new origin's preflight responses after deploying those services;
a source update alone does not update running systemd/Docker processes.

## Server configuration and secrets

The Node account-sync and media-gateway services use
`services/deployment_config.mjs` with matching, deployment-admin-controlled
environment values. Keep their `GENYCASTER_PUBLIC_ORIGIN`,
`GENYCASTER_OPENPROOF_ISSUER`, `GENYCASTER_BROWSER_GATEWAY_ORIGIN`,
`GENYCASTER_SNAPCHAIN_URL`, and `GENYCASTER_HYPERSNAP_ORIGIN` aligned with
the client build. `GENYLEAP_MEDIA_PUBLIC_BASE_URL`,
`GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL`, and
`GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE` remain supported for the
gateway's existing deployment interface.

**Never** put an OAuth client secret, SQL connection string, API key, session
cookie, recovery phrase, wallet private key or signer token in CMake cache
variables, generated files, Qt resources, public JavaScript, browser local
storage, or build logs. Load server secrets from the deployment's protected
secret mechanism, with appropriate file permissions and secret rotation.

The build-time authority list is deliberately immutable from QML, arbitrary
browser JS, user preferences or untrusted environment variables. Users may
choose valid protocol nodes via the separate provider settings layer; doing so
does not authorize changing the account issuer, payment treasury or cloud
cookie authority.

## Security and migration checklist

1. Configure new HTTPS hosts, TLS, DNS, reverse proxies, callback routes,
   provider OAuth registrations, and first-party browser `Origin` permissions.
   Do **not** allow `Access-Control-Allow-Origin: *` on credentialed APIs.
2. If the identity issuer changes, explicitly plan migration of old sessions,
   outstanding grants, issuer- and domain-separated recovery challenges,
   cookies, OAuth state/PKCE/redirect registrations and SIWF identity.
   The issuer is not a generic user-editable service URL.
3. Update backend environment **together** with the matching client build.
   Verify proof challenge, recovery, signer registry, Profile Extras,
   Mini App discovery, image proxy, upload and media delivery.
4. Preserve previously published immutable WASM hashes for open browser tabs;
   publish a new manifest and service worker alongside the new client. Never
   copy a generated JS module from a different build/deployment.
5. Check CMake configure rejects injection, URL credentials, IP literals,
   unexpected paths and mismatched identity origins. Check unsupported Origin
   and lookalike domains receive 403 without CORS approval.
6. Run the security gate, deployment-domain tests and platform build:
   `python3 -m unittest ci/test_deployment_config.py`,
   `python3 ci/check_deployment_boundaries.py`,
   `node --test services/test/deployment_config.test.mjs`,
   and the normal native/WASM suite. Confirm actual HTTP responses before
   switching a production release.
7. A server move without changing a public domain normally needs only DNS
   and backend configuration changes, not a rebuild. Changing trusted public
   origins requires a coordinated new client/deployment release.

Build-time URL validation does **not** itself eliminate XSS, SSRF or SQL
injection. Continue using safe DOM APIs, QML plain text or explicit rich-text
sanitization, bounded SSRF-aware server image proxies, exact-origin CSRF
checks, strict response sizes and redirects, and server-authoritative payment
verification. The on-chain Chain ID, token, treasury and amount are verified
independently of a network endpoint.

## Reviewed compatibility exceptions

Existing media and profile records may still contain the former media gateway,
media-delivery host or issuer-scoped cache key. Their **read-only migration
recognition** is deliberately retained in `ProfileSurface.qml`, Mini App
catalog restoration, the media classifier and the server image proxy.
These old hosts must never be used to create new sign-in challenges, route
client uploads, select new cloud endpoints or authorize payment. The CI
boundary audit checks for newly introduced bare-host strings as well as
full URL literals, with narrow compatibility exceptions.
