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
  When Resource Timing is unavailable, the first-run clone's completed
  byte stream can signal that a known-size uncompressed download has finished.
  Otherwise no fabricated installation milestone is shown.
- The loader preserves the native WASM Response without reconstructing a
  JavaScript ReadableStream. That permits native WebAssembly streaming and
  the browser's compiled-module cache to operate normally on refresh.
  First-use downloads can show exact byte percentages only when the
  response has a trustworthy uncompressed Content-Length. The response
  handed to Qt is still the original native stream; repeated refreshes do
  not track its bytes or clone it.
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
python3 ci/check_ui_design.py .
python3 ci/check_localization_catalog.py .
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
\n## Setup, account and responsive recovery (October 2026)

- Explicit **Log out** waits for a confirmed signed-out session before
  reopening Setup at its **Account** stage. This is intentionally different
  from a browser refresh or a network-disconnected viewer session. Existing
  legal consent, language and read-network settings are preserved. Only the
  non-sensitive `setup/showAfterSignOut` flag is persisted, so a refresh
  after an explicit logout still shows the account setup. Completing Setup
  clears the flag. Failed sign-outs never pretend the account is signed out.
- Every Setup stage uses a shared icon/title baseline. The Welcome language
  selector is now one compact, translated field instead of a wide stacked
  form. Footer controls preserve a bounded mobile layout.
- The Terms/Privacy reader is a shorter, article-like sheet and includes
  links to the **full published** Genyleap terms and privacy policy, plus a
  governing-note explanation. The in-app body is a summary, not a fabricated
  full legal document.
- The browser-only Farcaster phrase entry lives in a right-side desktop
  drawer and a bottom-up mobile sheet, with keyboard-safe scrolling and a
  sticky action row. No recovery words are written to logs, C++, localStorage,
  IndexedDB or browser transport. Validation still highlights invalid words
  while preserving entries when correction is needed.
- The floating mobile bottom navigation is lifted above the reading pane
  rather than participating in the content RowLayout. The reading workspace
  reserves enough bottom space for it. The dock retains a continuous rounded
  Superellipse surface.

## Mobile navigation, focus parity and stable boot markers

- The mobile BottomNavigation belongs physically to `readingPane`; it is
  anchored to that pane's bottom edge. Do not set `parent: root` on a
  control declared in a RowLayout: Qt Quick layout ownership and reparenting
  can leave the dock positioned over the mobile header. The navigation glass
  captures `readingContent`, not the dock's ancestor, to avoid recursion.
- Pointer-driven focus must not display a persistent white focus ring.
  `Theme.keyboardFocusVisible(control)` checks `focusReason` for Tab /
  Backtab and is shared by interactive controls (navigation, buttons,
  switches, sliders, menus). Keyboard focus remains visually discernible;
  typed-into text fields still show editing focus.
- Step indicators in the browser splash have fixed width/height and
  nonshrinking flex geometry. Current steps are blue and completed steps
  green; an animated progress bar must never affect marker dimensions or
  font-weight, including on a warm refresh.
- CMake forwards the project's semantic `PROJECT_VERSION` to the WASM
  packaging stage. `genycaster-assets.js` exposes public `appVersion` and
  a deterministic, eight-character `buildId` derived from the release's
  asset fingerprint. These appear below the brand and next to
  `Powered by Genyleap · WASM Engine`. This **is not** a CI build counter;
  the full fingerprint remains the release-cache identity.

## Verified download progress

On an initial visit or binary update, the loader may read a *clone* of the
same native WASM response **only** when a trustworthy, uncompressed
`Content-Length` is available. The original response is still returned to
Qt for streaming compilation, and no second network fetch is made. The
displayed percentage is `decoded bytes read / decoded Content-Length`, not
an estimated percentage for WASM compilation or workspace setup. A compressed
response or missing length gets an indeterminate loading animation instead of
an invented number. Warm refreshes do not clone the response. The
Download/Install/Initialize/Ready milestones remain tied to real browser
stream/resource events and Qt callbacks. Completed steps are green.

The boot footer shows `Powered by Genyleap · WASM Engine`.

