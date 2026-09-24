# Profile Read Expansion and Stable Pagination

Status: source implemented; macOS Qt runtime acceptance pending.

## Responsibility

This checkpoint expands the existing provider-neutral read pipeline so Profile
is a real testable product surface instead of a profile-details card. It owns:

- a user's reverse-chronological Posts feed;
- a user's Replies feed;
- a user's Popular feed;
- pagination and refresh for each profile feed;
- navigation from feed/cast/social-list authors into profiles;
- return-path preservation between Profile, social lists and Cast detail;
- non-blocking degraded read presentation when saved data remains usable.

It does not introduce a second network stack or provider DTOs into Application
or QML. Full flagship-client parity such as notifications, search, bookmarks,
channel membership management and arbitrary conversation discovery remains
future capability work.

## Capability contract

The capability catalog adds three operation-level read capabilities:

| Capability | Read class | Current Hypersnap mapping |
|---|---|---|
| `feed.user.casts.read` | protocol read | `/v2/farcaster/feed/user/casts` |
| `feed.user.replies.read` | derived read | `/v2/farcaster/feed/user/replies_and_recasts` |
| `feed.user.popular.read` | derived read | `/v2/farcaster/feed/user/popular` |

Application sees only `FeedQuery` kinds and `IFeedSource`. Provider URLs and
response behavior remain inside the Hypersnap adapter.

Posts are the mandatory profile timeline for product acceptance. Replies and
Popular are useful derived surfaces but fail independently: a derived endpoint
failure must not make profile identity/details or cached Posts unusable.

## Local-first cache isolation

The three profile timelines have independent durable feed keys:

```text
feed:user:casts:<fid>
feed:user:replies:<fid>
feed:user:popular:<fid>
```

This prevents switching tabs from reinterpreting one provider result as
another. Cached rows render first; network refresh reconciles into only the
selected feed key.

## Pagination invariant and binding-loop fix

Pagination state is now split into two concepts:

- `hasMore`: whether an opaque continuation exists;
- `busy`: whether a read is currently executing.

`hasMore` does not depend on `busy`. QML therefore does not bind footer geometry
to a value that flips synchronously while processing `contentYChanged`.

The old failure shape was effectively:

```text
contentYChanged
  -> loadMoreRequested
  -> C++ busy = true
  -> canLoadMore = false
  -> footer height changes
  -> contentHeight/contentY changes
  -> contentYChanged re-enters the same binding graph
```

The corrected shape is:

```text
contentYChanged
  -> schedule only when near the bottom
  -> Qt.callLater(...)
  -> re-check !busy && hasMore
  -> loadMoreRequested
```

The pagination footer has constant geometry. A local `loadMorePending` guard
coalesces repeated scroll events until the deferred request is consumed. The
same rule is used by the main feed and profile feed surfaces.

## Profile interaction surface

Profile now exposes:

- identity header, avatar, username, FID and bio;
- Follow / Unfollow for another identity when write authority is ready;
- Followers and Following lists;
- Posts / Replies / Popular tabs;
- pull-to-refresh and continuation pagination;
- ordinary Cast interactions on profile rows: open conversation, reply,
  Like/Unlike, Recast/Undo Recast and owner-only deletion;
- author navigation from casts and social-list rows into that author's profile;
- deterministic Back behavior to the originating Profile/Feed surface.

The authenticated account's Profile entry opens the authenticated account
rather than merely reusing the currently browsed identity. Guest identity
browsing remains separate from authenticated account state. Profile targets and
Followers/Following targets are also stored separately from the Home viewing
identity; opening another author's profile must not change which FID owns Home.

## Network degradation policy

The latency-sensitive Following path keeps a 5-second deadline. Derived
Trending, channel and profile-feed reads use 6-second deadlines. The default
Trending UI page is six rows to reduce work on a public derived endpoint; normal
profile pages remain ten rows.

A capability timeout while the operating system still reports Reachable is not
classified as device-offline. If usable cached content exists, the status strip
shows a non-warning **Saved** state while retaining Retry and detailed
transport diagnostics. If no usable content exists, Source issue remains
visible.

OS reachability transitions still trigger recovery. In addition, the Qt
composition root performs a low-frequency 35-second automatic retry when a
capability is unavailable but the network remains reachable. This is longer
than the 30-second capability-health cooldown and therefore probes recovery
without continuously hammering a stalled derived endpoint.

## Architecture

```text
QML ProfileSurface / FeedSurface
        |
PresentationBridge
        |
Application ReadService + FeedQuery
        |
RoutedReadGateway + capability health
        |
IFeedSource
        |
Hypersnap adapter (current composition choice)
        |
Qt Network transport
```

SQLite remains the durable local projection. QML performs no HTTP, protocol
encoding, cursor interpretation or persistence.

## Verification in the current container

Verified:

- clean provider-enabled, UI-disabled CMake configure: PASS;
- architecture guard: PASS;
- modified Hypersnap feed interface/implementation/registration objects: BUILD
  PASS;
- modified Hypersnap contract-test translation unit: BUILD PASS;
- contract coverage includes all three user-feed URLs and the 6-second profile
  read budget;
- read-pipeline source includes a Posts/Replies/Popular cache-isolation
  regression test;
- no `canLoadMore` symbol remains in `src/`, `ui/` or `tests/`.

Not verified in this container:

- full read-pipeline link/run, because GCC 14.2 ICEs while serializing existing
  higher-level C++ module interfaces in `cp/module.cc:16297`;
- strict `qmllint`, because Qt6 is unavailable;
- warning-fatal Qt runtime creation and the exact reported binding-loop
  reproduction, because Qt6 is unavailable;
- macOS visual/interaction acceptance.

The shipping-environment gate must explicitly verify zero
`Binding loop detected` warnings while repeatedly scrolling/loading Home and
Profile tabs.
