# WebAssembly composition (Qt 6.12 / Emscripten)

**Status: experimental read-first build; not a release-certified browser client.**
A configured WASM Qt kit is required to compile and actually test this path.
This repo's Linux native CI container does not contain the matching Emscripten
Qt kit, so a successful native test does NOT prove WASM execution.

## Dependency boundaries

The live Hypersnap reader parses JSON with **Boost.JSON**, compiled via
`boost/json/src.hpp` (header-only). Qt Creator's cross-build may reject
`find_package(Boost)` because BoostConfig is absent from the WASM sysroot.
`cmake/platform/WebAssembly/configure.cmake` supplies only an INTERFACE `Boost::headers`
target: it first tries native **headers** from the host (not macOS binary
libraries), then falls back to the official, SHA256-pinned Boost 1.89.0
source archive (about 155 MB, cached per build directory). Optional
`GENYCASTER_WASM_BOOST_INCLUDE_DIR` points to an existing include root with
`boost/json.hpp`. Boost's JSON amalgamation is compiled using em++, so
no macOS Boost binaries ever enter the browser application. The Qt Creator
MaintenanceTool platform warning is unrelated to Boost dependency resolution.

Do not resolve Homebrew/macOS SQLite for wasm32. `src/storage/CMakeLists.txt`
compiles the official SQLite 3.54.0 C amalgamation via the selected Emscripten
compiler (pinned SHA3-256), and links it to the existing SQLite boundary.
C language is enabled only on the WASM target. For reproducible offline builds,
seed CMake's FetchContent source directory with the same verified archive.

## Browser-managed TLS and asynchronous HTTP

Qt 6.12 WebAssembly deliberately omits native `QSslSocket`, the
`QNetworkReply::encrypted` and `sslErrors` signals, and parts of
`QSslError`. The Genycaster Qt transport therefore compiles TLS diagnostics
and backend checks only for native targets. For WebAssembly the browser
performs certificate verification and enforces CORS, secure contexts and
mixed-content restrictions; the app must **never** suppress those failures
or treat the missing Qt SSL API as an insecure TLS fallback.

## Single-threaded browser execution

Qt's default browser kit may run without native pthread support. The
application must not construct `std::thread`, `std::jthread`, or start
`QThread` from that profile. The serial and weighted CPU executors
dispatch bounded FIFO work through Emscripten's event loop (one job per
turn); native macOS/Linux builds retain worker threads. WASM CPU jobs
still execute on the browser main thread and must remain small to avoid
blocking rendering. On shutdown the browser executors cancel queued
jobs rather than synchronously waiting on an event loop.

Bootstrap initialization is posted to the Qt event loop instead of
creating a `std::jthread`. Native-thread-based link metadata preview
fetching falls back to URL-only previews, while Mini App discovery
remains disabled. Browser support is intentionally read-first; these
fallbacks must not be interpreted as full native feature parity.

The synchronous `IHttpTransport` returns an explicit `Unavailable` error
on WebAssembly instead of entering a nested blocking `QEventLoop`.
`IAsyncHttpTransport` uses the browser's Qt main event loop for the
`QNetworkAccessManager` reactor, avoiding `QThread` creation and
`Qt::BlockingQueuedConnection` in single-threaded WASM builds. Requests
and cancellations remain queued through the event loop. Native builds keep
their threaded Qt reactor and existing SSL diagnostics. Other unrelated
application executors are not yet certified on WebAssembly.

## Direct Farcaster sign-in in browser (identity verification)

The direct Farcaster option is a separate OpenProof-bound SIWF flow. Qt sends
`POST /v1/browser/farcaster/start` to the first-party Media Gateway. That
backend creates the OpenProof `/auth/web3/start` challenge, retains
the HttpOnly ceremony cookies and Farcaster Relay channel token in a bounded
five-minute, in-memory attempt, and returns only an opaque random ticket and
a first-party-approved HTTPS Farcaster authorization URL.

Qt requests approval from the user in Farcaster and polls with a POST body,
not an URL query. The backend passes the signed SIWE message to OpenProof
`/auth/web3/complete`, checks `/account/profile` and exactly one linked
Farcaster connection, and refuses nonce/FID mismatches. No relay token,
OpenProof cookie or unverified identity is exposed to QML. The verified
identity lives in the app's transient session and expires after one hour.
An interrupted/restarted gateway invalidates in-flight attempts;
they must be restarted instead of silently trusting expired proof state.

