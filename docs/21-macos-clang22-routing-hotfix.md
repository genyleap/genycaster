# macOS Clang 22 Routing Hotfix

## Context

A Qt 6.11.1 macOS build using Homebrew LLVM/Clang 22 progressed through the storage layer after the Phase 3 Apple SDK include-path fix, then failed in `farcaster.routing.health`.

## Failure signature

`std::ranges::find_if` instantiated by libc++ could not compare `std::__wrap_iter<CapabilityHealth*>` values while compiling a named-module implementation unit. The implementation also used `std::optional` and `std::nullopt` without including `<optional>` directly.

## Resolution

The routing layer now uses explicit indexed traversal for small routing registries:

- `HealthRegistry::update`
- `HealthRegistry::find`
- provider replacement/removal/lookup in `CapabilityRegistry`
- capability-access eligibility checks in `PolicyEngine`

This keeps the hot path simple, allocation behavior explicit, and avoids a compiler/library/module reachability edge case without introducing platform branches.

## Invariants preserved

- C++26 named modules remain enabled.
- `.cppm + .cpp` ownership is unchanged.
- no `import std` was introduced.
- no `-Wno-*` or compiler-specific source workaround was introduced.
- no provider-specific semantics crossed into routing.
- provider health remains per-capability.

## Regression coverage

`core.behavior` now verifies:

1. primary provider initially wins;
2. unavailable health causes failover to self-hosted;
3. updating the same provider/capability sample to healthy replaces the prior sample;
4. routing returns to the recovered primary;
5. strict-self-hosted policy still selects self-hosted.
