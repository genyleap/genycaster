# Phase 5C Verification — Publish Cast, Durable Outbox, and Reconciliation

Date: 2026-08-28

## Result

**Source implementation accepted. Normal GCC 14.2 module-target acceptance is
blocked by compiler ICE; scoped runtime behavior is independently verified.**

No verification-only harness, fake module, compiler workaround or temporary
CMake target remains in the deliverable tree.

## Environment

Current container:

```text
Linux
GCC 14.2.0
CMake 3.31.6
Qt6 not installed
Qt6Keychain not installed
```

The absence of Qt6 prevents native Qt UI compilation in this container. The
absence of Qt6Keychain keeps the Phase 5B secure signer vault fail-closed, as
designed.

## Clean configuration

```text
cmake --preset core-debug
PASS

cmake --preset secure-signer
PASS
```

`secure-signer` reports:

```text
Genycaster secure signer vault backend: unavailable
Qt6Keychain >= 0.17 was not found
```

This does not disable guest/read behavior or the protocol/outbox targets.

## Native committed contract targets

Built from the committed normal module targets under `secure-signer`:

```text
cmake --build build/secure-signer --target \
  farcaster_protocol_contract_tests \
  farcaster_protocol_write_contract_tests \
  farcaster_write_storage_tests -j2
PASS

ctest --test-dir build/secure-signer \
  -R '^(protocol\.message\.contract|protocol\.write\.contract|storage\.durable_write_outbox)$' \
  --output-on-failure
PASS 3/3
```

Coverage includes:

- existing canonical BLAKE3/protobuf envelope contract;
- PublishCast hash == idempotency key == optimistic CastId;
- canonical CAST_ADD preparation/sealing;
- 320-byte protocol rejection before signing;
- durable SQLite reopen;
- exact signed-envelope retention;
- optimistic projection preservation;
- deterministic idempotency dedup;
- FID/account-scoped ready-list isolation.

## Warning-fatal scoped build

A clean provider-free signing build with `CMAKE_CXX_FLAGS=-Werror` successfully
built the committed Phase 5C libraries:

```text
farcaster_protocol
farcaster_protocol_write
farcaster_storage_write
PASS
```

The existing `protocol.message.contract` executable also runs successfully in
that build. GCC 14.2 cannot compile the new test TUs themselves under global
`-Werror` because its modules implementation emits `-Wattributes` diagnostics
against imported libstdc++ entities such as `std::auto_ptr`, `std::out_ptr_t`
and `std::inout_ptr_t`. The same tests compile and pass under the normal project
warning policy. No source suppression was added for this compiler artifact.

## Application lifecycle behavior

The normal target:

```text
cmake --build build/secure-signer --target farcaster_write_service_tests -j2
```

reaches a GCC 14.2 compiler ICE while emitting the primary module interface:

```text
src/application/write.cppm: export module farcaster.application.write
internal compiler error: in write_location, at cp/module.cc:16297
```

The crash occurs at CMI emission, before the ordinary test translation unit can
compile. It is the same GCC 14 C++ Modules failure family already documented in
other project modules.

To distinguish compiler failure from source/runtime failure, verification then
generated a temporary **non-exporting ordinary translation unit** directly from
the committed:

- `src/application/write.cppm` declarations;
- `src/application/write.cpp` implementation body;
- `tests/unit/write_service_tests.cpp` test body.

It imported the real built dependency modules and linked the real protocol and
SQLite write targets. This is verification only; it was deleted immediately
after execution and never becomes product source.

Observed result:

```text
application WriteService lifecycle harness
BUILD PASS
RUN PASS
```

The executed tests cover:

- guest/no-session blocked before signing;
- expired session blocked before signing;
- offline publish durable + optimistic;
- normal submit + authoritative confirmation;
- restart reconciliation before any resubmit;
- transient failure -> bounded retry;
- protocol rejection -> terminal failure while retaining projection;
- maximum-attempt terminalization without another submit;
- FID/account-switch isolation;
- signer temporarily unavailable -> blocked without network I/O;
- signer replacement -> terminal authorization failure without network I/O.