Identity sign-in is **not** protocol signer authorization. The account UI
distinguishes **Sign in with Farcaster** from OpenProof's validation of the
signed proof. Successful login alone never sets `writeReady`.

## Browser signer, custody approval and canonical writes

The browser uses an asynchronous WebCrypto/IndexedDB signer path, separate from
the synchronous native OS-keychain `SignerService` and `WriteService`:
`ui/wasm/secure_signer_vault.mjs` creates a **non-extractable**, scope- and FID-bound
Ed25519 private CryptoKey. Its key material is never transmitted and never
exported as a plaintext seed. `farcaster_protocol.mjs` implements the exact
native BLAKE3-20 and protobuf wire format for scoped `KEY_ADD`, Casts, replies,
threads, reactions, follows and self-revoking `KEY_REMOVE`, with golden C++
fixtures. `browser_protocol_signer.mjs` manages the following protocol steps:

1. Require a current verified Farcaster FID and an injected **EIP-1193 custody
   wallet** (for example, a compatible extension). Check the selected
   account against `IdRegistry.custodyOf(fid)` on **Optimism chain 10**.
2. Create/recover the local Ed25519 signer key from IndexedDB. Read the
   network's authoritative `currentUserNonce` over the HTTPS Snapchain
   gateway. Request **both** `SignedKeyRequest` and `KeyAdd` EIP-712
   signatures from the custody wallet with explicit FID, scopes, nonce, TTL
   and deadline. Sign canonical `KEY_ADD` with the local Ed25519 key.
3. Submit the signed message to `/v1/submitMessage`, compare the returned
   hash to the locally calculated hash, and poll `/v1/signersByFid` until
   the signer key, FID, scope set and nonce appear as active. Only then
   may `writeReady` become true.
4. Recheck registration, required scope and expiry before **every** write.
   The same origin-isolated key signs protocol-native messages; unsigned
   drafts are never advertised as published network Casts. A failed or
   ambiguous submission is **not silently retried** to avoid duplicates.
5. Self-revocation requires a confirmed requester nonce. The browser does not
   invent a nonce when the network omits it; signer status fails closed.

Qt WebAssembly talks to browser Promises through a bounded Emscripten event
queue (`protocol-signer-pre.js` and `browser_signer_bridge.hpp`), never a
nested Qt event loop or `std::thread`. The existing QML Composer routes to
this signed writer; native desktop composition is left unchanged.
The source Gateway implements a restricted CORS surface for
`https://app.example.com` and local `127.0.0.1`/`localhost`
development origins; `0.0.0.0`, file URLs and untrusted sites are excluded.
**Deployment is mandatory:** the configured node service (for example `https://node.example.com`)
must be updated/restarted and its CORS preflight verified before the
WASM browser can call `/v1/submitMessage`. Merely building the client or
changing the repository's Gateway source does not configure the live server.
The Gateway forwards **only already-signed protocol bytes**, not wallet
private keys, bearer tokens or recovery phrases.

**Requirements and limitations:** the user must explicitly approve the
wallet's custody EIP-712 requests. The EIP-1193 wallet must have custody
control onchain; an identity proof from OpenProof alone does not authorize
signing. WebCrypto requires HTTPS or a loopback secure context. Qt
Creator's `emrun` may open the server's bind-all address
`http://0.0.0.0:30000`, which browsers do **not** treat as secure.
The WASM startup bridge redirects that precise origin to
`http://127.0.0.1:30000`, preserving the path, query and fragment; it never
rewrites non-local HTTP URLs. The browser signer reports insecure context,
missing SubtleCrypto and unavailable IndexedDB separately. Inspect
`window.__genycasterSignerEnvironment` in the DevTools console to see
`origin`, `secureContext`, `webCrypto` and `indexedDB`. All but
`origin` must be `true` for key provisioning. Browser privacy features or
private-window storage restrictions can still prevent IndexedDB operations
even when the API is present. Switching origins does **not** migrate existing
IndexedDB signer keys; registration must still be verified for the current
origin before enabling writes.
For users who own an existing Farcaster FID via a BIP39 custody wallet,
WASM now offers **Import Farcaster recovery phrase** instead of requiring a
wallet extension: the browser verifies 12/24 words, derives custody with
BIP44, checks `custodyOf(fid)` directly on Optimism, and performs the two
KeyAdd EIP-712 signatures in memory. The phrase is not sent to any server or
saved locally; only the independently authorized browser Ed25519 signer is
persisted. See [browser recovery design](farcaster-recovery-wasm.md) for
the Quorum Mobile reference, trust model, unsupported smart-contract custody,
key lifetime and device-recovery limitations.
A browser origin compromise could request signatures from an otherwise
non-exportable CryptoKey; this is not OS-keychain isolation. A successful mocked test is
not proof that an individual user's wallet has registered a signer:
a live Qt 6.12 WASM browser test, wallet approval and Snapchain-confirmed
`KEY_ADD` are still needed. Media upload and Mini App wallet SDK parity
remain separate work.

