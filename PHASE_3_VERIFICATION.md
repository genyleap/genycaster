# Phase 3 Verification

## Verification environment

```text
OS/container: Linux
CMake:        3.31.6
Compiler:     GCC 14.2.0
Ninja:        1.12.1
Python:       3.13.5
SQLite:       3.46.1
libcurl:      8.10.1
C++ mode:     project C++26 baseline
```

## Clean provider build

The `build/provider-hypersnap` directory was deleted before the final provider acceptance build.

```text
cmake --preset provider-hypersnap
cmake --build --preset provider-hypersnap -j2
ctest --preset provider-hypersnap --output-on-failure
```

Result:

```text
architecture_guard                 PASS
core.behavior                      PASS
storage.local                      PASS
provider.hypersnap.contract        PASS

100% tests passed, 0 failed out of 4
```

## Provider-free regression matrix

After the final canonical-domain/schema changes:

```text
core-debug          3/3 PASS
strict-self-hosted  3/3 PASS
minimal-core        3/3 PASS
```

Each includes `architecture_guard`, `core.behavior`, and `storage.local`.

This verifies that the provider implementation did not become a core requirement.

## Storage verification

The local storage test verifies canonical profile/cast persistence, URL embeds, protocol `parent_url`, timelines, opaque continuation bindings, continuation erase behavior, and sync checkpoints.

Schema migration 3 is implemented using SQLite schema introspection plus conditional `ALTER TABLE`, so an existing Phase 2 `casts` table without `parent_url` can be upgraded rather than silently relying on a fresh `CREATE TABLE`.

## Contract verification

The Hypersnap contract executable uses a deterministic fake HTTP transport plus a real temporary SQLite continuation store. It verifies mapping, endpoint/query encoding, HTTP errors, continuation affinity, feed/social semantics, truthful capability registration, channel parent URLs, and final linkage of the libcurl transport target.

## Diagnostics

GCC 14 emits module diagnostics such as:

```text
note: unable to represent further imported source locations
warning: ignoring attributes applied to ... after definition
```

while loading standard-library/module metadata in the contract test. They do not fail compilation or tests. No global warning suppression was added.

## Apple/Clang 22 status

**Not final-device-verified in this delivery environment.** Xcode/macOS SDK and the user's Homebrew LLVM 22 installation are not available here.

What is verified structurally:

- the failure-inducing explicit Apple SDK `/usr/include` propagation was removed from the SQLite link path;
- Apple libcurl uses the same no-explicit-SDK-include strategy;
- `tools/check_macos_sdk_includes.py` rejects reintroduction of `MacOSX.sdk/usr/include` into storage compile commands;
- the guard was independently smoke-tested with one synthetic valid command and one synthetic poisoned command; the valid command passed and the poisoned command was rejected;
- the affected storage code and provider code pass clean builds on the available C++26/GCC 14 module toolchain.

The next macOS build should therefore be treated as the final Apple acceptance run, not as proof already obtained here.

## Qt UI status

Phase 3 is a provider/core phase. Provider-to-UI composition is a Phase 4 concern. This verification does not claim a complete Qt GUI acceptance run.

## macOS Clang 22 routing hotfix verification

A second user-provided macOS build log progressed beyond the Apple SDK / SQLite boundary and exposed a separate C++ named-module/libc++ issue in `src/routing/health.cpp`.

Observed Apple failure:

```text
std::ranges::find_if
→ std::__wrap_iter<CapabilityHealth*>
→ invalid operands to binary expression for iterator comparison
```

The same translation unit also relied on `std::optional`/`std::nullopt` without a direct implementation-unit include.

The routing implementation was hardened as follows:

- `health.cpp` now directly includes `<optional>` and uses explicit indexed traversal instead of `std::ranges::find_if`;
- `registry.cpp` was proactively converted away from `std::ranges::find_if` and iterator-based erase paths;
- `policy.cpp` no longer uses `std::ranges::contains` for capability access checks;
- no warning suppression, classic-header fallback, module disablement, or Apple-specific source branch was added;
- behavior remains provider-neutral and deterministic.

A regression was added to `core.behavior` proving that an existing provider/capability health sample is replaced on recovery and routing returns to the recovered primary provider.

Verification after the patch:

```text
core-debug
  architecture_guard  PASS
  core.behavior       PASS
  storage.local       PASS

provider-hypersnap-verify
  architecture_guard          PASS
  core.behavior               PASS
  storage.local               PASS
  provider.hypersnap.contract PASS
```

The delivery environment still cannot execute the actual Apple Clang 22 + Qt 6.11.1 build, so final macOS acceptance remains the user's next local build. The exact failing routing code path from the supplied log has been removed.
