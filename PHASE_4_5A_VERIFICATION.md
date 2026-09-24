# Phase 4.5A Verification — Identity and Following Reliability

## Outcome

Phase 4.5A is implemented. Home is now the identity-scoped Following feed and
Trending is an explicit Explore surface. A manually opened or remembered
identity continues through identity resolution and profile refresh into Home
without any dependency on Trending.

The implementation gate and boundary rationale are recorded in
`docs/30-phase4.5a-identity-following-reliability.md`.

## Baseline finding

The pre-change clean matrix passed for the four non-UI profiles. The UI profile
configured and built, but `ui.runtime_smoke` failed because the runtime test
read the developer's real `QSettings` identity while its local HTTP fixture
implemented only Trending. This exposed a genuine isolation defect in the
remembered-identity startup path.

The runtime now accepts an explicit `--settings-path` for deterministic tests.
Runtime smoke without that option cannot read or persist the user's application
settings.

## End-to-end runtime evidence

`tests/ui/runtime_smoke.py` now executes three isolated processes against a
deterministic local HTTP server:

1. Explicit Trending verifies the discovery route remains available.
2. `--auto-open-identity=alice` verifies the first Identity → Profile →
   Following transition and persistence to an isolated settings file.
3. A second process without an explicit username verifies remembered identity
   restoration reopens Following.

Both Home scenarios require exactly these capability requests:

```text
/v2/farcaster/user/by-username?username=alice
/v2/farcaster/user?fid=42
/v2/farcaster/feed/following?fid=42&limit=10
```

They also fail if a Trending request occurs. The second process reuses the
SQLite database, exercising the cache-first identity/profile/feed path before
the routed refresh reconciles the canonical state.

## Final clean build matrix

Every final profile was configured in a new directory under
`/tmp/genycaster-phase45a.DQG4Ks` with Homebrew Clang 22.1.8. The UI profile
used Qt 6.11.1 from the machine-local SDK path; no machine-specific path was
added to source-controlled CMake.

```text
core-debug          5/5 PASS
provider-hypersnap  6/6 PASS
strict-self-hosted  5/5 PASS
minimal-core        5/5 PASS
ui-debug            8/8 PASS
strict QML lint     PASS (0 warnings)
```

The matrix includes the architecture guard, macOS SDK include guard, core,
storage, local-first Application, Hypersnap contract, warning-fatal QML smoke,
and the expanded runtime smoke.

## UI and design verification

The real Qt executable rendered and was inspected at 390 × 844, 760 × 900, and
1280 × 900. Home, Explore, selected navigation state, bounded reading measure,
text wrapping, and compact/wide containment remained correct.

```text
premium UI strict audit: 0 findings
DESIGN.md lint:           0 errors, 15 orphan-token warnings, 1 token summary
```

The orphan-token warnings reflect the established DESIGN.md component-map
limitation. The tokens remain mapped to and consumed by `ui/theme/Theme.qml`;
no visual tokens changed in Phase 4.5A.

The macOS offscreen PNG capture emitted the previously documented external Qt
generic `Sans Serif` alias diagnostic. Warning-fatal `ui.smoke` remained clean.