## Independent browser cloud-device sessions

`browser_account_session.mjs` uses a separate IndexedDB key scope
(`cloud-device-farcaster` or `cloud-device-hypersnap`) for enrolled private
product-service access. This is independent of the local continuity seal
and social signer. Enrollment requires fresh verified OpenProof authorization
or HyperSnap-native custody proof; subsequent device challenge/proof restores
a host-only HttpOnly cookie. The exact protocol/FID/issuer/subject is checked
on restore. Sign-out queues a signed revocation-only receipt before erasing
the cloud key; delivery retries without an active account. No OAuth bearer,
mnemonic or private key is persisted in ordinary web storage. An origin
compromise can still invoke non-extractable keys, so browser storage is not
an OS security boundary. See [security design](08-security-auth-and-signing.md).

## Browser OpenProof sign-in (read-only identity)

The browser uses OpenProof's public `genycaster-native-v1` OAuth device
grant through `https://identity.example.com`. The Qt Network adapter
starts the device challenge, polls the access token, verifies the canonical
subject via `/oauth/userinfo`, and requires exactly one server-confirmed
linked Farcaster FID from `/account/connections`. Since that account API
does not expose CORS directly, the Genycaster-owned HTTPS Media Gateway
forwards **only** `GET /account/connections` under
`/v1/browser/openproof/connections`, using the supplied Bearer token and
no cookies or configurable upstream. The response never includes the token.

The access token is kept transiently in Qt memory and zeroed after
verification; OAuth credentials are not persisted. A separate non-extractable
local continuity key seals the protocol-scoped viewer snapshot for up to
30 days. This is not a renewed issuer token or account-service cookie. Identity login is not a Farcaster protocol
signer. The separate browser writer above requires custody-wallet approval
and on-chain Snapchain registration before enabling protocol actions.
The Genyleap account must already have exactly one linked Farcaster identity;
linking/registration is completed in Genyleap's owned account experience.

The browser cannot expose a native OS keychain or bind the app's localhost
custody-authorizer server. The wizard therefore retains the explicit
**Continue as guest** path on WebAssembly, after reviewing Terms and Privacy.
Native builds retain mandatory authenticated setup. The native secure
signing service and synchronous Snapchain write service remain excluded from
WASM **in favor of the asynchronous browser protocol signer**. Native media
uploads, native Mini App wallet SDK parity and other unsupported capabilities
remain separate. Guest reads still depend on each upstream's CORS policy.

Emscripten's IDBFS stores SQLite files below
`/home/web_user/.local/share` into IndexedDB. A preRun hook waits for
`FS.syncfs(true)` before the app opens the database, while an 8-second
Qt timer asynchronously triggers `FS.syncfs(false)` for persistence.
Browser private mode, quota exhaustion, and sudden tab termination may
still lose recent writes; **do not claim durable protocol writes in WASM**.
Unconfirmed/failed local write projections are no longer resurrected into
the public feed after a browser reload. Failed writes are removed from the
optimistic feed immediately on failure on every platform. Durable outbox
records are retained for diagnostics/recovery rather than silently deleted
or represented as successfully published Farcaster casts.
CORS and HTTPS requirements also apply to external Farcaster/Hypersnap nodes.

## Public Mini Apps in the browser

