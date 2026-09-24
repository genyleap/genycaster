# Phase 4.5B Verification — Capability Health and Transport Ownership

## Outcome

**VERIFIED** for deterministic builds, routing behavior, provider contracts,
local-first runtime smoke, architecture boundaries, and strict QML lint.

Phase 4.5B provides independent runtime health/circuit state for each
provider/capability pair, bounded manual and automatic recovery probes, layered
diagnostics, and a Qt Network implementation owned by Transport rather than
Platform.

Live public-node availability and external Internet/TLS conditions are
environmental and were not used as acceptance evidence; the Qt executable's
network path is exercised against the deterministic local runtime fixture.

## Toolchain

```text
host:              macOS Apple Silicon
compiler:          Homebrew Clang 22.1.8
language:          C++26
generator:         Ninja 1.13.2
CMake:             4.4.2
Qt:                6.11.1
verification root: /tmp/genycaster-phase45b.aCwn2z
```

The Qt SDK path was supplied only to the clean configure command and was not
added to source-controlled CMake.

## Final clean matrix

Every profile was configured into a new directory, then built from source with
Homebrew Clang 22.1.8.

```text
core-debug          configure PASS · build PASS · 5/5 tests PASS
provider-hypersnap  configure PASS · build PASS · 6/6 tests PASS
strict-self-hosted  configure PASS · build PASS · 5/5 tests PASS
minimal-core        configure PASS · build PASS · 5/5 tests PASS
ui-debug            configure PASS · build PASS · 8/8 tests PASS
strict QML lint     PASS · --max-warnings 0
```

The matrix covers the macOS SDK include guard, architecture guard, core routing,
storage, local-first Application pipeline, Hypersnap provider contract,
warning-fatal UI smoke, and end-to-end UI runtime smoke.

## Circuit-breaker evidence

`core.behavior` verifies with an injected fake monotonic clock:

1. the first 12-second-equivalent Trending timeout records `Suspect/Closed`;
2. Following succeeds and records Healthy on its independent key;
3. the repeated Trending timeout records `Degraded/Open` with 30 seconds left;
4. normal Trending navigation fails fast without incrementing the source call
   count, while Following remains callable;
5. manual Retry is admitted during cooldown and a successful probe closes;
6. a later failed manual probe reopens the circuit and renews the full cooldown;
7. advancing fake time admits one automatic probe, which closes on success.

No test sleeps or reads wall-clock time.

## Diagnostic evidence

Core and provider contracts assert the layered fields independently:

- routing: capability, selected provider, measured latency, normalized result;
- open circuit: provider, `CircuitOpen`, and remaining cooldown;
- provider request: endpoint, limit, and deadline;
- Trending contract: `/v2/farcaster/feed/trending`, limit `10`, deadline
  `12000ms`;
- Qt transport: stage-aware timeout detail is compiled into and exercised by the
  UI-selected `farcaster.transport.qtnetwork` implementation.

## Transport boundary evidence

Source inspection and the architecture test confirm:

```text
src/transport/http.cppm              provider-neutral HTTP port
src/transport/curl.*                 provider/core concrete transport
src/transport/qt_network/*           Qt graphical-runtime concrete transport
src/platform/{android,ios,linux,...} OS application/platform adapters only
```

No active source or build reference remains to `src/platform/qt_network`, the
old platform target, or the old `farcaster.platform.qtnetwork.http` module;
their names appear only in this historical verification record. The
architecture guard explicitly rejects recreating the misplaced directory.

## UI runtime evidence

The clean `ui-debug` profile passed both warning-fatal smoke tests. The runtime
test still verifies isolated first-run and restored Identity → Profile →
Following flows against a local HTTP fixture and rejects any accidental Trending
request from Home. Signal/slot compilation and that runtime exercise also verify
the provider-neutral `ReadOptions` propagation through the Qt composition root.

No visual QML layout was changed in Phase 4.5B, so the previous Phase 4.5A
compact/standard/wide rendered inspection remains applicable.
