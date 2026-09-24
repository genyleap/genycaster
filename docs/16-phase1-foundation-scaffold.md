# 16 — Phase 1 Foundation Scaffold

## Status

Phase 1 established the initial repository/CMake/module foundation. Phase 2 subsequently performed a compliance refactor against the pinned `ai-for-modern-cpp` contract; **current source structure and naming are authoritative over historical Phase 1 examples**.

## Phase 1 intent

The phase proved that a provider-free core could exist before any production backend adapter was introduced.

Foundation/domain/capability ownership created here remains valid, while several implementation details were corrected in Phase 2:

- the reference path is now explicitly C++26;
- module namespaces now mirror full dotted module identities;
- `farcaster.application.configuration` supersedes the earlier build-profile name;
- `ContinuationToken` is the application-safe pagination handle;
- empty feature interface libraries were removed because the upstream architecture contract forbids ceremonial layers;
- Qt shell CMake was upgraded to the pinned Qt Quick baseline.

## Phase 1 invariants preserved

- no provider implementation in the core graph;
- project-owned named C++ modules;
- provider-specific data never becomes canonical domain data;
- provider cursors never become application/domain pagination contracts;
- UI remains an adapter over application state;
- optional product concepts do not become mandatory core dependencies.

For current implementation details continue with `docs/18-phase2-routing-local-state.md` and `PHASE_2_REVIEW.md`.