The single-threaded WASM Mini App catalogue issues bounded asynchronous
`QNetworkAccessManager` reads on Qt's GUI event loop, without a `QThread`.
The first-party Media Gateway exposes only fixed, rate-limited public Neynar
catalogue/search/relevant routes at `/v1/browser/miniapps`; the public
catalogue uses a bounded 20-item response to avoid CDN truncation (deeper
pagination is not yet implemented). User-controlled
upstream hosts, credentials and arbitrary paths are not accepted. The upstream
API key never enters WebAssembly. An unprivileged in-app browser
host displays embeddable public Mini Apps inside a sandboxed HTTPS iframe
with a built-in title/domain bar and explicit close control. It does **not**
open an external tab. The iframe is isolated from QML account secrets and
does not silently inherit a Farcaster signer or EVM wallet.

Unlike macOS WKWebView, a browser cannot inject the desktop's
`ReactNativeWebView` SDK shim at the start of a cross-origin iframe
document. Accordingly this is **not full Farcaster Mini App SDK parity**:
privileged SDK calls/wallet/compose are not exposed. Remote sites with
a restrictive `Content-Security-Policy: frame-ancestors` or X-Frame-Options
cannot be embedded by an unrelated origin and need a protocol-compliant
separately authorized host. Native macOS continues using its full isolated
WebKit Mini App runtime.

## Global browser mouse wheel dispatch

Qt 6.12 creates its canvas dynamically. The WebAssembly composition now
registers Emscripten's native `emscripten_set_wheel_callback` with capture on
the window, rather than calling an application export from JavaScript via
`Module._genycaster_browser_wheel`. That export is not reliably exposed by
Qt's generated loader. The callback reads the browser-provided wheel deltas,
normalizes pixel/line/page units, and uses a passive capture-phase observer
of the actual DOM event composed path for Qt shadow-root canvas targeting.
It falls back to DOM hit testing if necessary. The prior strategy of
synthesizing and sending only a `QWheelEvent` proved unreliable for WASM
Flickables even when mouse dragging scrolled successfully. The callback now
moves the **nearest interactive Flickable viewport** by the normalized
pixel delta, directly updating its `contentY`/`contentX` position. It
walks the visual stacking order instead of treating an empty Qt Quick overlay
as the scroll destination, and constrains scrolls to the content of the top
open modal Popup/Drawer. Non-scrollable widgets still receive the regular Qt
wheel event. Non-canvas wheel and browser Ctrl+wheel zoom are left alone.
Native platforms do not install this callback.

Browser diagnostics: `window.__genycasterWheelDiagnostics` exposes
`received`, `onCanvas`, `forwarded`, and `directScrolled`.
After hovering over a long feed and turning the mouse wheel, all four
counters should increase. `received=0` means the Emscripten callback is
not receiving DOM wheel events; `onCanvas=0` means DOM canvas targeting is
failing; `directScrolled=0` despite non-zero onCanvas means the Quick
Flickable was not located or had no available scroll range. The Emscripten
API returns an explicit registration error logged to the Qt console.
Native Qt regression tests include a live ListView, a modal Popup ListView,
scroll bounds and disabled items; Node tests cover the browser coordinate
bridge. Real Qt 6.12 WASM mouse testing remains required before declaring
the browser issue resolved.

## Required CJK and Devanagari glyph coverage

WASM builds now enable pinned downloads and **require** the Noto Sans CJK
TTC and Devanagari font. A missing resource is a configure failure instead
of silently showing □ tofu for Japanese, Korean, Chinese or Hindi names
in a language selector. Offline builders may supply the two verified files
via `GENYCASTER_SCRIPT_FONT_DIR`. Qt 6.11 headless glyph checks confirmed
Japanese, Korean and Simplified Chinese glyph support in the bundled TTC.

## Emoji coverage in the browser

The WASM target embeds a compact, **monochrome emoji fallback** rather than
the 10.7 MB Noto Color Emoji bitmap font. The 846 KB static subset of Google's
OFL-licensed Noto Emoji includes 1,384 mapped code points (including 🧬,
❤️, 👍, 😀, 🔥, and 🚀). Its SHA-256 and a 1 MiB size limit are checked by
CMake. The SIL OFL license is embedded alongside the subset.

