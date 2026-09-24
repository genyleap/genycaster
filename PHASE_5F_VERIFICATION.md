# Phase 5F Verification — Write Hardening

Date: 2026-08-29

## Verification environment

Available runner:

- Linux;
- CMake + Ninja;
- GCC 14.2 for the primary configured build;
- Clang 17 available without `clang-scan-deps`;
- Qt 6.6+ not installed.

The repository's shipping gate remains macOS Clang 22 + Qt 6.11.x.

## Clean configure

Command:

```bash
cmake -S . -B build-phase5f -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFARCASTER_BUILD_UI=OFF \
  -DFARCASTER_BUILD_TESTS=ON \
  -DFARCASTER_ENABLE_HYPERSNAP_PROVIDER=OFF \
  -DFARCASTER_ENABLE_COMPATIBILITY_PROVIDERS=OFF \
  -DFARCASTER_ENABLE_PROTOCOL_SIGNING=ON \
  -DFARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT=OFF \
  -DFARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER=ON \
  -DFARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER=ON \
  -DFARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER=OFF \
  -DFARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER=OFF
```

Result: **PASS**.

## Source/architecture gate

Command:

```bash
python tools/check_architecture.py .
```

Result: **PASS** — `Architecture guard passed.`

## Durable storage tests

Commands:

```bash
cmake --build build-phase5f --target farcaster_write_storage_tests
./build-phase5f/tests/unit/farcaster_write_storage_tests
```

Result: **PASS**.

The test surface includes:

- v13 Cast-removal tombstone durability;
- v14 Like/Recast projection durability and backfill;
- v15 Follow/Unfollow projection durability and backfill;
- newer Unfollow intent resisting an old duplicate Follow idempotency key;
- close/reopen persistence;
- account/FID isolation.

## Canonical protocol-write contract

Commands:

```bash
cmake --build build-phase5f --target farcaster_protocol_write_contract_tests
./build-phase5f/tests/protocol/farcaster_protocol_write_contract_tests
```

Result: **PASS**.

## Higher-level behavior tests present in source

Phase 5F adds focused behavior coverage for:

- timeout after merge -> reconcile to Confirmed without a second submit;
- conflict + delayed indexing -> reconciliation-only retries with one total
  submit;
- concurrent resume wakeups -> one executor scan;
- signer terminal state -> future-backoff records fail without network access;
- signer replacement -> old-key queued write fails before network access;
- authoritative restore overriding an optimistic sliding-TTL checkpoint;
- `RemoveCast` NotFound -> Confirmed while `PublishCast` NotFound -> Absent.

### GCC 14.2 result

`farcaster_write_service_tests` cannot reach semantic implementation
compilation. GCC 14.2 ICEs while serializing `src/application/write.cppm`:

```text
internal compiler error: in write_location, at cp/module.cc:16297
```

The same compiler class of failure blocks signer and Snapchain write test
interfaces (`src/application/session.cppm` and
`src/providers/snapchain/write.cppm`). The ICE was reproduced in Debug and
Release configurations and is a toolchain failure, not a C++ diagnostic from
the Phase 5F implementation.

Result for those GCC targets: **NOT VERIFIED — COMPILER ICE**.

### Clang 17 fallback attempt

A Clang configure succeeds, but module compilation cannot start because the
runner does not provide `clang-scan-deps`:

```text
CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS-NOTFOUND
```

Result: **NOT VERIFIED — REQUIRED CLANG MODULE TOOL MISSING**.

## Qt product surface

Qt 6.6+ is not installed on this runner, so the following are not claimed:

- full Qt executable build/link;
- strict qmllint;
- runtime warning-fatal smoke flow;
- rendered interaction acceptance.

Result: **NOT VERIFIED — QT UNAVAILABLE**.

## Required macOS/live closure matrix

Before Phase 5F may be called release-complete, run on the shipping environment:

| Gate | Required result |
|---|---|
| Clean macOS Clang 22 + Qt 6.11.x configure/build | PASS |
| Full CTest suite | PASS |
| Strict qmllint | PASS, zero project warnings |
| Qt runtime smoke | PASS, zero project warnings |
| Deployed App-FID attestation | PASS; custody key stays server-side |
| Cast / Reply / Delete | PASS live |
| Like / Unlike | PASS live + refresh/restart state |
| Recast / Undo Recast | PASS live + refresh/restart state |
| Follow / Unfollow | PASS live + refresh/restart state |
| Restart with pending write | PASS |
| Offline -> online automatic recovery | PASS |
| Timeout after server merge | PASS, no duplicate submit |
| Conflict + delayed indexing | PASS, reconcile-only |
| Signer replacement | PASS, old envelope terminally rejected |
| Signer expiry/revocation | PASS, all pending records invalidated |
| Authoritative sliding TTL | PASS |

## Verification conclusion

Focused source/storage/protocol evidence is **PASS**. Phase 5F source hardening
is implemented, but final product/live acceptance is **PENDING** because the
required macOS Qt toolchain and live deployment environment are not available on
this runner. Phase 6 should wait for that closure evidence.

## Profile/read expansion evidence

A follow-up acceptance-support slice was added after the write-hardening gates
because live UI testing exposed a derived Trending timeout and a QML pagination
binding loop.

Verified in this container:

- provider-enabled, UI-disabled clean configure: **PASS**;
- architecture guard after the read/profile changes: **PASS**;
- changed `farcaster.provider.hypersnap.feed` interface and implementation
  objects: **BUILD PASS**;
- changed Hypersnap registration object: **BUILD PASS**;
- changed Hypersnap contract-test translation unit: **BUILD PASS**;
- provider contract source covers user Posts, Replies and Popular endpoint
  routing and the 6-second profile-read budget;
- read-pipeline source includes separate cache-key regression coverage for the
  three profile feed kinds;
- source scan contains no remaining `canLoadMore` reference in `src/`, `ui/` or
  `tests/`;
- Profile, social-list and Home viewing FIDs are stored independently in the
  presentation adapter, preventing author/profile browsing from silently
  retargeting the Home/Following identity;
- Cast detail now exposes author-to-Profile navigation and direct traversal to
  the parent cast when `parentCastId` is present, without inventing an
  undocumented child-replies provider endpoint.

Not verified here:

- full read-pipeline executable: **NOT VERIFIED — GCC 14.2 MODULE SERIALIZER
  ICE** in existing higher-level module interfaces;
- strict qmllint: **NOT VERIFIED — QT UNAVAILABLE**;
- the exact `Binding loop detected for property "canLoadMore"` runtime
  regression: **NOT VERIFIED — QT UNAVAILABLE**;
- rendered Profile Posts/Replies/Popular interaction matrix: **NOT VERIFIED —
  QT UNAVAILABLE**.

The macOS closure matrix therefore additionally requires zero project-owned Qt
warnings while repeatedly paginating Home and Profile, plus live verification
of Profile Posts/Replies/Popular, Followers/Following, author navigation, and
cached Saved-state recovery. See `docs/40-profile-read-expansion.md`.
