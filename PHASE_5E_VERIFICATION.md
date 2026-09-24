# Phase 5E Verification — Social Writes and App-FID Authorization

Date: 2026-08-28
Environment: Linux container, GCC 14.2.0, Ninja, SQLite 3.46.1

## Clean configure

PASS:

```text
cmake -S <source> -B <build> -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFARCASTER_BUILD_UI=OFF \
  -DFARCASTER_BUILD_TESTS=ON \
  -DFARCASTER_ENABLE_PROTOCOL_SIGNING=ON \
  -DFARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER=ON \
  -DFARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER=ON \
  -DFARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER=ON \
  -DFARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER=ON \
  -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=OFF
```

Expected environment warning: Qt6Keychain is unavailable in this Linux
container, so a persistent secure signer vault is fail-closed here. This does
not affect provider-neutral protocol/storage contract verification.

## Verified targets

PASS — architecture guard:

```text
architecture_guard
```

PASS — clean build:

```text
farcaster_protocol_write_contract_tests
farcaster_write_storage_tests
```

PASS — tests:

```text
storage.durable_write_outbox
protocol.write.contract
```

Result: 2/2 write tests passed; architecture guard passed separately.

These tests cover canonical Cast/Remove/Reaction/Link message preparation,
message-hash idempotency, social target persistence across SQLite reopen and the
shared durable outbox shape.

## GCC 14 module limitation

`farcaster_application_write` cannot be used as Linux/GCC14 final evidence in
this environment because GCC 14.2.0 ICEs while serializing the exported module
interface (`cp/module.cc:16297`, `write_location`). The same class of ICE is
reproducible on the pre-Phase-5E baseline application modules, so it is recorded
as a toolchain limitation rather than promoted to a Phase 5E source diagnostic.

The browser-wallet, App-FID attestation and Snapchain provider module targets
also hit the same GCC14 module-serialization failure before their implementation
objects can be used as acceptance evidence.

## UI status

NOT VERIFIED in this container — Qt6/qmllint are unavailable. Source-level Qt
wiring is present, but the required macOS Clang + Qt 6.11.x clean build, strict
qmllint, runtime smoke and live wallet/Snapchain acceptance remain release gates.

## Required release acceptance

On the normal macOS toolchain:

1. clean-configure `ui-debug` with Qt 6.11.x and Clang;
2. build the full default target and `farcaster_client_qmllint_strict`;
3. run all CTest tests;
4. sign in as a guest-to-user transition;
5. provision a production App-FID signer and verify one user custody `KEY_ADD`
   signature flow;
6. verify Cast, Reply, Delete, Like, Unlike, Recast, Undo Recast, Follow and
   Unfollow against authoritative network state;
7. restart the client and prove exact-envelope outbox recovery;
8. revoke/expire the signer and prove writes fail closed while reads continue.

## 2026-08-28 module-interface packaging fix

A macOS/Clang incremental build exposed a module implementation/interface skew at
`farcaster.domain.write`: the separate `write.cpp` implementation referenced the
expanded `OperationKind` enum while an older BMI could still be selected by an
existing build tree. The scope mapping is now defined directly in the exported
`write.cppm` interface and the separate `src/domain/write.cpp` implementation unit
has been removed from the target. This makes the enum and its mapping part of the
same module compilation unit and forces one authoritative BMI shape.

Fresh Linux GCC 14 verification after this structural fix:

- `farcaster_domain`: PASS
- `architecture_guard`: PASS
- `storage.durable_write_outbox`: PASS
- `protocol.write.contract`: PASS

Qt/macOS GUI runtime remains to be verified on the shipping macOS Qt 6.11 kit.

## Module-cache hardening (v3)

The App-FID SignedKeyRequest attestation contract now lives in the dedicated
`farcaster.capability.signer_attestation` module. The HTTP attestation provider
imports that module directly instead of relying on new declarations added to the
older `farcaster.capability.signer` BMI. This prevents incremental Clang builds
from observing an older signer BMI that lacks the attestation interface.

The delivery archive refreshes source timestamps so extracting it over an
existing workspace causes CMake/Ninja to rebuild affected module interfaces.
A clean build directory is still the preferred verification path after replacing
an entire source archive.

## Clang 22 direct-module import fix (v4)

A clean macOS Clang 22 build reached `src/providers/snapchain/write.cpp` and
correctly rejected a direct use of `domain::identity::Fid` that relied on an
indirect/transitive module dependency. The implementation unit now imports
`farcaster.domain.identity` explicitly.

