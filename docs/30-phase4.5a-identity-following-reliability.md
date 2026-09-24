# Phase 4.5A — Identity and Following Reliability

## Feature implementation gate

```text
Responsibility: Make a resolved/restored identity enter the personal Following feed deterministically, while keeping Trending an explicit Explore surface and preserving cache-first behavior.
Capability origin: Protocol (identity.resolve, profile.read) plus Derived/Protocol (feed.following.read) and Local (remembered presentation identity).
Mandatory or optional: Identity, profile, and Following are mandatory for the primary Home experience; Trending remains optional discovery.
Public contract: Existing provider-neutral Application read states and PresentationBridge intents/signals; no provider-specific public API is added.
Canonical data/invariants: A canonical Fid and Profile identify the active account; Home means feed.following.read for that Fid; provider DTOs/cursors remain outside Application and QML; cached data publishes before refresh.
Dependencies: QML → PresentationBridge → Application ReadService → local read store/routed gateway; QSettings remains composition-owned presentation preference state.
Expected failures: Invalid/unknown username, identity/profile/feed timeout, unavailable source, protocol mismatch, storage failure, missing route, and stale continuation.
Local-state representation: Canonical Profile/Fid and Following timeline remain in SQLite; only the last successfully usable username is retained in QSettings for startup restoration.
Offline behavior: A restored cached identity may open its cached Following timeline; refresh failure preserves usable rows and reports stale/source status without claiming device Offline unless OS reachability does.
Provider policy and disablement: Identity, profile, and Following route independently by capability. Trending failure or disablement cannot block Home.
Failover semantics: Each read resolves independently; application-safe continuations remain provider-affine and a route change cannot forward a foreign cursor.
Tests proving the boundary: Deterministic local HTTP runtime smoke covers Identity → Profile → Following, verifies limit=10 and no Trending request, then relaunches from an isolated persisted identity; existing restart/cache, provider contract, routing, QML lint, and UI smoke tests remain green.
```

## Product behavior

```text
Fresh launch
  → idle Home
  → open identity once
  → identity + profile resolve
  → Home / Following opens automatically

Later launch
  → restore remembered username
  → identity + profile refresh
  → Home / Following opens automatically

Explore
  → explicit Trending selection
  → failure remains isolated from Home
```

The visual system and responsive shell do not change in this iteration. The
navigation vocabulary changes so **Home** owns Following and **Explore** owns
Trending. Channel, profile, cast, and social-list behavior remains unchanged.
