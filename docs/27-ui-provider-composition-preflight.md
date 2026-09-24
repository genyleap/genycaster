# UI Provider Composition Preflight

Status: implemented corrective checkpoint after a real desktop run exposed a providerless graphical build.

## Observed failure

The running UI reported:

```text
Unavailable · No eligible provider route is available for the requested capability.
```

OS reachability was known and reachable. The request did not fail in HTTP or JSON; routing had no eligible source.

## Root cause

The top-level defaults were internally inconsistent for an ordinary IDE configuration:

```text
FARCASTER_BUILD_UI=ON
FARCASTER_ENABLE_HYPERSNAP_PROVIDER=OFF
```

Core presets intentionally disable the provider, but a fresh CLion/Xcode/default CMake configuration could therefore compile and launch the graphical executable with an empty runtime capability registry. The UI looked healthy while every remote read was structurally impossible.

## Corrected build contract

- When the graphical UI is enabled, the Hypersnap provider defaults ON because it is the only implemented live-read adapter in this checkpoint.
- Core-only presets continue to set UI/provider OFF explicitly and remain provider-free.
- Explicitly requesting a runnable UI with Hypersnap OFF now fails during CMake configure instead of creating a dead runtime.
- `FARCASTER_SELF_HOSTED_ONLY` plus the runnable UI is rejected until a real self-hosted read binding exists. The strict self-hosted core preset remains valid.

This is a composition-root rule, not a Domain/Application dependency on Hypersnap. When another live UI provider is implemented, the configure preflight should evolve from the current single-provider condition to a generic "at least one runnable read provider" condition.

## Runtime preflight

The composition root now resolves the mandatory `identity.fid.resolve` route immediately after constructing the router. If route composition is incomplete, runtime initialization fails before Home starts a read. The presentation therefore reports a setup/runtime issue rather than pretending a live source temporarily failed.

The router diagnostic also distinguishes:

```text
no provider registered for capability X

from

providers registered for X, but none eligible under policy/health
```

This keeps capability routing failures actionable without leaking provider-specific types into Application or QML.

## UI recovery semantics

A failed runtime preflight is not retryable from the current process because no provider binding exists to retry. The status strip therefore labels it `Setup issue`, diagnostics show `Read runtime: unavailable`, and the ordinary Retry control is hidden. Network/provider failures after a valid runtime remains retryable.

## Verification in the Linux container

Verified:

- provider-free core configuration succeeds with UI/provider OFF;
- a providerless UI configuration fails early with the new actionable CMake error;
- a fresh UI configuration records `FARCASTER_ENABLE_HYPERSNAP_PROVIDER=ON` before this container later stops at missing Qt;
- architecture guard passes;
- QML structural delimiter validation passes;
- the core build again reaches the pre-existing GCC 14.2 C++ modules ICE in `read_service.cppm`, after configuration and source scanning.

Not verified here:

- Qt compile / strict `qmllint`;
- macOS Clang 22 clean UI build;
- live Hypersnap read on the target Mac.

## Target Mac acceptance

Use a clean UI preset/build directory so an old CMake cache cannot preserve `FARCASTER_ENABLE_HYPERSNAP_PROVIDER=OFF`:

```text
cmake --preset ui-debug
cmake --build --preset ui-debug
ctest --preset ui-debug
```

Then launch the newly built executable and verify that Details no longer reports a missing eligible route. Any remaining failure should now identify the actual HTTP/transport/protocol layer.
