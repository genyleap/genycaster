# Canonical GenyCaster WASM website migration

## Current destination

- `https://genycaster.xyz/` — canonical WebAssembly application on the
  existing trusted first-party Origin.
- `https://genycaster.xyz/about/` — preserved historical product landing.
- `https://genycaster.xyz/download` — redirects to the original download
  section on the preserved landing.
- `https://genycaster.xyz/api/v1/cloud/` — unchanged host-only account API
  (never redirect or expose cross-origin session cookies).
- `https://app.genycaster.xyz` — previous public WASM preview. Keep it
  operational until the canonical main-site migration passes live tests;
  implement an HTTP redirect as a separate reviewed compatibility step.

The primary app's adaptive **Get the app** dialog offers Windows x64/ARM64,
macOS Intel/Apple Silicon, Linux x86_64/ARM64, and Android arm64-v8a.
It is a desktop-centered dialog or a mobile bottom sheet. It fetches
`/releases/native.json` using an origin-local no-store request, and only
enables official, checksum-backed GitHub Release links. If no publicly
released packages exist, all choices remain "Not published yet" — never
invent a platform installer or show a misleading availability nudge.

## Verified preview currently staged (November 19, 2026)

The older successful November 18 `v1.0.0-rc1` WASM artifact was built for
`https://genycaster.xyz` for both `GENYCASTER_PUBLIC_ORIGIN` and
`GENYCASTER_SIWF_ORIGIN` and is appropriate as an **origin-consistent
preview**, not as the current-HEAD final release. The source changes since
its original compilation are *not* represented in its existing WASM binary.
Its HTML was updated with the adaptive download overlay, and all asset hashes
and the HTML cache-busting fingerprint were regenerated in a private staging
area. No native GitHub Release has been created or advertised.

Preview assets now reside in `/var/www/genycaster.xyz/public`.
The old `index.html` landing remains in place, with a separate identical
copy at `/var/www/genycaster.xyz/public/about/index.html`. The site still
serves the old landing at `/` until its Nginx vhost is switched.

Before activation, verify on the VPS:

```bash
cd /opt/genycaster/repository
git pull --ff-only
python3 ci/verify_primary_preview_payload.py /var/www/genycaster.xyz/public
python3 ci/activate_primary_wasm_site.py
python3 -m unittest -q ci.test_primary_wasm_migration ci.test_wasm_domain_scope
```

## Activate (requires root on the VPS)

Remote Operations currently permits writing to the web root but denies direct
writes to `/etc/nginx`, so the root operator must perform the final switch.
The activator preserves the previous Nginx vhost in a mode-0600 file outside
the public web root, requires existing certificate identity, checks the
primary-domain HTML and metadata, runs `nginx -t` and reloads the service,
then verifies both the local TLS origin and Cloudflare endpoint. It automatically
rolls back an invalid local origin, never touches other domain vhosts, and
preserves a valid origin when only the CDN routing fails.

```bash
cd /opt/genycaster/repository
git pull --ff-only
sudo python3 ci/activate_primary_wasm_site.py --apply
```

Exit **0** and `PUBLIC READY: ... = genycaster-main-v1` means the origin and
the public Cloudflare route both passed. Exit **2** means Nginx verified but
Cloudflare verification failed — investigate CDN routing without deleting the
working origin. Exit **1** means a local failure and rollback.

```bash
curl -fsS https://genycaster.xyz/.well-known/genycaster-ready
curl -sSI https://genycaster.xyz/
curl -sSI https://genycaster.xyz/about/
curl -sSI https://genycaster.xyz/download
curl -sSI https://genycaster.xyz/releases/native.json
curl -sSI https://genycaster.xyz/api/v1/cloud/session
```

Expected: marker `genycaster-main-v1`; both root and /about HTML 200;
download redirect 302 to /about/#downloads; native catalog 200; an
unauthenticated cloud/session response 401; JS modules and hashed WASM 200 with
correct MIME types. **An HTTP 200 is not proof that Qt's user interface has
started** — verify in real desktop/mobile browsers and exercise sign-in,
external-wallet pairing, and Mini Apps separately.

If a post-activation rollback becomes necessary, the backed-up Nginx file
at `/var/www/genycaster.xyz/primary-wasm-previous-nginx.conf` is preserved
outside the public root. Restore only with explicit operator authorization
and always run `nginx -t` before reloading. Do not delete user account data
or change the API host during rollback.

## Later: official native releases

Once the full platform CI matrix, persistent Android release signing,
macOS ad-hoc code-signing audits (or optional Developer ID notarization),
and release gates have genuinely passed and GitHub's tagged Release exists:

```bash
python3 ci/generate_native_release_catalog.py \\
  --release-dir /path/to/verified/release \\
  --tag v1.0.0 \\
  --output-dir /path/to/catalog-output
```

This validates exact native installer bytes against the Release
`SHA256SUMS` and produces **both** `native.json` (in-app dialog) and
`releases.json` (preserved landing) from the same published release. Only
after verifying the GitHub Release is public may those files replace
`/var/www/genycaster.xyz/public/releases/native.json` and
`/var/www/genycaster.xyz/public/releases.json`.

A current-HEAD WASM build can later be produced directly on the VPS using
`ci/build_deploy_wasm_server.sh` and the matching Qt WASM SDK, after its
canonical public HTTPS marker is active. This requires no GitHub Actions
minutes and performs manifest verification before promotion.
