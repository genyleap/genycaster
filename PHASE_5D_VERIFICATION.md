# Phase 5D Verification — Reply and Shared Composer

Date: 2026-08-28

## Result

**Source implementation accepted.** Native protocol/storage contracts and a
verification-only Application lifecycle harness pass. The current Linux GCC
14.2 environment still cannot emit the normal `farcaster.application.write` CMI
reliably, and Qt6 is not installed, so normal target-platform Qt acceptance is
still required before a release claims reply support on that OS.

No verification-only harness target or temporary CMake hook remains in the
deliverable source tree.

## Protocol references

Pinned Snapchain revision:

```text
8f82bcc68dd4e5c75b4a4814bf6fc6b54215a73c
```

Pinned official Farcaster client revision reviewed for shared-composer behavior:

```text
b6922e24036cac6f5e6d51904a59ff7cfcdd8483
```

Snapchain contract exercised by Phase 5D:

```text
MESSAGE_TYPE_CAST_ADD = 1
MessageData.cast_add_body = field 5
CastAddBody.parent_cast_id = field 3
CastId.fid = field 1
CastId.hash = field 2 (20 bytes)
```

## Clean configuration

```text
cmake --preset core-debug
PASS

cmake --preset secure-signer
PASS
```

The Linux container has no Qt6Keychain backend, so `secure-signer` reports the
secure vault as unavailable and correctly keeps signer persistence fail-closed.
This does not disable protocol/outbox verification or guest/read behavior.

## Native committed contracts

`core-debug`:

```text
cmake --build build/core-debug --target farcaster_protocol_contract_tests -j2
ctest --test-dir build/core-debug -R '^protocol\.message\.contract$' --output-on-failure
PASS
```

`secure-signer`:

```text
cmake --build build/secure-signer --target \
  farcaster_protocol_contract_tests \
  farcaster_protocol_write_contract_tests \
  farcaster_write_storage_tests -j2

ctest --test-dir build/secure-signer \
  -R '^(protocol\.message\.contract|protocol\.write\.contract|storage\.durable_write_outbox)$' \
  --output-on-failure
PASS 3/3
```

Phase 5D-specific executed coverage includes:

- Phase 5C root CAST_ADD protobuf fixture unchanged;
- canonical reply `parent_cast_id(fid, 20-byte hash)` protobuf fixture;
- parent FID zero rejected;
- malformed parent hash rejected;
- reply protocol identity remains hash == idempotency key == optimistic CastId;
- reply optimistic parent is preserved;
- SQLite close/reopen preserves reply parent;
- existing durable outbox/account isolation contracts continue to pass.

Pinned reply MessageData fixture:

```text
0801102a18f8c00720012a211a1808071214000102030405060708090a0b0c0d0e0f10111213220568656c6c6f
```

## Application lifecycle behavior

Normal GCC 14.2 compilation of the Application write target remains blocked by
the previously documented modules compiler ICE during CMI emission. The module
boundary was not weakened to accommodate that compiler bug.

For behavioral verification, a temporary ordinary translation unit was generated
in the excluded build tree from the current committed-equivalent:

- `src/application/write.cppm` declarations;
- `src/application/write.cpp` implementation;
- `tests/unit/write_service_tests.cpp` tests.

It imported and linked the real dependency modules and real protocol/SQLite write
libraries. The temporary CMake target was restored immediately after execution.

Observed result:

```text
Phase 5D WriteService lifecycle harness
BUILD PASS
RUN PASS
```

Executed reply-specific behavior includes:

- offline reply is durably enqueued with canonical parent;
- invalid parent fails before signing and before outbox insertion;
- restart resumes the stored reply envelope;
- resumed reply uses the exact previously signed message bytes;
- signer `signCalls` remains exactly one across restart/retry;
- account-switch ready-list processing never crosses FID boundaries.

The existing Phase 5C lifecycle tests in the same executable also re-prove
session blocking, transient retry, terminal rejection, attempt bounds and signer
unavailable/replacement behavior for the shared pipeline.

## Warning-fatal scoped libraries

A provider-free C++26 build with `CMAKE_CXX_FLAGS=-Werror` builds the final Phase
5D write libraries:

```text
farcaster_protocol
farcaster_protocol_write
farcaster_storage_write
PASS
```

No warning suppression or compiler-specific architecture workaround was added.

## Presentation/QML static acceptance

Qt6 is absent in this container, so native QML compilation cannot be claimed
here. Static source audit verifies:

- FeedSurface and DetailSurface expose/forward Reply intent;
- CastItem delegates Reply through the shared EngagementBar;
- AppShell opens the same composer for root and reply;
- QML contains no `authorFid`, protobuf, MessageData, message bytes, BLAKE3 or
  signature construction;
- QML stores only display parent context;
- `PresentationBridge::prepareReply()` resolves parent FID from current canonical
  C++ presentation maps;
- `publishReply()` performs runtime/auth/write-ready/busy/empty/320-byte
  preflight before emitting C++ intent;
- close/sign-out/account switch clears retained reply target;
- composition creates the same `PublishCastRequest` with optional
  `CastReference`, so provider/outbox paths are shared.

Normal Qt 6.11.1 + supported Clang runners must still compile/lint/execute these
QML changes before release acceptance on a platform.

## Architecture and hygiene

Required final checks:

```text
python3 tools/check_architecture.py .
PASS
```

No provider DTO or Snapchain HTTP detail entered Domain/Application. Parent wire
encoding remains Protocol-owned. FID stays in C++ for protocol construction and
no new secret material enters QML/SQLite.

Packaging excludes build directories and therefore excludes every generated
verification-only translation unit.

## Required platform release scenario

On every platform that advertises write interactions:

```text
Guest reads canonical cast
 -> Sign In
 -> signer Ready with CAST_ADD scope
 -> tap Reply on canonical cast
 -> shared composer shows parent context
 -> publish reply
 -> optimistic reply appears with final canonical hash identity
 -> terminate/restart after durable enqueue
 -> same reply parent survives
 -> castById reconciliation runs before any resubmit
 -> exact stored envelope submits only if absent
 -> authoritative reply confirms without duplicate optimistic entity
 -> switch account and prove old account reply never resumes/projects
```

Also inject offline, retryable failure, signer unavailable/revoked/replaced and
malformed-parent cases while proving guest/public/cached reads remain usable.
