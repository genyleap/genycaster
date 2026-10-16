# WebAssembly composition (Qt 6.12 / Emscripten)

**Status: experimental read-first build; not a release-certified browser client.**
A configured WASM Qt kit is required to compile and actually test this path.
This repo's Linux native CI container does not contain the matching Emscripten
Qt kit, so a successful native test does NOT prove WASM execution.

## Dependency boundaries

Do not resolve Homebrew/macOS SQLite for wasm32. `src/storage/CMakeLists.txt`
compiles the official SQLite 3.54.0 C amalgamation via the selected Emscripten
compiler (pinned SHA3-256), and links it to the existing SQLite boundary.
C language is enabled only on the WASM target. For reproducible offline builds,
seed CMake's FetchContent source directory with the same verified archive.

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
