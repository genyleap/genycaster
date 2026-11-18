# GenyCaster execution backlog

Priorities and dependencies below are tracked by GitHub issues. Work is
accepted only with reproducible tests and, where appropriate, native device
evidence. Update [PROJECT_STATE.md](PROJECT_STATE.md) after each session.

| Priority | Work and dependency | Completion / acceptance evidence |
| --- | --- | --- |
| P0 | [GenyCaster #4](https://github.com/genyleap/genycaster/issues/4): canonical HyperSnap native FID link to OpenProof; depends on [OpenProof #52](https://github.com/genyleap/openproof/issues/52). | Server-verified single-use custody challenge, pinned shard-0 registry, nonce/subject uniqueness, canonical uint64 FID; coordinator checks authenticated returned tuple; adversarial and real-device E2E. |
| P0 | [GenyCaster #9](https://github.com/genyleap/genycaster/issues/9): implement actual CHAT and WALLET product composition, privileged origin-bound Android Mini App host, per-chain/account consent and full target parity. | Runtime adapters + adversarial tests, `verify_release_feature_contract.py --strict` PASS on same SHA, device matrix; never turn on a stub. |
| Completed (2026-11-18) | Close the three previous Dependabot High findings without incompatible overrides. | Quick Auth Zod fixed by `9a06626f`, host SDK Zod fixed by `8a8a18db` (removed Reown dependency), picomatch advisory withdrawn. GitHub API confirms **0 open alerts**; lockfile and 56/56 wallet tests PASS. Preserve this state in future upgrades. |
| P0 | [GenyCaster #6](https://github.com/genyleap/genycaster/issues/6): funded registration qualification, secure signing, packaging and public-release gate. Depends on #4/#9. | Operator-approved live test, all-platform CI and installed-device smoke, notarization/signing, performance/localization/security sign-off. |
| P1 | [GenyCaster #5](https://github.com/genyleap/genycaster/issues/5): source architecture/UI remediation now passes; finish native visual and strict same-SHA acceptance. Depends partly on #9. | Native Qt RTL/resize/Neon/media smoke, measured performance and full `server_acceptance.py --strict-design` PASS. |
| P1 | Extend wallet consent race tests to real browser hosts and native Android once implemented. | End-to-end reconnect, revoke, chain/account switch, stale callbacks, teardown/relaunch, browser and device runtime results. |
| P2 | Verify release workflow dispatches and supported ABIs without expensive unnecessary reruns. | WebAssembly CI [#44349790738](https://github.com/genyleap/genycaster/actions/runs/44349790738) succeeded with Qt/C++26 compile and artifact upload on `8e89d54c`. Recheck newer [#44354415384](https://github.com/genyleap/genycaster/actions/runs/44354415384) for the actual WalletConnect bundle; other target architectures, full Release matrix and installed runtimes still outstanding. |

## Execution rules

- Work on `main`, synchronize before changes, use scoped Conventional Commits,
  validate locally and push to `origin/main`; never force-push.
- Resolve correctness/security before visual polish; use existing Qt/QML tokens,
  C++ Core Guidelines and actual modular interfaces.
- Record test command, exit/result, commit and evidence; differentiate source tests,
  compilation, CI, installed runtime and production verification.
- Do not close issues for partial fixes, bypass the release gate, trigger paid
  registration or deploy external production contracts without explicit approval.
