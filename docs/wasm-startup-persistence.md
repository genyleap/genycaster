# WebAssembly startup, caching, and persisted onboarding

## Observed behavior and corrections

- Onboarding completion and legal consent are **non-secret preferences**, not an
  authenticated session. Losing an ephemeral session or revoking a signer does
  not reset completed Setup. Users return to the app as guests until they
  authenticate; they must never regain write access from a saved Setup flag.
- Under Emscripten, QSettings now saves an INI file inside the mounted IDBFS
  directory \`/home/web_user/.local/share\`. The preRun restoration finishes
  before Qt main, while changes flush at setup completion and periodically.
  A private browsing session, unavailable IndexedDB, user data deletion, or
  a different origin can still reset preferences. These are browser limits.
- A cryptographically verified, device-local viewer identity can be resumed
  after refresh without persisting an OAuth token or recovery phrase. It is
  not proof that the provider session was freshly renewed. Explicit logout
  clears the device credential and disables further automatic restoration.
- Regular sign-in is **viewing only unless a previously registered signer is
  independently restored and verified**. For missing signer permissions, a
  user-controlled "Enable publishing with recovery phrase" action remains
  available in Account after completing Setup. A successful sign-in itself
  must not open the recovery form, generate a key or submit KEY_ADD.

## Loading experience

Qt 6.12 requires the application-specific Emscripten factory to be supplied
explicitly to `qtLoad`: `qt.entryFunction: window.farcaster_client_entry`.
Simply importing `farcaster_client.js` does not select the entry function.
The startup screen surfaces the underlying error if the factory is absent,
rather than assuming every failure is a network outage.

- \`ui/wasm/genycaster-shell.html\` is installed as \`farcaster_client.html\`
  after the WASM binary links; it uses the supported \`qtLoad()\` API and
  includes \`farcaster_client.js\` and \`qtloader.js\`.
- The boot loader uses a non-secret localStorage completion marker and release
  fingerprint to distinguish **first launch**, **normal refresh** and
  **new deployment**. These are UX hints, not proof of an installed PWA or
  authenticated account. First use/update displays Download → Install →
  Initialize milestones. Download starts with the actual WASM fetch;
  Install is shown only when browser Resource Timing reports the resource
  completed; Initialize follows Qt's real runtime-dependency callback.
  Ordinary refresh starts with Initialize. Failure never records completion.
  When Resource Timing is unavailable, the interface does not invent an
  install-completion event or fake download percentage.
- The loader preserves the native WASM Response without reconstructing a
  JavaScript ReadableStream. That permits native WebAssembly streaming and
  the browser's compiled-module cache to operate normally on refresh.
  Only actual resource-timing transfer bytes and Service Worker cache hits
  are used for cache diagnostics; no invented download percentages appear.
  Qt's onLoaded marks Ready. The boot icon uses the same original Geny SVG
  as the application's icon system, with a Powered by Genyleap footer.
  Startup failures show an actionable retry button.
- A \`visualViewport\`-aware host resizes the Qt screen when mobile browser
  chrome or the on-screen keyboard changes the actual visible viewport.

## Deployment contract

- Ship the generated \`farcaster_client.html\`, \`qtloader.js\`,
  \`farcaster_client.js\`, \`genycaster-loader.js\`, \`farcaster_client.wasm\`
  and the separately packaged browser recovery/crypto files **from the same
  build**. Do not deploy the new HTML without its matching binary and modules.
- Serve \`.wasm\` as \`application/wasm\`, preferably with Brotli or gzip
  compression on the CDN. Serve compressed content with correct
  \`Content-Encoding\` and \`Vary: Accept-Encoding\`. Transfer-size percentages
  cannot reliably be inferred from decompressed bytes, so the loader does not
  fabricate them.
- Use ETags and conditional revalidation for **stable filenames**,
  especially HTML and \`farcaster_client.js\`. Cache unchanging content
  immutably **only after introducing content-hashed filenames**; never mark
  a mutable \`farcaster_client.wasm\` URL as immutable across deployments.
- Provide COOP and COEP headers if the Qt WASM build needs threads and
  SharedArrayBuffer; test deployed HTTPS, CORS and all worker/module responses.
- Actual first-use and repeat load times depend on the WASM bundle size,
  compression ratio, bandwidth, CPU compilation speed, disk cache, and Qt
  startup. Measure production timings with browser Navigation/Resource Timing
  and server logs; do **not** promise a fixed startup time without those values.
  Repeat visits should benefit from HTTP cache/revalidation but can still
  recompile when the browser discards its compiled WASM cache.
- The public static shell now registers an optional, origin-scoped Service
  Worker and a Web App Manifest, and generates content-addressed WASM asset
  URLs. No identity API responses, bearer tokens, custody phrases or private
  signer material enter the cache. See [WASM static deployment](wasm-static-delivery.md)
  for exact cache headers, Nginx/Brotli setup and offline limitations.
  Availability of offline boot depends on the worker having cached all
  needed public files; do not promise offline social network access.

## Regression commands

\`\`\`sh
node --test tests/wasm/*.test.mjs
python3 tools/check_ui_design.py .
python3 tools/check_localization_catalog.py .
\`\`\`

An actual Qt WebAssembly build plus tests on mobile browsers and a deployed
HTTPS origin are required before making production performance claims.

## Mobile browser icons and Firefox runtime logs

Qt Creator's `emrun` server uses the contents of its build `bin/` folder.
Mobile browsers can request `/apple-touch-icon.png` and
`/apple-touch-icon-precomposed.png` even when the Web App Manifest has an
SVG icon. Both real 180×180 PNG files are shipped at the output root, and
the HTML has an explicit Apple Touch link. They are included in the static
asset manifest and optional public Service Worker cache.

macOS Firefox messages such as `sandbox_extension_issue_file_to_process`,
`Firefox GPU Helper ... Connection invalid`, or a GPU-helper process exiting
on signal 9 are **browser/OS diagnostics**, not evidence of a missing
Genycaster asset. Likewise the `GLD_TEXTURE_INDEX_2D` warning can indicate
a WebGL driver issue, but does not establish an application failure by
itself. Do not silence or reinterpret these warnings as successful startup.
If the UI fails to render, check Web Console/WebGL errors, Firefox
`about:support` graphics information, and compare the same release in a
second WebGL-capable browser. Device-specific reproduction is required to
decide whether a Qt rendering workaround is warranted.

## Social Workspace startup display

The brand name remains a single header. Warm navigation uses the
`Initializing Social Workspace` stage with two verified milestones:
loading the runtime and opening the social interface. On the first
successful launch or a new release, the timeline shows download, install
(runtime preparation), and initialization. Completion is driven by the
WebAssembly Resource Timing and Qt callback events, **never a fabricated
percent counter**. Actual Qt `onLoaded` is the only successful completion
criterion. A failed startup stays failed, even if a later asynchronous
event arrives.
