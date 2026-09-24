# ADR 0014 — Bound Live Read Concurrency And Preserve Serialized Cache Work

## Status

Accepted for the Hypersnap reliability hardening pass. This supersedes the
single-lane runtime composition in ADR 0008 without changing blocking
capability contracts.

## Context

ADR 0008 intentionally began with one serialized worker because early local
state and reconciliation code had not yet established concurrency safety. That
choice kept work off the GUI thread, but a slow derived feed could occupy the
only live-read lane until its provider deadline expired. Cancellation only
suppresses stale observer delivery, so the already-blocking call continued to
hold the lane and delayed unrelated profile, feed, search, or notification
refreshes.

The current SQLite read store and provider-continuation store serialize their
mutable connection/state access internally. Runtime capability health is also
made internally synchronized by this change. The transport creates
thread-local Qt network managers, so independent worker calls do not share a
`QNetworkAccessManager` across threads.

## Decision

- Keep blocking capability interfaces provider-neutral.
- Keep cache/session work on owned serialized executors.
- Execute independent live read refreshes on a fixed four-worker
  `farcaster.execution.thread_pool.ThreadPoolExecutor`.
- Keep the worker count bounded; do not create one thread per request.
- Make `HealthRegistry` internally synchronized so route health and circuit
  admission remain correct under concurrent reads.
- Drain cache work before draining the live-read pool during runtime shutdown,
  then join every worker while borrowed stores/providers are still alive.
- Keep observer/UI commits marshalled through the existing presentation
  boundary; no network or persistence work moves to the GUI thread.

## Consequences

- One stalled Hypersnap request no longer head-of-line blocks unrelated reads.
- At most four live read lifecycles can execute concurrently in the production
  composition root.
- Provider and storage adapters must remain safe for concurrent independent
  calls or synchronize their mutable internals.
- Cancellation still does not claim to interrupt a blocking third-party call;
  the bounded deadline and worker pool limit the impact of stale work.
- Future coroutine/native-async transports can replace the blocking adapter
  without changing Application or capability contracts.
