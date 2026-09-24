# Phase 5A Verification — Guest-First Authentication and Client Session

## Result

**NOT VERIFIED** on the required final macOS/Qt matrix in this environment.

## Source-level acceptance covered

- Guest read path remains independent of compatibility authentication.
- Viewed identity and authenticated account are independent state.
- Write access requires session + signer readiness.
- Re-authentication preserves the existing verified session until replacement succeeds.
- SQLite session persistence excludes transient SIWF challenge material.
- SIWF proof validation covers domain, URI, nonce, statement, chain, FID resource, `expirationTime`, and `notBefore`.
- Signature verification uses a deployless universal verifier compatible with the auth-client 0.7.1 / viem 2.29.2 baseline (EOA, ERC-1271, ERC-6492).
- Auth-address authorization is checked before custody fallback.
- QML does not perform network, database, crypto, JSON parsing, or authentication polling transport.
- The approval URI is transient and not exposed as readable/editable QML text.

## Deterministic tests added/extended

- guest / authenticated-without-signer / write-ready gate behavior;
- expired persisted session -> Guest;
- successful session persistence only after verified completion;
- sign-out session clearing;
- re-authentication preserving an established session;
- compatibility-auth unavailable behavior;
- SQLite session round-trip;
- custody SIWF flow;
- active auth-address flow;
- ERC-6492 counterfactual signature flow;
- invalid universal signature rejection before FID authorization;
- wrong FID resource rejection before chain verification;
- expired SIWF proof rejection;
- future `notBefore` rejection.

## Environment constraints

The current environment does not contain the repository's final verification toolchain. GCC 14 is known to ICE while compiling this repository's C++ named modules; the available Clang installation lacks the module dependency scanner required by CMake; Qt 6 is unavailable. Therefore core/provider/UI results must not be promoted to VERIFIED here if they cannot execute to completion.

## Required final-device matrix

Run from a clean tree on macOS Apple Silicon using Homebrew Clang 22.1.8 and Qt 6.11.1:

```text
core-debug             configure + build + ctest
provider-hypersnap     configure + build + ctest
strict-self-hosted     configure + build + ctest
minimal-core           configure + build + ctest
provider-auth          compatibility auth enabled, UI disabled, build + provider auth contract test
ui-debug               configure + build + ctest
farcaster_client_qmllint_strict
warning-fatal UI smoke / interaction path
```

The final UI smoke must prove guest navigation works without a session and that write interactions remain gated until session + signer readiness.

## Evidence from this environment

Toolchain observed:

```text
CMake 3.31.6
Ninja 1.12.1
GCC 14.2.0
Clang 17.0.0 (clang-scan-deps unavailable)
Qt 6 unavailable
```

Executed evidence:

- `python3 tools/check_architecture.py .` — **PASS**.
- `cmake --preset core-debug` — **PASS configure**.
- `cmake --preset strict-self-hosted` — **PASS configure**.
- `cmake --preset minimal-core` — **PASS configure**.
- `cmake --preset provider-hypersnap` — **PASS configure**.
- `cmake --preset provider-auth` — **PASS configure**, including Boost/OpenSSL discovery and no Qt dependency.
- `ctest -R ^architecture_guard$` in all five configured trees above — **PASS**, 1/1 each.
- source audit for `channelToken` outside the compatibility adapter — **PASS**, no production leak found.
- source audit for SIWF token/relay/crypto material in QML — **PASS**, none found.
- `core-debug` build — **BLOCKED by toolchain**: GCC 14.2.0 ICE in `cp/module.cc:write_location` while compiling `farcaster.application.read.service`; this occurs before a complete product/test build.
- compatibility-auth contract-test target — **BLOCKED by the same GCC module ICE** while compiling `farcaster.provider.compatibility.auth` interface. The deterministic auth tests therefore exist in source but could not execute in this environment.
- Clang 17 clean configure — **PASS configure**; build — **BLOCKED** because CMake resolves `CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS-NOTFOUND`.
- `cmake --preset ui-debug` — **NOT AVAILABLE** because Qt 6 package configuration is not installed.
- strict `qmllint` and warning-fatal Qt smoke — **NOT VERIFIED** because the Qt toolchain is unavailable.

The first causal build failures are compiler/toolchain availability failures. They must not be reclassified as Phase 5A test PASS or product verification.
