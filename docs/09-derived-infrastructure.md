# 09 — Derived Infrastructure Ownership

## Why this layer exists

Raw protocol access is not equivalent to a compelling product experience.

Historically, major client differentiation can live in backend-derived state rather than the protocol itself.

Examples:

- Mini App discovery;
- search;
- ranking;
- recommendations;
- trending;
- channel directories;
- notification aggregation.

A third-party client that ignores this layer becomes dependent on the flagship backend or is forced to repeatedly reconstruct indexes from raw network data.

## Strategic principle

> Strategically important derived capabilities should become independently owned/self-hostable infrastructure rather than permanent compatibility dependencies.

## General pipeline

```text
Protocol/network events
        ↓
Canonical ingestion
        ↓
Normalized event/entity stream
        ↓
Derived infrastructure
   ├── Search Index
   ├── Mini App Index
   ├── Channel Directory
   ├── Ranking Engine
   ├── Trending
   └── Recommendations
```

## Mini App discovery example

Do not force every client session to crawl arbitrary cast URLs.

Build a persistent incremental indexer:

```text
New Cast/Event
    ↓
URL extraction
    ↓
Known URL?
    ↓ no
Metadata/manifest probe
    ↓
Validation
    ↓
Mini App Index
    ↓
Search / categories / ranking / discovery
```

Mini App execution remains independent of this index. A direct URL may resolve/run without discovery infrastructure.

## Search

Long-term search architecture:

```text
network-wide canonical ingestion → server/self-hosted index
seen/downloaded data            → local SQLite FTS5
                                      │
                                      ▼
                              unified search facade
```

## Ranking

Personalized ranking should be an independent derived capability, not a provider-specific feed endpoint hardcoded into the app.

Potential inputs:

- graph relationships;
- engagement history;
- recency;
- channel affinity;
- local/user preferences;
- explicit filters;
- safety/moderation signals.

## Derived services and privacy

Routing/configuration must allow derived services to be:

- public managed;
- private managed;
- self-hosted;
- disabled.
