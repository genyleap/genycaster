# GenyCaster release specification

> Engineering acceptance index. Existing authoritative design documents are under
> [docs/](docs/); this file does not supersede their protocol or security contracts.

## Verified build baseline (18 November 2026)

- Product: Qt 6.12.x / Qt Quick, modern C++26 modules and CMake >= 3.30
  (`CMakeLists.txt`, `ui/CMakeLists.txt`, `docs/10-build-deployment-profiles.md`).
- Advertised desktop deployment targets: macOS, Linux and Windows, with
  architecture-specific workflows. Additional builds: Android (including ARM64),
  WebAssembly, and separately qualified unsigned iOS CI output.
- Application architecture: independent read, write, identity, signer and account
  authorities; provider-specific semantics belong in provider adapters.
  See `docs/00-architecture-overview.md`, `docs/01-non-negotiable-invariants.md`,
  `docs/06-feature-compartmentalization.md` and `docs/08-security-auth-and-signing.md`.

## Non-negotiable behavioral requirements

1. Protocol registration success is established by the appropriate authoritative
   registry, not a relay HTTP response. A native HyperSnap FID is **not** a
   verified OpenProof identity link merely because its custody wallet completed
   SIWE; see `docs/OPENPROOF_AND_SERVICES.md` and `docs/19-account-registration-test-plan.md`.
2. Signer authority, recovery material and Mini App wallet grants must remain
   isolated from untrusted origins, account/chain transitions and stale callbacks.
   No paid/on-chain registration retry follows an ambiguous write.
3. Release features must have real, tested runtime implementations across every
   advertised target. Flipping `FARCASTER_ENABLE_*` switches is not acceptance.
4. The UI must remain localized, accessible, responsive, RTL-correct and stable
   across async image loading, scrolling, navigation and theme changes.
5. Platform delivery must include required Qt plugins, host runtimes, security
   checks, update metadata, executable smoke tests and appropriate OS signing.

## Required public-release evidence

- Same-SHA PASS: `ci/check_architecture.py`, `ci/check_ui_design.py`,
  `ci/verify_ci.py`, `ci/verify_release_feature_contract.py --strict`,
  and `ci/server_acceptance.py --strict-design`.
- Actual Qt/C++ builds, hosted GitHub CI, artifact inspection, clean-install
  runtime smoke, device visual/RTL/performance checks and wallet consent testing
  on every advertised supported platform/architecture.
- Verified, operator-approved funded HyperSnap and Neynar registration and
  interruption/recovery, OPP FID connection proof, signer registration and
  account synchronization; do not execute irreversible operations by default.
- Signed/notarized installers and documented update/downgrade/revocation
  behaviors, localized content review and security acceptance.

**No version tag or public-release readiness claim is authorized until the
above evidence is complete.** The executable release matrix and implementation
details are documented in `docs/10-build-deployment-profiles.md`,
`docs/19-account-registration-test-plan.md`, `docs/12-roadmap.md`,
`docs/LOCAL_RELEASE_REPRODUCTION.md` and `docs/MACOS_RELEASE_SIGNING.md`.
