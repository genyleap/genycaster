# Feed Request Budgets And Idle Home

## Problem

A healthy network route and successful TLS handshake do not guarantee that a
provider feed can answer within the client's interaction budget. Hypersnap feed
surfaces are derived reads: Trending depends on metrics and full cast fetches,
and Following aggregates casts from the identity's social graph.

Genycaster previously made two poor assumptions for this workload:

- every `FeedQuery` requested 30 items by default;
- a fresh UI with no restored identity immediately requested Trending.

A slow provider could therefore consume the only live-read worker and delay
unrelated refreshes until its deadline expired.

## Decision

Feed reads now use these interaction constraints:

- the Application default feed page is 10 items;
- Following uses a 5 second total transport budget and derived feed/profile reads use 6 seconds;
- other Hypersnap reads retain their existing default request budget;
- a fresh UI with no restored identity starts in an idle, ready state and does
  not automatically request Trending;
- restored identities continue into Following after successful identity
  resolution;
- the idle Home surface lets the user explicitly open an identity or browse
  Trending;
- Qt Network timeout diagnostics record the last observable request stage so a
  timeout can be distinguished between connection setup, TLS, response-header
  wait, and response-body transfer.

The feed timeout is intentionally not extended merely because the endpoint is
heavy. Blocking provider ports remain an accepted adapter boundary, but ADR 0014
uses a bounded live-read pool so one hung request cannot head-of-line block every
unrelated refresh. Smaller pages and a finite total request budget still protect
interaction latency and bound stale work.

## Provider rationale

Hypersnap's current Trending implementation obtains ranked entries from its
metrics index and then fetches the complete cast for each selected result. Its
Following implementation gathers followed FIDs, fetches recent casts, and
merge-sorts the result. A default page of 10 matches the provider's documented
API default more closely and avoids multiplying backend work unnecessarily.

## UI states

Before any feed read has started:

```text
Ready
Home
Choose a personal Following feed or browse network-wide Trending
[Open identity / Open Following] [Browse Trending]
```

A timeout after a user-selected feed remains a source failure and does not erase
usable cached data. Debug diagnostics include elapsed milliseconds and the last
network stage observed by the Qt transport.

## Verification contract

Provider contract tests assert the feed-specific timeout budget. Core routing
unit tests assert that the default Application feed page reaches the provider as
10 items. Runtime acceptance on macOS must additionally verify:

1. fresh startup without an identity performs no automatic feed request;
2. restored identity startup opens Following;
3. explicit Trending uses `limit=10`;
4. a forced timeout reports its request stage;
5. switching away from a timed-out feed remains responsive after the finite
   provider call returns;
6. cached feed content remains visible through refresh timeout.