## iOS browser appearance, Beta 1.0, and mobile Home layout

- The HTML defaults to a dark `#0b0c0f` background rather than a light page
  behind the Qt canvas. `viewport-fit=cover`, `theme-color`, and
  `apple-mobile-web-app-status-bar-style` provide mobile browser hints.
  The Web App Manifest uses matching dark theme/background colors.
- On WebAssembly, `AppearanceController::appearanceChanged` invokes
  `__genycasterSetBrowserChromeTheme()` through Emscripten. The HTML
  background, color scheme, and theme-color follow the effective Qt appearance.
- Browser limitation: OS and normal Safari/Brave toolbars are outside the
  webpage and may ignore theme hints depending on browser, iOS version,
  PWA versus ordinary tab, and origin. CSS cannot force native toolbar colors.
- Public product designation is **Genycaster Beta 1.0**; CMake semantic
  version is `1.0.0`, and the asset manifest also includes a hashed build ID.
  The HTML title, PWA name, splash, Qt header, Setup and desktop navigation
  display the release designation. The Qt label is translated for 12 locales.
- Compact Home presents a two-column four-tile grid: Explore, Share a cast,
  Apps and Search, using bare glyphs. Desktop retains its three-column
  shortcuts. Existing route signals and composer actions are unchanged.

## Safari chrome, Beta splash and bounded feeds (October 2026)

- The ordinary iOS Safari tab owns its top status bar and bottom URL toolbar.
  HTML theme-color and color-scheme are hints, not a guaranteed way to paint
  those native controls. Apple status-bar meta tags primarily apply to
  installed standalone web apps. For a chrome-free iPhone experience, test
  Add to Home Screen over HTTPS; local network HTTP may limit PWA features.
- The public Beta 1.0 brand uses a small superscript badge aligned to the
  Genycaster wordmark. The splash subtitle changes on real loader stages:
  Explore conversations / Connect with communities / Create and share on
  Farcaster. It does not synthesize download progress or use a timed fake step.
- Boot footer now has two centered lines: Powered by Genyleap · WASM Engine
  followed by the release designation and content-hashed Build ID. The
  redundant center build line is removed. The Qt canvas opts out of browser
  touch panning and the HTML document opts out of body overscroll.
- FeedSurface and ProfileSurface use StopAtBounds for drag behavior and
  movement. Their existing explicit refresh actions still work; large
  pull-to-refresh overscroll animations are intentionally retired.
- FeedSurface's no-results EmptyState is a ListView footer, below the inline
  feed header, not an overlay at a potentially negative viewport height.
  Its implicit height derives from its entire content and remains scrollable
  when the mobile header or translated messages are long.

## 2026-10-18: download progress, recovery diagnostics, simplified legal consent

- The WASM packager includes the decoded byte count `wasmBytes` of the
  immutable, content-hashed binary in both release manifest and JS config.
  An initial download measures actual bytes read from a cloned native
  Response against this total even with no Content-Length or a compressed
  transport. The original Response still streams unchanged into Qt;
  warm refreshes never clone it. Download percent is never a fake overall
  compilation or initialization percentage.
- Recovery checks local BIP39 words/checksum, Optimism on-chain account
  ownership, and Snapchain signer authorization separately. It provides
  stage-specific sanitized errors rather than labelling every failure as
  wrong words. Inputs stay available for retry and are cleared after
  success/cancel. Keys or phrases never enter Qt, logs, or HTTP payloads.
- Setup shows the translated Terms and Privacy summaries directly on the
  stage, with links to the official full policies and a single explicit
  acceptance checkbox. No modal document reader or scroll-to-end step.
  The original consent-version persistence and gated next-step behavior
  are preserved in native C++, across all twelve supported locales.

- The packager stamps all four public bootstrap JS script URLs in the release HTML
  with `?v=<release fingerprint>`. Cloudflare can retain bare `.js` responses
  for several hours despite origin `must-revalidate`; a versioned URL avoids
  stale asset configuration, loader and Qt startup scripts across deploys.
  The canonical HTML is hashed before stamping, while manifest integrity
  checks the final stamped bytes. Repeated packaging is idempotent.
