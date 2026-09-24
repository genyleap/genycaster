# Phase 3 Review — Hypersnap Provider Adapter

## Status

Phase 3 is implemented and verified in the delivery environment.

This phase introduces the first production-oriented provider adapter without changing the provider-neutral application architecture established in Phases 1–2.

## Implemented

### Generic transport boundary

- `farcaster.transport.http` defines provider-neutral HTTP request/response contracts.
- `farcaster.transport.curl` implements the boundary with libcurl.
- Provider modules do not depend on QtNetwork.

### Hypersnap provider package

The provider is split into focused C++26 named modules:

- `farcaster.provider.hypersnap.config`
- `farcaster.provider.hypersnap.request`
- `farcaster.provider.hypersnap.codec`
- `farcaster.provider.hypersnap.continuation`
- `farcaster.provider.hypersnap.registration`
- `farcaster.provider.hypersnap.feed`
- `farcaster.provider.hypersnap.profile`
- `farcaster.provider.hypersnap.cast`
- `farcaster.provider.hypersnap.social`
- `farcaster.provider.hypersnap.identity`

The adapter targets the pinned Hypersnap v2 API contract inspected at commit:

`2eee4c9f2a7833ce7971dfef028480abbe9c4720`

Implemented read capabilities include:

- user/profile by FID;
- username-to-FID resolution;
- cast lookup by hash;
- following feed;
- trending feed;
- channel feed;
- followers;
- following.

No unsupported capability is advertised merely because a legacy or historical API mentioned it.

### Canonical normalization

Hypersnap response DTOs terminate inside `codec`.

Provider JSON is normalized into canonical domain values before crossing the adapter boundary. Canonical Cast embeds preserve both URL and cast-reference forms through `EmbedKind` + value rather than exposing provider DTOs.

### Provider-private continuation isolation

Raw Hypersnap cursors never leave the provider package.

`farcaster.provider.hypersnap.continuation` binds provider cursors to application-safe continuation tokens using the SQLite continuation store from Phase 2. Resolution enforces both capability affinity and provider affinity.

### Registration

The provider exposes a descriptor/capability registration surface so routing can treat Hypersnap as one provider implementation rather than a special-case dependency.

## macOS / Clang 22 build fix

The user-provided macOS build log exposed a concrete Apple SDK include-order failure. Storage compile commands contained both:

- `--sysroot=.../MacOSX.sdk`
- an explicit `-isystem .../MacOSX.sdk/usr/include`

With Homebrew LLVM/Clang 22, the explicit SDK include path can bypass the compiler resource-header ordering and produced Darwin typedef failures such as `__uint32_t`, `size_t`, and `ptrdiff_t` while compiling `entity_store.cppm`, `timeline_store.cppm`, and `sqlite_boundary.cpp`.

The fix is architectural rather than a header workaround:

- on Apple, storage locates and links the SQLite library directly with `find_library`;
- it does not consume `SQLite::SQLite3` include-directory propagation;
- Apple SDK headers remain supplied by `CMAKE_OSX_SYSROOT` only;
- `tools/check_macos_sdk_includes.py` is registered as a macOS regression test and rejects explicit `MacOSX.sdk/usr/include` injection for storage compile commands.

No Apple SDK header is modified and no compiler warning/error suppression is used.

## Verification

A clean provider build was configured and compiled from an empty `build/provider-hypersnap` directory with:

- CMake preset: `provider-hypersnap`
- compiler: GNU C++ 14.2.0
- C++ standard: C++26
- UI: disabled for the provider contract build
- tests: enabled

The clean build completed successfully.

CTest result:

```text
100% tests passed, 0 tests failed out of 4

architecture_guard                Passed
core.behavior                     Passed
storage.local                     Passed
provider.hypersnap.contract       Passed
```

The provider contract test validates:

- canonical user mapping;
- canonical cast mapping;
- URL and cast-reference embeds;
- RFC3339 timestamp normalization;
- malformed JSON handling;
- profile endpoint URL construction;
- username URL encoding;
- cast identifier URL encoding;
- HTTP status/error mapping;
- provider configuration validation;
- feed continuation isolation;
- capability/provider cursor affinity;
- social graph mapping;
- provider registration.

GCC 14 emits non-fatal module diagnostic notes/warnings while loading some libstdc++ template metadata. They do not originate from Hypersnap behavior and do not prevent compilation or test execution.

## macOS verification boundary

The concrete macOS failure reported by the user is fixed in CMake and protected by a compile-database regression guard. The delivery environment is Linux, so an actual Apple Clang 22 + Qt 6.11.1 binary cannot be executed here. The next macOS build should verify that storage compile commands no longer contain an explicit `-isystem .../MacOSX.sdk/usr/include` argument.

## Phase 3 acceptance

Phase 3 is accepted when:

1. the provider-neutral core still builds without Hypersnap enabled;
2. Hypersnap is opt-in via `FARCASTER_ENABLE_HYPERSNAP_PROVIDER`;
3. provider DTOs/cursors/URLs remain inside the adapter boundary;
4. the clean provider build succeeds;
5. architecture, core, storage, and provider contract tests all pass.

All five conditions are satisfied in this delivery.

## Post-Phase-3 macOS Clang 22 routing correction

The user's next real Qt/macOS build verified that the earlier SQLite/SDK include poisoning was no longer the active blocker: compilation advanced through storage and stopped in `routing/health.cpp`.

Homebrew Clang 22 + libc++ rejected `std::ranges::find_if` instantiations over the `CapabilityHealth` vector imported through a named module, reporting unavailable `std::__wrap_iter` comparisons. `health.cpp` also lacked a direct `<optional>` include for the `std::optional` return definition.

The routing implementation now avoids this fragile ranges/module interaction entirely. Health and provider registries use explicit indexed traversal, capability access checking is likewise explicit, and all implementation units include the standard-library facilities they use directly. This is a portability correction, not a provider or platform specialization.

The recovery path is covered by `core.behavior`: marking the primary provider unavailable selects the self-hosted fallback; replacing the same health record with `Healthy` makes the standard router select the primary again.
