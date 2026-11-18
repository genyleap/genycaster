# GenyCaster durable engineering checkpoint

Last reconciled: **2026-11-18**, repository `genyleap/genycaster`, `main`.
Companion acceptance sources: [SPEC.md](SPEC.md), [PLAN.md](PLAN.md),
`docs/19-account-registration-test-plan.md` and GitHub issue comments.

## Verified baseline

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
  `27b285ac` push. Re-check HEAD/upstream before subsequent edits.

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
- Dependabot currently reports **three high-severity alerts**: Zod in
  `services/quick-auth/package-lock.json` and
  `ui/wasm/host-sdk/package-lock.json`, plus picomatch in the WASM host
  dependency chain. Direct lockfile inspection finds Zod 4.6.7 at the root
  but nested 3.x installations; picomatch 2.3.2 is transitive through
  WalletConnect/unstorage/anymatch. Published picomatch advisory currently
  lists no fixed version. No vulnerability is claimed closed.

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
  on `cc2b2f57`: **gate PASS**, WebAssembly job currently **in progress**
  (last inspected at host Qt ShaderTools installation). This is not a
  completed artifact, runtime smoke or version-tag release.
- No successful all-target CI, notarized/signed release or post-security-change
  installed-device tests are evidenced. Source-only checks and a successful
  gate do **not** authorize a version tag.

## Exact next executable tasks

1. Finish inspecting targeted WebAssembly CI [#44336446437](https://github.com/genyleap/genycaster/actions/runs/44336446437):
   gate has PASS, but Qt cross-build and artifact smoke are still running.
   Fix any actual failed job before accepting a Web artifact; do not rerun
   an already active workflow or equate dispatch/gate with success.
2. Refresh `origin/main` and Dependabot state; inspect nested vulnerable
   Zod and picomatch dependents. Prefer compatible upstream updates or verified
   removal; do not force an incompatible major override or claim an unpatched
   advisory is resolved.
3. Add focused deterministic tests for any dependency change; run WASM host
   bundle and Quick Auth tests. Commit/push only tested changes.
4. Continue real Wallet/CHAT/Android host implementation for #9 and verified
   OpenProof native-FID integration after reviewing the server trust contract.
5. Run the full approved matrix once feature
   parity is truly implemented. Keep #4/#5/#6/#9 open until complete.

No paid registration, irreversible on-chain write, production server mutation,
signing/notarization or public release has been performed by this checkpoint.
