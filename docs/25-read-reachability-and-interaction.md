# 25 — Read Reachability and Interactive Failure States

## Status

Implemented as a corrective iteration on the Phase 4 read lifecycle and the
production UI foundation. It does not change provider routing or introduce new
network capabilities.

## Problem

The previous presentation path treated several unrelated failures as if the
user's device were offline:

- provider HTTP 5xx / unavailable;
- request timeout;
- DNS, TLS, or other transport failure;
- actual OS-reported network disconnection.

`ReadService::failedMetadata()` also marked every failure stale even when no
usable cached snapshot existed. Together these rules could produce an
"offline/saved data" UI on a first-load provider failure with an empty cache.

## Correct state ownership

Application read metadata now owns only provider-neutral read state:

```text
ReadPhase
isStale              // true only when usable cached data is present
isSourceUnavailable  // timeout / transport / unavailable source class
Error                 // stable code + diagnostic context
```

Application no longer carries an `isOffline` bit. It has no authoritative
knowledge of host connectivity.

The Qt composition layer owns network reachability through
`QNetworkInformation`:

```text
Unknown       -> do not claim offline
Disconnected  -> isOffline = true
Local/Site/Online -> isOffline = false
```

Reachability remains presentation/lifecycle evidence; it is not used to skip a
real HTTP request. A transition from disconnected to a known reachable state
triggers one bounded retry of the current read intent.

## Failure matrix

| Condition | Cached snapshot | UI state | Data behavior |
| --- | --- | --- | --- |
| Provider succeeds | any | Live | canonical fresh data reconciles into SQLite |
| Provider fails | yes | Source/Data issue + Saved | cached snapshot remains rendered |
| Provider fails | no | Source/Data issue | empty/error state; never claims saved data |
| OS disconnected | yes | Offline | cached snapshot remains rendered |
| OS disconnected | no | Offline | explicit no-saved-data state |
| Reachability unknown | any | never Offline solely from OS signal | HTTP result decides read outcome |

## Diagnostics

Friendly product copy stays provider-neutral. Debug builds expose a collapsible
**Details** section in the status strip containing:

- OS reachability state;
- source-unavailable versus source-responded classification;
- structured error code and diagnostic message.

Release builds hide this diagnostic control. Runtime smoke failures also print
the diagnostic to the process log so CI failures identify HTTP/transport/codec
causes instead of only returning an exit code.

## Presentation data correction

The feed row mapping now preserves more canonical data instead of substituting
placeholder presentation values:

- `Cast::createdAt` -> relative timestamp;
- `Profile::avatarUrl` -> avatar source;
- parent cast / parent URL context -> reply context;
- URL embeds -> lightweight embed preview;
- canonical cast id remains the detail navigation key.

Provider JSON remains confined to the Hypersnap adapter.

## Provider contract verification

The current Hypersnap v2 API source defines feed responses as `casts` plus
`next`, and each cast contains the `author`, `text`, `timestamp`, `embeds`, and
parent fields consumed by the adapter. The local codec therefore matches the
current top-level response contract; runtime diagnostics remain necessary to
separate endpoint availability from future schema drift.

## Verification

Verified in the current Linux container:

- architecture guard passes;
- every QML source remains registered in the UI target;
- Application no longer owns an offline flag;
- stale failure state is now conditional on usable cached data;
- a regression test covers source failure with an empty cache;
- `read_model.cppm` compiles before the known GCC 14.2 C++ modules ICE occurs
  while compiling `read_service.cppm`.

Not verified in this container:

- Qt compile / `qmllint` / runtime smoke, because Qt 6 is not installed;
- live Hypersnap HTTP execution, because container DNS/network access is not
  available;
- target macOS rendered interaction.

Target acceptance should run `ui-debug`, strict `qmllint`, runtime smoke against
both a deterministic local HTTP fixture and the configured live source, then
exercise disconnect/reconnect while cached and uncached.
