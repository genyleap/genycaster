# Phase 5B Verification — Cross-Platform Protocol Signer and Write Readiness

Date: 2026-08-28

## Result

**Source implementation accepted; platform-native production acceptance is
partially verified.**

The current Linux verification environment is GCC 14.2.0 and does not have
Qt6Keychain installed. It can therefore prove core protocol behavior,
fail-closed backend selection, architecture constraints, and syntax of the new
browser-wallet implementation, but it cannot truthfully claim native Windows,
Android, iOS, or current QtKeychain runtime integration.

Historical Apple Silicon verification for the direct Apple Keychain/OpenSSL
vault was completed in the earlier Phase 5B checkpoint with Homebrew Clang
22.1.8. That evidence is retained as historical Apple coverage; the complete
cross-platform shipping matrix must still run on release CI/runners.

## Commands and observed results

### Architecture

```text
python3 tools/check_architecture.py .
PASS
```

### Core protocol target

```text
cmake --preset core-debug
PASS

cmake --build build/core-debug --target farcaster_protocol_contract_tests -j2
PASS

ctest --test-dir build/core-debug -R '^protocol\.message\.contract$' --output-on-failure
PASS 1/1
```

The contract covers official BLAKE3 vectors including chunk/tree boundaries,
canonical KEY_ADD and KEY_REMOVE protobuf fixtures, outer Message sealing,
Farcaster epoch conversion, malformed authorization fields, and canonical
SignedKeyRequest/KeyAdd typed-data + ABI metadata fixtures.

A full `cmake --build build/core-debug -j2` on this GCC 14.2 host is not a
meaningful pass/fail signal for the project because GCC crashes while emitting
existing C++ module interfaces (`farcaster.application.read.service` and
`farcaster.application.session`) at `cp/module.cc:16297`. The protocol target
above builds before that compiler failure and remains the scoped acceptance
evidence for this continuation.

### Secure-signer composition configure

```text
cmake --preset secure-signer
PASS
```

Observed on this Linux host:

```text
Genycaster secure signer vault backend: unavailable
Qt6Keychain >= 0.17 was not found
```

This is the expected read-safe/write-closed behavior when
`FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=OFF`.

Protocol contract under the same composition:

```text
cmake --build build/secure-signer --target farcaster_protocol_contract_tests -j2
PASS

ctest --test-dir build/secure-signer -R '^protocol\.message\.contract$' --output-on-failure
PASS 1/1
```

A fresh protocol-only build with the project warning set promoted to `-Werror`
also passes:

```text
CMAKE_CXX_FLAGS=-Werror
cmake --build build/protocol-werror-final --target farcaster_protocol_contract_tests -j2
PASS
ctest ... protocol.message.contract
PASS 1/1
```

### Required secure-vault fail-closed policy

A clean configure with:

```text
FARCASTER_ENABLE_PROTOCOL_SIGNING=ON
FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT=ON
FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON
```

and no Qt6Keychain installation fails configuration as designed:

```text
FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON but no secure signer vault backend is available
EXPECTED FAIL-CLOSED: PASS
```

This proves a distribution job can make secure-storage availability mandatory.

## Browser-wallet authorization provider

The normal GCC 14.2 named-module target reaches a GCC compiler ICE while
emitting the provider interface CMI:

```text
internal compiler error: in write_location, at cp/module.cc:16297
```

The failure occurs at the `export module` declaration while importing the
existing capability graph and is the same GCC 14 C++ Modules family observed in
other project modules. It is not a project diagnostic.

To avoid treating the compiler crash as syntax evidence, the actual updated
`authorizer.cpp` implementation was also compiled against the real dependency
CMIs with a verification-only empty primary-module CMI and the project warning
set plus `-Werror`:

```text
browser authorizer implementation syntax: PASS
```

The previously executed supplementary loopback harness proved the actual
one-shot GET approval page, EIP-1193 payload exposure, JSON POST callback,
signature decoding, ABI metadata construction, malformed-response failure and
launcher failure behavior. This continuation additionally maps the explicit
`{"error":"rejected"}` callback to `Unauthorized`; the committed provider
contract now contains that case. The committed module contract should be rerun
on the non-GCC-14 release toolchain.

## Snapchain write provider

The source contract covers:

- binary `application/octet-stream` KEY_ADD submission;
- Ed25519 signing of the 20-byte protocol hash;
- signer/public-key consistency check before transport;
- post-submit signer re-read and exact TTL/scope/requestFid verification;
- submit failure mapping without false network success;
- self-revoking KEY_REMOVE and requester-nonce verification.

On this GCC 14 environment the real Snapchain provider interface remains
subject to the same known module CMI ICE. Earlier verification compiled its
implementation unit against the real dependency CMIs with `-Werror` after a
verification-only fake primary CMI.

## Qt UI availability in this verification container

`cmake --preset ui-debug` cannot configure here because Qt6 is not installed in
the container. This is an environment dependency failure at `find_package(Qt6
6.6 ...)`, not a project compile result. QML/UI acceptance must therefore be
rerun on the normal Qt-enabled platform runners.

## Cross-platform native acceptance matrix

| Target | Source path | Current evidence | Release acceptance still required |
| --- | --- | --- | --- |
| macOS | Security.framework Keychain | earlier Apple Silicon Clang 22 build | rerun current complete UI/signer flow |
| iOS | Security.framework Keychain | shared source boundary | iOS build + Keychain runtime + wallet transport |
| Windows | QtKeychain Credential Store | source/config boundary | MSVC/Clang build + credential runtime + wallet |
| Linux | QtKeychain Secret Service/KWallet | fail-closed selection verified here | Qt6Keychain-installed build + secret-service runtime |
| Android | QtKeychain Android Keystore | source/config boundary | Android Qt build + keystore runtime + wallet transport |

No row should be upgraded to "production verified" without the actual target
runner and real OS secure-store behavior.

## Required live acceptance scenario

For each shipping platform that enables interactions:

```text
Guest public/cached read
 -> Sign In With Farcaster
 -> secure Ed25519 key creation
 -> exact custody EIP-712 approval
 -> KEY_ADD accepted by current Snapchain
 -> re-read returns active exact registration
 -> writeReady = true
 -> application restart
 -> secure key reload + network re-check -> Ready
 -> self-revoke
 -> re-read confirms not active
 -> local secure key erased
```

Failure injection must also confirm that secure-store/wallet/Snapchain outages
never disable Guest/Public/Cached reads.

## Acceptance statement

Phase 5B is **implementation-final**. It is safe to begin Phase 5C development
behind the existing `writeReady` gate. Shipping interaction support on a given
OS remains conditional on that OS passing the live acceptance matrix above.
