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

## Initial public version scope (v1.0.0-rc1)

The initial release includes the social experience (feeds, profiles, casts,
channels, search, notifications), account sign-in and protocol-authenticated
publishing, and supported Mini App hosts. **Chat and the standalone Wallet
product are deliberately deferred**: every v1 platform explicitly compiles
`FARCASTER_ENABLE_CHAT=OFF` and `FARCASTER_ENABLE_WALLET=OFF`.
These flags do not disable protocol custody/recovery, account and signer
authorization, browser WalletConnect v2, injected EIP-1193 wallets, or
permission-scoped external wallet bridges used by supported Mini App hosts.

**Android v1 Mini Apps: public HTTPS pages only.** Android's isolated
QtWebView is not a Farcaster Mini App SDK host: it does not grant Mini Apps
sign-in, signer access, EIP-1193 RPC or transactions inside GenyCaster.
External wallet-browser handoff, if offered, is a separate external session
and must not be represented as an in-app wallet authorization. The Release
build explicitly uses `GENYCASTER_ANDROID_MINIAPP_SCOPE=public-only`;
CMake refuses a privileged Android SDK claim until an audited host exists.
Do not advertise fully functional *privileged* Android Mini Apps for v1.

The scope decision does **not** waive authentication/onboarding correctness,
OpenProof-native FID linking requirements, wallet-origin and chain consent
rules on supported hosts, OS signing, live verification or install testing.
The strict feature gate validates only what v1 explicitly advertises; it is
not a public-release approval on its own.

## Requested release delivery targets (2026-11-18)

- **macOS**: `x86_64` (Intel) and `arm64`, DMG plus signed update transport
- **Windows**: `x86_64` and `arm64`, NSIS installer plus update transport
- **Linux**: `x86_64` and `arm64`, AppImage, DEB and zsync metadata
- **Android**: `arm64-v8a` only, release-signed APK (not CI-test-signed)
- **WebAssembly**: site-only build for `https://app.genycaster.xyz`,
  **not** an installable GitHub Release download or a replacement for the
  `genycaster.xyz` company landing site.

All native artifacts must come from the **same tagged source revision** and
pass post-package and clean-install checks. Web hosting additionally
requires DNS/TLS, a dedicated app vhost, and an exact origin/host-protected
account API. Signing, notarization and live registration verification remain
mandatory and cannot be bypassed by a successful source-only CI gate.

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
