# GenyCaster execution backlog

Priorities and dependencies below are tracked by GitHub issues. Work is
accepted only with reproducible tests and, where appropriate, native device
evidence. Update [PROJECT_STATE.md](PROJECT_STATE.md) after each session.

| Priority | Work and dependency | Completion / acceptance evidence |
| --- | --- | --- |
| P0 | [GenyCaster #4](https://github.com/genyleap/genycaster/issues/4): canonical HyperSnap native FID link to OpenProof; depends on [OpenProof #52](https://github.com/genyleap/openproof/issues/52). | Server-verified single-use custody challenge, pinned shard-0 registry, nonce/subject uniqueness, canonical uint64 FID; coordinator checks authenticated returned tuple; adversarial and real-device E2E. |
| P0 | [GenyCaster #9](https://github.com/genyleap/genycaster/issues/9): implement actual CHAT and WALLET product composition, privileged origin-bound Android Mini App host, per-chain/account consent and full target parity. | Runtime adapters + adversarial tests, `verify_release_feature_contract.py --strict` PASS on same SHA, device matrix; never turn on a stub. |
| P0 | Investigate GitHub Dependabot alerts #1, #2 (Zod) and #3 (picomatch) in the Quick Auth and WASM host lockfiles. | Resolve with compatible patched upstream dependency or removal; re-run builds and wallet/auth tests. Picomatch advisory GHSA-fc5q-39xc-vjpf currently has no patched version; do not label it fixed by a version-only change. |
| P0 | [GenyCaster #6](https://github.com/genyleap/genycaster/issues/6): funded registration qualification, secure signing, packaging and public-release gate. Depends on #4/#9. | Operator-approved live test, all-platform CI and installed-device smoke, notarization/signing, performance/localization/security sign-off. |
| P1 | [GenyCaster #5](https://github.com/genyleap/genycaster/issues/5): source architecture/UI remediation now passes; finish native visual and strict same-SHA acceptance. Depends partly on #9. | Native Qt RTL/resize/Neon/media smoke, measured performance and full `server_acceptance.py --strict-design` PASS. |
| P1 | Extend wallet consent race tests to real browser hosts and native Android once implemented. | End-to-end reconnect, revoke, chain/account switch, stale callbacks, teardown/relaunch, browser and device runtime results. |
| P2 | Verify release workflow dispatches and supported ABIs without expensive unnecessary reruns. | Inspect latest run results, build/publish metadata, actual installers, reproducibility and complete target matrix. |

## Execution rules

- Work on `main`, synchronize before changes, use scoped Conventional Commits,
  validate locally and push to `origin/main`; never force-push.
- Resolve correctness/security before visual polish; use existing Qt/QML tokens,
  C++ Core Guidelines and actual modular interfaces.
- Record test command, exit/result, commit and evidence; differentiate source tests,
  compilation, CI, installed runtime and production verification.
- Do not close issues for partial fixes, bypass the release gate, trigger paid
  registration or deploy external production contracts without explicit approval.
