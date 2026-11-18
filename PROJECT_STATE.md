# GenyCaster durable engineering checkpoint

Last reconciled: **2026-11-18**, repository `genyleap/genycaster`, `main`.
Companion acceptance sources: [SPEC.md](SPEC.md), [PLAN.md](PLAN.md),
`docs/19-account-registration-test-plan.md` and GitHub issue comments.

## Verified baseline

- Latest PWA wallet offline public-asset allowlist:
  `8ebce4d4b99cdd84dbc833a55205339f3ea7f627` (same-origin
  `genycaster-walletconnect.mjs`; wallet/session/secret routes excluded).
- Latest wallet silent-session race hardening:
  `aa6a1dc1282e6c84f9bf1562784f74b5a0fcec93` (reject delayed RPC
  results following topic, approved account, approved methods or chain change).
- Latest browser WalletConnect security and completeness fix:
  `8a8a18db09a231f8f870e0e446d2d5528458a4e3`.
  Replaced the unnecessary Reown AppKit/EthereumProvider dependency with
  pinned `@walletconnect/universal-provider@2.26.0`, an explicit fail-closed
  EIP-1193 event/chain adapter, the missing lazy SDK CMake bundle, and a
  missing-asset package fail gate. Native WalletConnect end-to-end remains
  unverified on a physical wallet.
- Latest native PoW/WASM boundary correction:
  `8e89d54c1c6229db066d07c0674097f9c19c8090`
  (guarded browser native registration preparation against OpenSSL imports).
- Latest browser-native custody ABI isolation:
  `e1e5c43b46c19a0a0df6f040e81f2c4cb47d3dfe`
  (WASM fail-closed non-signing controller; no OpenSSL/custody module import,
  browser-native QML ABI remains registered; strict release gate unchanged).
- Patched Quick Auth dependency chain:
  `9a06626fed542e00fa44ecba670138a80cda3f38` (explicit upstream
  `@farcaster/quick-auth@0.1.0`, Zod 4.6.7 and runtime test).
- Latest verified Home/Mobile UI regression remediation:
  `e89c6e1dec204ed5453423cc58e904a4ce6bd21d` (50/50 focused Node tests).
- Browser wallet async-consent security commit:
  `5c724735e588bda19a6b88b09d75422f3a427d6f`.
- Latest tested UI Neon gate remediation: `cc2b2f57381575d55b2f3462426ccff6593e0787`
  (Magazine-only GPU surface and quiet custom accent control contracts).
- Previous CI gate regression fix: `98934849b374b190bf6375871f1f049104a712bc`
  (one-line profile link checks exclude the separately wrapping tooltip).
- Latest verified mobile wallet link hardening commit:
  `27b285aca0129599475bd4f8599a67b5b9d10036`
  (reject bracketed IPv6, numeric/private authority, internal suffixes and
  trailing-root-dot DNS, retaining valid internationalized HTTPS DNS names).
- Root checkpoint documents committed as `fca31db4cf1a887de2d25aad29aee1c32d1fbf48`;
  strict root-file acceptance updated in `74bd5a18`.
- Earlier direct-main Release gate hardening:
  `aba3f1bf654b73747f51fea3286b0b9ebce741c7`.
