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
- Regular sign-in is **viewing only unless a previously registered signer is
  independently restored and verified**. For missing signer permissions, a
  user-controlled "Enable publishing with recovery phrase" action remains
  available in Account after completing Setup. A successful sign-in itself
  must not open the recovery form, generate a key or submit KEY_ADD.

## Loading experience

- \`ui/wasm/genycaster-shell.html\` is installed as \`farcaster_client.html\`
  after the WASM binary links; it uses the supported \`qtLoad()\` API and
  includes \`farcaster_client.js\` and \`qtloader.js\`.
- \`genycaster-loader.js\` distinguishes the **first completed launch** from
  later launches using a non-secret localStorage marker. This is UX state, not
  proof of an installed/offline app, and it is never an authorization token.
- It shows a measured download percentage **only when the decoded response
  size is reliably known** via an unencoded Content-Length. Missing sizes and
  compressed transfer bodies use indeterminate loading. WebAssembly compilation,
  IDBFS restore, and Qt initialization have stage labels, not invented
  completion percentages. Startup failures show an actionable retry button.
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
- Use strong ETags and conditional revalidation for **stable filenames**,
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
- No service worker, offline installation, or custody phrase caching is
  installed by this change. The branded first-use screen is not an offline
  install promise.

## Regression commands

\`\`\`sh
node --test tests/wasm/*.test.mjs
python3 tools/check_ui_design.py .
python3 tools/check_localization_catalog.py .
\`\`\`

An actual Qt WebAssembly build plus tests on mobile browsers and a deployed
HTTPS origin are required before making production performance claims.