During final verification this path exposed and fixed a duplicate local test
variable that GCC's earlier CMI crash had hidden. The final harness was rerun
successfully after that correction.

## Snapchain write provider behavior

The normal provider contract target similarly reaches GCC 14.2 CMI emission
failure:

```text
src/providers/snapchain/write.cppm: export module farcaster.provider.snapchain.write
internal compiler error: in write_location, at cp/module.cc:16297
```

A temporary non-exporting verification translation unit was generated directly
from the committed provider interface/implementation/contract test, linked to
the real transport/domain/capability/Boost.JSON targets, then deleted.

Observed result:

```text
Snapchain write provider harness
BUILD PASS
RUN PASS
```

Executed coverage:

- POST URL `/v1/submitMessage`;
- exact canonical binary body;
- `Content-Type: application/octet-stream`;
- configured timeout/body limit;
- `castById(fid, hash)` reconciliation URL;
- HTTP 404 -> authoritative Absent;
- successful response with mismatched hash -> ProtocolFailure;
- HTTP 409 -> Conflict without false success.

## Architecture guard

```text
python3 tools/check_architecture.py .
PASS
```

No provider wire detail was moved into Domain/Application and no new textual
project-owned C++ boundary was introduced for Phase 5C.

## Qt/QML acceptance in this container

Qt6 is not installed, so a native Qt UI build cannot be truthfully claimed from
this environment. Static composition source contains:

- `WriteService` runtime wiring;
- independent Snapchain write provider flag;
- root-cast composer trigger;
- 320 UTF-8 byte presentation preflight;
- write lifecycle state;
- optimistic row dedup/reconciliation;
- account-switch/late-callback isolation;
- periodic resume while current signer is Ready.

The supported Qt-enabled platform runners must compile and execute this normal
composition before a release claims the UI write flow on that OS.

## GCC 14 policy

No architectural simplification, header regression, fake production CMI or
committed compiler workaround was added to accommodate GCC 14.2.

Release acceptance should use the project's supported newer Clang/toolchains
and actual target platform runners. GCC 14 evidence is scoped to targets it can
compile reliably plus the explicitly temporary runtime verification described
above.

## Required release scenario

For each platform that enables interactions:

```text
Guest read
 -> Sign In
 -> verified Phase 5B signer Ready with CAST_ADD scope
 -> open composer
 -> publish root cast
 -> optimistic row appears with canonical cast hash
 -> process/network interruption after durable enqueue
 -> restart
 -> reconcile castById before resubmit
 -> submit only if absent
 -> authoritative confirmation
 -> feed refresh contains same CastId without duplicate optimistic row
```

Failure injection must additionally cover offline/rate-limit/revoke/replace
cases while proving guest/public/cached read behavior remains usable.

## Acceptance statement

Phase 5C is **implementation-final in source**. The next development phase may
extend composer/reply behavior over this durable write pipeline. Shipping on a
specific OS remains conditional on that platform's Phase 5B live authority
acceptance plus a normal Qt/module build and live Phase 5C submit/reconcile test.

## macOS Clang/libc++ include-visibility correction (2026-08-28)

A Qt 6.11.1 macOS Debug build with Homebrew LLVM 22.1.8 reached the Phase 5C
unit/provider tests and reported direct uses of `std::unexpected` without the
public `<expected>` header being textually included in those test translation
units. The production modules had already compiled past this point; the failure
was test-TU standard-library visibility under libc++/Clang modules.

Corrected source:

- `tests/unit/write_service_tests.cpp` now directly includes `<expected>` and
  the other standard headers it directly owns (`<cstddef>`, `<optional>`,
  `<system_error>`);
- `tests/providers/snapchain/write_contract_tests.cpp` now directly includes
  `<expected>`;
- a proactive audit found and corrected
  `tests/providers/hypersnap/contract_tests.cpp` to directly include
  `<optional>`.

Do **not** include libc++'s internal `__expected/unexpected.h`; `<expected>` is
the portable public header. A repository-wide test-TU audit for the relevant
standard symbols passes after this correction. The macOS full-build rerun is a
platform-runner acceptance step and is not claimed from the Linux verification
container.
