# 05 — Local-First State and Synchronization

## Principle

The local canonical store is the application-facing state source. Providers are synchronization sources.

## Correct read path

```text
User gesture
    ↓
Application query
    ↓
Local canonical state
    ↓
Immediate UI snapshot
```

In parallel:

```text
Sync engine
    ↓
Capability router
    ↓
Provider
    ↓
Normalize
    ↓
Persist / reconcile
    ↓
Incremental state diff
    ↓
UI update
```

## Why this is required

A Telegram-class experience cannot depend on round-trip latency for ordinary navigation when data has already been seen.

The goals are:

- cached conversations/profiles/feeds open immediately;
- offline browsing remains useful;
- provider failover does not blank the UI;
- process restarts preserve durable optimistic state where required;
- synchronization is independent of rendering.

## Phase 4 executable path

The read path is implemented through `farcaster.application.read.service`. Cache
work remains on `farcaster.execution.serial`; independent live refresh work uses
the bounded `farcaster.execution.thread_pool` implementation behind the same
`farcaster.application.executor.IExecutor` port. The service publishes cached
state from `farcaster.local.read`, invokes `farcaster.routing.read.gateway`, and
then persists normalized provider results before publishing a fresh snapshot.

Public capability contracts remain blocking and unchanged. Qt observers are
delivered off the GUI thread and marshalled as small owned commits to the GUI
thread. ADR 0014 records the bounded-concurrency evolution from the Phase 4
single-lane baseline.

## Storage direction

Implemented local persistence:

- SQLite;
- WAL mode;
- prepared statement reuse;
- serialized cache/session executors plus a bounded live-read executor;
- incremental migrations;
- durable application-safe read continuations and social-list ordering.

FTS5 remains future derived search infrastructure rather than a Phase 4 core dependency.

## Initial conceptual tables

```text
users
casts
cast_text
cast_embeds
cast_reactions
social_edges
feeds
feed_entries
channels
conversations
messages
message_reactions
message_attachments
notifications
outbox
sync_state
media_cache
```

Not all tables are Phase 1 requirements; this list defines the long-term state shape.

## Normalized entity storage

Timelines should store stable IDs/references, not duplicated full objects.

Example:

```text
CastStore[CastId]
ProfileStore[Fid]
ChannelStore[ChannelId]

FeedTimeline = [CastId, CastId, CastId, ...]
```

## Sync responsibilities

The sync layer will eventually own:

- initial hydration;
- incremental refresh;
- cursor/anchor management;
- stale-state policy;
- retries/backoff;
- realtime events;
- sequence/gap detection;
- gap repair;
- reconciliation;
- durable outbox;
- optimistic mutation confirmation/failure.

## UI-thread rule

The UI thread may perform:

- input handling;
- lightweight presentation-state commit;
- layout/render scheduling.

It must not perform:

- HTTP/WebSocket I/O;
- SQLite I/O;
- JSON parsing;
- cryptographic operations;
- image/video decode;
- full-list diffing;
- persistence serialization.
