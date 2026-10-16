# WebAssembly composition (Qt 6.12 / Emscripten)

**Status: experimental read-first build; not a release-certified browser client.**
A configured WASM Qt kit is required to compile and actually test this path.
This repo's Linux native CI container does not contain the matching Emscripten
Qt kit, so a successful native test does NOT prove WASM execution.

## Dependency boundaries

The live Hypersnap reader parses JSON with **Boost.JSON**, compiled via
`boost/json/src.hpp` (header-only). Qt Creator's cross-build may reject
`find_package(Boost)` because BoostConfig is absent from the WASM sysroot.
`cmake/WasmBoostHeaders.cmake` supplies only an INTERFACE `Boost::headers`
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

## Browser OpenProof sign-in (read-only identity)

The browser uses OpenProof's public `genycaster-native-v1` OAuth device
grant through `https://openproof.genyleap.com`. The Qt Network adapter
starts the device challenge, polls the access token, verifies the canonical
subject via `/oauth/userinfo`, and requires exactly one server-confirmed
linked Farcaster FID from `/account/connections`. Since that account API
does not expose CORS directly, the Genycaster-owned HTTPS Media Gateway
forwards **only** `GET /account/connections` under
`/v1/browser/openproof/connections`, using the supplied Bearer token and
no cookies or configurable upstream. The response never includes the token.

The access token is kept transiently in Qt memory and zeroed after
verification; browser sessions are in-memory and expire, and are not saved
as durable client credentials. Identity login is not a Farcaster protocol
signer. Browser publishing, reactions, follows, signing and media uploads
remain disabled until a secure dedicated signer workflow exists. The
Genyleap account must already have exactly one linked Farcaster identity;
linking/registration is completed in Genyleap's owned account experience.

The browser cannot expose a native OS keychain or bind the app's localhost
custody-authorizer server. The wizard therefore has an explicit
**Continue as guest** path on WebAssembly, after reviewing Terms and Privacy;
native builds retain their mandatory authenticated setup. WASM builds therefore disable native secure signing,
Snapchain write service, OpenProof/SIWF blocking auth adapters, Mini Apps,
native media uploads and related unsupported capabilities instead of
falling back to insecure client-side secrets. Those are explicit feature gaps,
not successful web sign-in or write support. Read-only guest functionality
can use the QtNetwork asynchronous adapter, subject to CORS at each upstream
endpoint. A truly feature-complete browser version needs a distinct async
authentication bridge and credential-vault/signer flow.

Emscripten's IDBFS stores SQLite files below
`/home/web_user/.local/share` into IndexedDB. A preRun hook waits for
`FS.syncfs(true)` before the app opens the database, while an 8-second
Qt timer asynchronously triggers `FS.syncfs(false)` for persistence.
Browser private mode, quota exhaustion, and sudden tab termination may
still lose recent writes; **do not claim durable protocol writes in WASM**.
CORS and HTTPS requirements also apply to external Farcaster/Hypersnap nodes.

## Public Mini Apps in the browser

The single-threaded WASM Mini App catalogue issues bounded asynchronous
`QNetworkAccessManager` reads on Qt's GUI event loop, without a `QThread`.
The first-party Media Gateway exposes only fixed, rate-limited public Neynar
catalogue/search/relevant routes at `/v1/browser/miniapps`; the public
catalogue uses a bounded 20-item response to avoid CDN truncation (deeper
pagination is not yet implemented). User-controlled
upstream hosts, credentials and arbitrary paths are not accepted. The upstream
API key never enters WebAssembly. The browser can open a discovered public
Mini App in a separate HTTPS tab; this is **not** an embedded Farcaster Mini App
SDK host, and gives the app no signer, authentication tokens, wallet or
privileged host capabilities. Native macOS continues to use its existing
isolated WebKit Mini App runtime.

## Global browser mouse wheel dispatch

The Qt WebAssembly UI is rendered inside a canvas. The application registers
one non-passive capture-phase `wheel` listener scoped to the Qt canvas. It
normalizes DOM pixel/line/page deltas, prevents duplicate browser/Qt delivery,
and forwards the event as a standard `QWheelEvent` to the `QQuickWindow`.
Qt Quick then routes it to the item under the cursor (feed, ComboBox popup,
settings ScrollView, etc.). This does **not** use page-level CSS scrolling or
override individual Flickable `contentY` properties. Touch input and browser
Ctrl+wheel zoom are left to their existing handlers. The listener is not
installed for native builds. Browser-wheel and popup hit testing must be
smoke-tested on the actual Qt 6.12 WASM runtime before parity is claimed.

## Browser image delivery

The native image provider retains its direct HTTPS/CDN behavior. WASM uses
`https://storage-farcaster.genyleap.com/v1/browser/images?url=...` for public
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
5. Verify browser sign-in/write buttons are disabled until a browser-specific
   secure auth and write flow has actually been implemented.

Qt documents limitations of QtNetwork on WebAssembly (browser CORS, limited
sockets and no native SSL operations) and lists Qt Multimedia as Technology
Preview: https://doc.qt.io/qt-6.12/wasm.html

Never call this a production-ready cross-platform build based solely on
getting past the SQLite configure error. Browser smoke testing, end-to-end
sign-in and secure writing require additional implementation and test hardware.