Qt 6.12's `BIG_RESOURCES` CMake mode compiles these font resources directly
to object files rather than expanding them into huge C++ hexadecimal arrays.
That eliminates the memory-intensive constant-data compilation step on small
Linux WASM builders without changing application Release optimization.

Qt WebAssembly does not necessarily inherit the emoji fonts installed in
Safari or other browsers; removing all emoji fonts could produce missing
glyph boxes. This lightweight fallback preserves common pictograms but is
**not full-color** and does not guarantee every newer or multi-code-point
emoji sequence. Existing animated-emoji resources remain independent.
Recheck glyph coverage in a real Qt 6.12 WASM browser before adding any
further emoji packs.

## Browser image delivery

The native image provider retains its direct HTTPS/CDN behavior. WASM uses
`https://uploads.example.com/v1/browser/images?url=...` for public
avatar and post-image bytes because cross-origin CDN XHR generally cannot be
read by Qt WebAssembly. The first-party Media Gateway returns explicit CORS
headers and permits only bounded raster responses (JPEG, PNG, GIF and WebP).
The gateway enforces an approved-host policy, HTTPS-only destinations,
public-DNS validation with pinned lookups, a maximum eight-megabyte response
and rate limiting. Redirects are revalidated, and credentials/cookies are
never forwarded. Private-address URLs are rejected; it is not an open proxy.
Cloud providers that are not on the approved list fail visibly via browser
console diagnostics instead of being accepted as arbitrary remote hosts.

A browser build alone cannot serve this URL; deploy the matching updated
Media Gateway before expecting remote images to render. The proxy route does
not enable browser account sign-in, write capabilities, or authenticated images.

## WASM linker heap sizing

Qt WebAssembly normally supplies a 50 MB initial linear-memory heap. The
current Genycaster application, with Qt Quick, the translations, and embedded
script/emoji fonts, requires 57,226,176 bytes (about 54.6 MiB) just to link.
The `farcaster_client` target therefore sets
`QT_WASM_INITIAL_MEMORY "128MB"` in `ui/CMakeLists.txt`. Set the **Qt
target property**, not another `-sINITIAL_MEMORY` flag: Qt applies its
linker defaults when finalizing the target and may overwrite an earlier flag.
128 MiB is a multiple of the 64 KiB WebAssembly page size; runtime heap
growth (`ALLOW_MEMORY_GROWTH`) remains independent from link-time sizing.
The larger initial heap does not mean that WebAssembly's maximum heap
or the downloaded `.wasm` file has grown to 128 MiB.

After pulling the change, **rerun CMake Configure**, then Build in Qt Creator.
On the final `em++` link command verify `-s INITIAL_MEMORY=128MB`
rather than `50MB`. The link-step failure is not caused by benign
C++ `-Wdollar-in-identifier-extension` or unused-capture warnings.

## Build and acceptance

1. Install the Qt 6.12 **WebAssembly** kit and its exact matching Emscripten
   SDK. Do not use desktop macOS SQLite or Homebrew dependencies.
2. In Qt Creator select the WebAssembly kit, use Ninja and a **new**
   `build/wasm` directory. Configure from the repository source root.
3. Confirm configure prints `Genycaster WebAssembly: browser sandbox composition`,
   retrieves SHA3-verified `sqlite-amalgamation-3540000`, and does not run
   `FindSQLite3` for the host platform.
4. Run the resulting loader over HTTPS (or localhost), not `file://`. Verify
   browser Console for WASM exceptions, CORS denial, loading, navigation,
   IndexedDB persistence across refresh, reduced-motion indicators and
   read-only guest feeds in Chrome/Firefox/Safari.
5. Verify `window.__genycasterSignerEnvironment` indicates a secure
   context, WebCrypto and IndexedDB. Browser writes require a custody-wallet
   approval and network-confirmed Snapchain signer; sign-in alone does not
   authorize publishing. Confirm the live gateway CORS preflight and then
   perform a supervised KEY_ADD/CAST integration test before production use.

Qt documents limitations of QtNetwork on WebAssembly (browser CORS, limited
sockets and no native SSL operations) and lists Qt Multimedia as Technology
Preview: https://doc.qt.io/qt-6.12/wasm.html

Never call this a production-ready cross-platform build based solely on
getting past the SQLite configure error. Browser smoke testing, end-to-end
sign-in and secure writing require additional implementation and test hardware.
