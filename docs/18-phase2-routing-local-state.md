# 18 — Phase 2 Routing, Local State, and Sync

## Purpose

Phase 2 proves that provider-neutral routing and local-first persistence can exist before any production provider is linked.

## Capability routing contract

A route request is operation-level, not provider-level:

```text
CapabilityRequest
    ↓
CapabilityRegistry
    ↓
PolicyEngine
    ↓
Health snapshot
    ↓
CapabilityRouter
    ↓
ProviderRoute
```

The router is synchronous and side-effect free on the request path. Network health probes are explicitly outside it.

## Capability quality

Endpoint existence is not sufficient evidence of native support. Implementations declare quality:

```text
Native
Derived
Emulated
Partial
Experimental
Unavailable
```

Policy can reject implementations below the requested minimum quality.

## Provider health

Health is tracked per provider/capability pair rather than globally. A provider may therefore remain eligible for one operation while degraded for another.

## Local persistence ports

Stable local-state ports are:

```text
farcaster.local.entity
farcaster.local.timeline
farcaster.sync.checkpoint
```

SQLite implements these ports at the adapter boundary. Application/domain code never imports SQLite.

## Provider-affine continuation

Public pagination uses:

```text
farcaster.capability.pagination.ContinuationToken
```

The token is application-owned and opaque. Infrastructure maps it to:

```text
CapabilityId
ProviderId
provider-private cursor
createdAt
```

A provider cursor is therefore never assumed portable across providers.

## SQLite ownership

The private `sqlite_boundary.hpp` exists solely because SQLite exposes a C API requiring textual type declarations. It is an explicit MOD-007/MOD-008 boundary and is not a precedent for project header architecture.

Database behavior:

- WAL enabled;
- foreign keys enabled;
- schema migrations recorded;
- entity writes transactional where multiple tables must remain consistent;
- blocking work belongs on a storage executor once the runtime execution model is introduced.

## Sync coordinator

The Phase 2 coordinator owns only authoritative state transitions:

```text
Idle → Running → Idle
             ↘ Failed
```

Every run receives a generation. Stale success/failure attempts are rejected. This prevents old asynchronous work from overwriting newer state when the later executor-based sync engine is added.

## Explicitly deferred

Phase 2 does not choose or implement:

- HTTP transport;
- JSON parser/provider DTOs;
- provider adapters;
- background executor implementation;
- realtime websocket ingestion;
- gap repair;
- durable write outbox;
- signing;
- feed ranking/search infrastructure.

Those remain later responsibilities and must not be smuggled into routing/storage merely because the infrastructure now exists.
