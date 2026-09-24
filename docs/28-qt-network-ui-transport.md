# 28 — Qt Network Transport For The Graphical Runtime

## Problem

The first runnable UI composition reused `farcaster.transport.curl` directly.
On macOS this meant the graphical application inherited the behavior of the
selected system libcurl build rather than the networking stack already owned by
the Qt runtime. A reported runtime failure reached the provider route correctly
but stopped at TLS with libcurl's generic `SSL connect error`.

The failure was transport-level: routing, provider registration, and provider
JSON decoding had not yet executed for the request.

## Decision

Keep `farcaster.transport.http` as the provider-neutral port, but let the
Qt graphical composition use a Qt Network transport implementation:

```text
Hypersnap provider adapter
        ↓
farcaster.transport.http
        ↑
        ├── farcaster.transport.curl
        │      provider/core contract builds
        │
        └── farcaster.transport.qtnetwork
               runnable Qt UI
```

The Qt implementation lives under `src/transport/qt_network/` because it
directly implements the HTTP transport port. It remains a separate Qt-linked
target, so Qt does not leak into the provider-neutral transport module or other
core targets. `src/platform/` is reserved for adapters that implement
application/platform ports rather than transport mechanisms.

## macOS behavior

The Qt adapter:

- uses the platform proxy configuration by default;
- therefore reads macOS HTTP/HTTPS/SOCKS proxy configuration through CFNetwork in `system` mode;
- supports an explicit `direct` composition policy that installs `QNetworkProxy::NoProxy` on each request manager instead of silently falling back around a user-configured proxy;
- uses one thread-local `QNetworkAccessManager` per bounded live-read worker;
- retains certificate verification;
- never calls `ignoreSslErrors()`;
- captures `QSslError` descriptions for Debug diagnostics;
- reports the active Qt TLS backend for handshake failures;
- preserves HTTP status/body semantics for the provider adapter;
- keeps timeout and response-size limits at the provider-neutral transport port.

Qt's native TLS plugin is available on Darwin builds, while an OpenSSL backend
may also be selected by Qt when present. The application does not hard-code a
TLS backend.

The graphical composition selects proxy policy with `--network-proxy=system|direct`
or `GENYCASTER_NETWORK_PROXY=system|direct`. `system` remains the default so a
shipping build does not bypass an OS-level privacy/corporate proxy without an
explicit user choice. `direct` is useful when the OS proxy/PAC configuration is
stale or cannot carry the application's HTTPS traffic.

## Security invariant

A TLS failure is never repaired by disabling peer verification. If the active
platform trust store rejects a certificate, the request remains failed and the
specific TLS diagnostics are surfaced to the existing read-error pipeline.

## Verification contract

`ui.runtime_smoke` already launches the real executable against a local HTTP
fixture and therefore exercises the UI-selected transport through:

```text
QML/presentation bootstrap
→ routed read gateway
→ Hypersnap adapter
→ farcaster.transport.qtnetwork
→ local HTTP fixture
→ canonical decode
→ SQLite
```

On macOS the final acceptance additionally requires a real HTTPS run against the
configured public node. The Debug Details panel should show a Qt Network error
and TLS backend if HTTPS still fails.
