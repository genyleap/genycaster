# GenyCaster durable engineering checkpoint

Last reconciled: **2026-11-18**, repository `genyleap/genycaster`, `main`.
Companion acceptance sources: [SPEC.md](SPEC.md), [PLAN.md](PLAN.md),
`docs/19-account-registration-test-plan.md` and GitHub issue comments.

## Verified baseline

- Last directly pushed wallet security commit:
  `5c724735e588bda19a6b88b09d75422f3a427d6f`
  (`security(wallet): invalidate browser Mini App RPCs after consent revocation (#9)`).
- Earlier direct-main Release gate hardening:
  `aba3f1bf654b73747f51fea3286b0b9ebce741c7`.
- HyperSnap exact native uint64 FID/height merge:
  `35550e73ce9447e2650e99e4f7dab21346c053ce` (PR #39).
- Repository was clean and tracking `origin/main` immediately after the
  `5c724735` push. Re-check HEAD/upstream before subsequent edits.

## Completed and locally verified in the latest execution

- Browser Mini App wallet uses an incrementing authorization epoch to reject
  delayed account-connect, privileged read and signature results after consent,
  chain or principal changes. Successful EIP-2255 revoke also invalidates the
  origin grant; failed revoke does not. Individual origin approvals remain required.
- `node --test tests/wasm/wallet_async_revocation_20261118.test.mjs`:
  **7/7 PASS** (including revoke, stale sign, regrant and initial account event).
- `node --test tests/wasm/miniapp_wallet_provider.test.mjs`: **30/30 PASS**.
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
- **No native/browser installed-device wallet E2E** or new full hosted CI
  has been claimed for this commit.

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
- No successful all-target CI, notarized/signed release or post-security-change
  installed-device tests are evidenced. The latest source-only checks do
  **not** authorize a version tag.

## Exact next executable tasks

1. Refresh `origin/main` and Dependabot state; inspect nested vulnerable
   Zod and picomatch dependents. Prefer compatible upstream updates or verified
   removal; do not force an incompatible major override or claim an unpatched
   advisory is resolved.
2. Add focused deterministic tests for any dependency change; run WASM host
   bundle and Quick Auth tests. Commit/push only tested changes.
3. Continue real Wallet/CHAT/Android host implementation for #9 and verified
   OpenProof native-FID integration after reviewing the server trust contract.
4. Run targeted hosted CI followed by the full approved matrix once feature
   parity is truly implemented. Keep #4/#5/#6/#9 open until complete.

No paid registration, irreversible on-chain write, production server mutation,
signing/notarization or public release has been performed by this checkpoint.
