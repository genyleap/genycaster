# Genycaster WebAssembly: reproducible caching and deployment

## What a browser refresh actually does

A Qt 6.12 WebAssembly page invokes the generated JS factory on **every document navigation**. WASM initialization/compilation and restoring Qt UI can therefore happen on every refresh, even when the actual `.wasm` bytes come from memory, disk cache, or the Service Worker. The boot screen showing "Loading" is **not** evidence of a new network download.

Open DevTools > Network with **Disable cache unchecked**; filter `wasm`.
Inspect **Transferred**, **Size**, **Status**, and the **Response Headers**.
A 304, `(disk cache)`, `(memory cache)`, or `x-genycaster-cache: hit`
means a full WASM body was not transferred. To measure cold start once, clear site data and use a new profile. Compare first visit, normal refresh, and deployment update separately.

## Build output and update contract

The final Emscripten link now invokes `tools/package_wasm_assets.py`.
It creates:
- `farcaster_client.<first-20-hex-of-SHA256>.wasm`, copied from the
  generated binary. **Only a changed WASM binary changes that URL.**
- `genycaster-assets.js`, exposing the trusted build-time WASM pathname.
  The shell imports it before starting Qt; the Qt/Emscripten `locateFile`
  override maps the original `farcaster_client.wasm` request to this URL.
- `genycaster-assets.json`, listing only public assets and their content
  checksums; its release version changes when JS/HTML/WASM contents change.
- `genycaster-sw.js`, `genycaster.webmanifest`, and a small SVG icon.

The build output also contains `genycaster-assets.js`, which must be deployed
with the HTML, and `genycaster-sw.js` plus `genycaster.webmanifest` and
`genycaster-icon.svg`. The Service Worker precaches the metadata script
without including it in its own version hash.

The generated original `farcaster_client.wasm` remains in the build output
for tools and Qt compatibility. **Do not cache the mutable original URL as
immutable.** Both files can be deployed, but the app uses the hashed URL.

Do not overwrite or remove previously deployed hashed `.wasm` assets during
a rolling release. Existing tabs may still request their old hash until they
reload. Upload new binaries/modules first, then publish HTML/JS/manifest.

## HTTP headers and Brotli

`docs/nginx-wasm-client.example.conf` is a **standalone client example**,
not a configuration for Snapchain/OpenProof APIs. Adjust domain, TLS certificate,
root and caching policy for the real deployment; check `nginx -t` before reload.

- Hashed WASM: `Cache-Control: public, max-age=31536000, immutable`.
- HTML, JS, MJS, JSON, Web Manifest, stable WASM: `max-age=0,
  must-revalidate` and ETag-based revalidation; unchanged files give a cheap 304.
- Serve WASM with `Content-Type: application/wasm`.
- Brotli can reduce the **initial** download (at a CPU/storage build cost):
  run `brotli -f -k -q 11 farcaster_client.<hash>.wasm` for the hashed file.
  Set `brotli_static on` **only if Nginx has its Brotli module enabled**.
  If unavailable, precompress to `.gz` and enable `gzip_static on` if supported.
  Test `Content-Encoding: br` and `Vary: Accept-Encoding`.
- Qt thread-enabled builds may need COOP/COEP and HTTPS. Enable the suggested
  headers only after checking third-party resources and workers.

For example, using your actual origin:

```sh
curl -I https://your-domain.example/farcaster_client.html
curl -I https://your-domain.example/farcaster_client.<actual-hash>.wasm
curl -I -H 'Accept-Encoding: br' https://your-domain.example/farcaster_client.<actual-hash>.wasm
```

## Offline behavior

The Service Worker is **optional** and installs only on secure origins
(HTTPS or trusted loopback). It intercepts **same-origin, public, static GET**
requests under its own scope. Content-hashed WASM uses cache-first; mutable
HTML/JS/MJS uses network-first, cached response only as an offline fallback.
It never intercepts or persists third-party identity, API, POST, signer,
custody, or social network requests.

The boot sequence briefly permits initial worker activation so the first
large WASM request can be cached without a second background download.
Installation can be blocked by browser settings; compilation still proceeds.
Only after required assets have actually entered CacheStorage can the shell
start offline. Remote posts, follows and feeds still need a connection.
Private mode, browser eviction, cleared site data, and browser-specific PWA
policies may discard cached assets.

The Web App Manifest provides basic standalone metadata and SVG icon. Full
install-prompt support is browser-dependent; some browsers require additional
raster icon sizes. This is **not** a guarantee of fully offline social features.

## Tests and rollout

```sh
node --test tests/wasm/*.test.mjs
python3 tools/check_ui_design.py .
python3 tools/check_localization_catalog.py .
```

A **real Qt 6.12 Emscripten Build + macOS Firefox/Brave + production HTTPS
test** is still required. Pay particular attention to the `locateFile` URL
and module MIME, cache headers, SW control timing, offline refresh, and
rollback with a previously deployed hashed binary.