- HyperSnap exact native uint64 FID/height merge:
  `35550e73ce9447e2650e99e4f7dab21346c053ce` (PR #39).
- Repository was clean and tracking `origin/main` immediately after the
  `8ebce4d4` push. Re-check HEAD/upstream before subsequent edits.

## Completed and locally verified in the latest execution

- Browser Mini App wallet uses an incrementing authorization epoch to reject
  delayed account-connect, privileged read and signature results after consent,
  chain or principal changes. Successful EIP-2255 revoke also invalidates the
  origin grant; failed revoke does not. Individual origin approvals remain required.
- `node --test tests/wasm/wallet_async_revocation_20261118.test.mjs`:
  **7/7 PASS** (including revoke, stale sign, regrant and initial account event).
- `node --test tests/wasm/miniapp_wallet_provider.test.mjs`: **30/30 PASS**.
  Re-verified after `27b285ac` with the new IPv6/internal authority,
  root-dot and IDNA handoff tests: combined browser wallet tests
  (`miniapp_wallet_provider.test.mjs` + `wallet_async_revocation_20261118.test.mjs`)
  **37/37 PASS**. The original unsafe IPv6/root-dot URLs were reproduced
  before patching (mobile handoff incorrectly returned two wallet links).
- Combined wallet origin/security tests:
  `node --test tests/wasm/wallet_origin_revocation_contract_20261118.test.mjs
  tests/wasm/wallet_canonical_origin_contract_20261118.test.mjs
  tests/wasm/wallet_async_revocation_20261118.test.mjs`: **13/13 PASS**.
- `python3 -m unittest ci/test_release_feature_contract.py
  ci/test_release_workflow_integrity.py`: **13/13 PASS**.
- `ci/check_architecture.py`: PASS;
  `ci/check_ui_design.py`: PASS (183 QML files);
  `ci/verify_ci.py`: PASS.
- The new JS wallet adversarial test is wired into
  `.github/workflows/ci.yml` and `ci/server_acceptance.py`.
- Root document allowlist now explicitly includes only the three expected
  checkpoint files; `ci/verify_ci.py` and 13/13 Release/workflow tests PASS
  after the `74bd5a18` correction.
- CI/profile test reproduction and repair: `ci.test_linux_distro_assets`
  originally failed due to checking a nested tooltip's `Text.WrapAnywhere`
  and obsolete GridLayout `columns` against the actual ColumnLayout. After
  the `98934849` correction, the **same 83/83 hosted preflight Python
  unit-test matrix passed locally**.
- A second hosted CI attempt exposed two obsolete Neon source assertions.
  The `cc2b2f57` correction passes the exact four-file hosted Neon
  regression command **23/23**, plus 11/11 focused Magazine/Settings
  tests, architecture, CI source policy, and 183-QML design audit.
- A broader independent Home/Mobile/Wallet/Android WebView test batch initially
  failed three obsolete Home/Card/Drawer source assertions (47/50). The
  `e89c6e1d` correction retains actual Home shortcut semantics and separate
  Search, ambient Neon activity cards, and Qt Drawer edge-consistent corner/
  inset geometry. The complete targeted suite then **50/50 PASS**.
  No real-device visual or security E2E implied.
- `e1e5c43b` isolates the browser-native custody compatibility ABI and
  refuses mnemonic creation/restore/export/signing. Static source boundaries,
  async wallet consent tests, all QML checks and release/CI guards passed:
  **40/40 Node**; **13/13 Python release/workflow**, UI design **183 QML**.
  No native OpenSSL methods or custody module are compiled in this WASM
  controller. WebAssembly compilation and artifact upload are now **CI
  verified** by run #44349790738 at `8e89d54c` (not a browser E2E).
- Quick Auth upstream override is independently npm-installed and tested:
  **7/7 PASS**, `npm audit --omit=dev` **0 findings** for that package.
  `.github/workflows/ci.yml` enforces the runtime test before Qt setup.
- Reown dependency chain to the vulnerable Zod 3 has been removed from the
  source lockfile by `8a8a18db`, leaving Zod **4.6.7 only** in the
  new WalletConnect universal-host dependency graph. The adapter source,
  mocked pairing, session reuse/abort, relay-independent EIP-1193 events
  and package completeness: **49/49 targeted Node tests PASS**.
  Independent WASM package/release fixtures **22/22 PASS**, Release/workflow
  unit tests **13/13 PASS**, CI and architecture guards PASS, and the design
  guard audited **183 QML**. Both first-party JS assets were actually built
  with esbuild (~503/515 KiB) in an isolated staged npm install. There is
  **no real-wallet/browser-installed E2E evidence**; the new hosted Web run
  is still pending.
- `aa6a1dc1` rejects stale relay RPC responses if WalletConnect loses its
  approved session, silently changes session topic, EVM account or selected
  network before a signature/read returns. New adversarial tests, existing
  consent suites and actual esbuild compilation passed:
  **52/52 wallet tests**, native-independent JS check PASS.
- `8ebce4d4` admits only the same-origin lazy public WalletConnect module
  in the PWA Service Worker allowlist, retaining the private URL exclusion:
  **26/26 WalletConnect/PWA asset tests PASS**, syntax/source guards PASS.
- Five additional source guards were verified on `8a8a18db`: async
  resources, data storage, media uploads, vNext release and deployment
  boundaries all PASS.
- **No native/browser installed-device wallet E2E** or new full hosted CI
  has been claimed for these commits.

## Open blockers

- GenyCaster [#4](https://github.com/genyleap/genycaster/issues/4):
  verified OPP native HyperSnap FID link remains absent; server dependency
  [OpenProof #52](https://github.com/genyleap/openproof/issues/52) open.
- GenyCaster [#5](https://github.com/genyleap/genycaster/issues/5):
  source architecture/UI guards green; strict full acceptance/native visual
  evidence remains incomplete.
- GenyCaster [#6](https://github.com/genyleap/genycaster/issues/6):
  funded registrations, all-platform installed runtime, OS signing and release
  acceptance unverified.
- GenyCaster [#9](https://github.com/genyleap/genycaster/issues/9):
  `ci/verify_release_feature_contract.py --strict` still blocks **CHAT,
  WALLET and privileged Android Mini App SDK host**. Do not bypass these.
- **Zero OPEN GitHub Dependabot alerts** verified on 2026-11-18 after
  `8a8a18db` via `gh api repos/genyleap/genycaster/dependabot/alerts`.
  Both Zod alerts [Quick Auth #1 and host SDK #2] are now **fixed** (host
  source lock has only Zod 4.6.7). The Picomatch advisory
  GHSA-fc5q-39xc-vjpf was withdrawn at 2026-11-18T15:38:56Z and
  disappeared from active GitHub findings. An earlier scratch *global*
  Zod-major override was rejected as incompatible and never shipped.
  npm advisory caches may still show withdrawn picomatch; GitHub has 0 open.

## Hosted CI evidence (not current-HEAD release qualification)

- Latest visible workflow run:
  [#43176994046](https://github.com/genyleap/genycaster/actions/runs/43176994046),
  **success**, 2026-11-12, head `5b37b436`, **macOS ARM64 only**.
  Linux, Windows, Android, Web, iOS and publish jobs were skipped.
- Targeted Web CI run [#44334629613](https://github.com/genyleap/genycaster/actions/runs/44334629613)
  on `27b285ac` **failed** in the gate at obsolete Python profile URL
  layout assertions. WebAssembly job was skipped.
- Targeted Web CI run [#44335501229](https://github.com/genyleap/genycaster/actions/runs/44335501229)
  on `98934849` **failed** at two stale Neon source assertions in
  `neon_video_surface_coverage.test.mjs`. WebAssembly job was skipped.
- Targeted Web CI run [#44336446437](https://github.com/genyleap/genycaster/actions/runs/44336446437)
  on `cc2b2f57`: **gate PASS**, **WASM build failed** scanning
  `ui/app/identity/native_wallet.cpp`: `openssl/crypto.h` missing.
  Root cause: OpenSSL-backed native custody source was added to the
  executable even though the WebAssembly protocol-signing module is absent.
- Targeted Web CI run [#44344815130](https://github.com/genyleap/genycaster/actions/runs/44344815130)
  on `e1e5c43b`: **gate PASS**, **WASM failed** at
  `native_prepare.cpp` importing missing OpenSSL-based
  `farcaster.platform.custody_recovery`.
- Targeted Web CI run [#44349790738](https://github.com/genyleap/genycaster/actions/runs/44349790738)
  on `8e89d54c`: **SUCCESS** including secure-deployment preflight,
  Qt Multimedia WebAssembly source build, full Qt/C++26 browser app compile,
  browser packaging and artifact upload. This establishes a build baseline,
  **not browser wallet runtime E2E**.
- Targeted Web CI run [#44354415384](https://github.com/genyleap/genycaster/actions/runs/44354415384)
  on `8a8a18db`: **in progress** at the last inspection, including the
  newly built lazy WalletConnect bundle and dependency fixes. The previous
  successful run predates these changes; recheck exact run/commit/asset.
- No successful all-target CI, notarized/signed release or post-security-change
  installed-device tests are evidenced. Source-only checks and a successful
  gate do **not** authorize a version tag.

## Exact next executable tasks

1. Finish inspecting targeted WebAssembly CI [#44354415384](https://github.com/genyleap/genycaster/actions/runs/44354415384)
   at exact `8a8a18db`: secure source gate PASS and browser C++/Qt build
   still running on last inspection. Require staging of both JS bundles,
   artifact hash/manifest completeness and success status before acceptance.
   It does **not** include later `aa6a1dc1` and `8ebce4d4` changes.
2. Security dependency graph is now clean (0 open). Verify WalletConnect
   consent using a *real* EIP-1193-compatible wallet on physical browsers.
   Do not confuse deterministic SDK integration mocks with network-device E2E.
3. After the active run is complete, run a targeted browser build of the
   current complete code SHA only if there are no remaining source defects.
   Inspect uploaded artifact, and then qualify native platform parity.
4. Continue real Wallet/CHAT/Android host implementation for #9 and verified
   OpenProof native-FID integration after reviewing the server trust contract.
5. Run the full approved matrix once feature
   parity is truly implemented. Keep #4/#5/#6/#9 open until complete.

No paid registration, irreversible on-chain write, production server mutation,
signing/notarization or public release has been performed by this checkpoint.