The same direct-domain-use audit was applied to all project implementation units;
there are no remaining `domain::<module>::...` uses without the corresponding
`import farcaster.domain.<module>;` declaration. Files touched by the audit were
retimestamped so overlay extraction also invalidates affected Ninja module work.

This fixes the observed macOS diagnostic:

```text
error: declaration of 'Fid' must be imported from module
'farcaster.domain.identity' before it is required
```

Linux/GCC14 remains unsuitable as full Snapchain module evidence because its
known module serializer ICE can occur while compiling the provider interface;
this is independent of the Clang direct-import diagnostic above.

## Runtime custody verification hardening (v6)

Live macOS acceptance with FID `321969` exposed a stateful KEY_ADD failure: the
wallet completed EIP-712 approval and `/v1/submitMessage` accepted the envelope at
the submission boundary, but the signer never became observable before the
confirmation deadline. Current Snapchain merge validation recovers the KeyAdd
custody signature and requires it to equal the FID's current IdRegistry custody
address. Genycaster previously accepted whichever EIP-1193 account the browser
wallet returned, so an SIWF AuthAddress or another selected wallet could reach
submission and fail only during stateful merge.

The provisioning path now resolves `IdRegistry.custodyOf(fid)` during verified
SIWF completion, persists the normalized custody address in `ClientSession`,
passes it into `KeyAddRequest`, displays it on the one-shot approval page, and
checks the selected wallet both in JavaScript and again in native C++ before any
KEY_ADD metadata is accepted. Sessions created before this schema change fail
closed and instruct the user to sign out/sign in again rather than guessing a
custody wallet.

Two adjacent protocol mismatches were corrected during the same investigation:

- `/v1/signersByFid` requester nonce lookup uses the indexed sequence form
  `requesterFids[0]=<fid>` required by Snapchain's `serde_qs` HTTP parser;
- gasless signer `lastUsedAt` is converted from Farcaster-epoch seconds to the
  application's Unix wall-clock `Timestamp` before sliding-TTL evaluation.

Additional Linux/GCC14 evidence collected after these changes:

- `farcaster_auth_contract_tests`: PASS;
- `farcaster_snapchain_signer_contract_tests`: PASS;
- `farcaster_protocol_contract_tests`: PASS;
- `farcaster_storage_tests`: PASS.

The browser-wallet contract target remains **NOT VERIFIED** on GCC 14 because the
compiler ICEs while serializing the unchanged exported browser-wallet module
interface (`cp/module.cc:16297`). The shipping macOS Clang 22 + Qt 6.11 build is
the required acceptance environment for that adapter.

## Runtime endpoint correction — 2026-08-28

The shipping composition now targets the stable Genyleap Snapchain gateway at `https://snapchain.genyleap.com`. Signer/write adapters themselves have no vendor URL and no vendor-specific credential behavior; composition may override the origin and an optional generic HTTP authorization header/value for advanced or self-hosted deployments. Upstream provider credentials live only on the gateway server, and changing the gateway's upstream requires no desktop-client rebuild.

Linux/GCC 14.2 cannot complete the two Snapchain provider contract targets because GCC ICEs while serializing `farcaster.provider.snapchain.signer`; this is a compiler failure, not a test assertion or source diagnostic. macOS Clang + Qt remains the authoritative integration gate.

### v10 Snapchain requesterFids query fix

Snapchain HTTP GET query parsing uses `serde_qs`; `requesterFids` is a sequence and must be encoded using indexed array notation. Genycaster now emits `requesterFids[0]=<fid>` instead of the invalid scalar `requesterFids=<fid>`. The signer contract test pins this URL shape.

### v12 first-party deployment defaults

Shipping composition no longer depends on development command-line flags for SIWF identity. Build-configured defaults are `genycaster.genyleap.com`, `https://genycaster.genyleap.com/login`, and `https://snapchain.genyleap.com`. Runtime precedence is CLI > environment > build default. The three first-party values are exposed through `farcaster.application.configuration` and are configurable at CMake time; provider-specific upstream identity remains confined to the Genyleap gateway deployment.

In the Linux verification environment, `configuration.cppm` and the generated deployment implementation compile successfully. The aggregate `farcaster_core_tests` target is still blocked later by the known GCC 14 C++-module ICE while serializing the unrelated `farcaster.application.session` interface; no source diagnostic is emitted for this deployment-default change.
