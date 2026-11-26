# 00 — Architecture Overview

## Goal

Build a native Farcaster platform/client whose first network is Farcaster but whose architecture is not coupled to a managed backend.

The system is **not** “a Hypersnap client” and is **not** tied to any managed backend.

Hypersnap may be the preferred default provider for many protocol-native operations, but provider choice is a routing/deployment policy, never a domain assumption.

## High-level architecture

```text
                         QML / Qt Quick UI
                                │
                                ▼
                        Presentation Layer
                                │
                                ▼
                         Application Layer
                     commands / queries / use cases
                                │
                                ▼
                           Domain Layer
                                │
                 ┌──────────────┴──────────────┐
                 │                             │
                 ▼                             ▼
          Local State Core               Capability Facades
       SQLite / cache / timelines              │
                 ▲                             ▼
                 │                      Capability Router
                 │                             │
                 │                 ┌───────────┼────────────┐
                 │                 ▼           ▼            ▼
                 │             Hypersnap   Self-hosted   Compatibility
                 │              Adapter      Adapter       Adapters
                 │                                           │
                 └──────────── Sync Engine ───────────────────┘
```

## Identity and product-service boundaries

[OpenProof](https://github.com/genyleap/openproof), running as the separate
`opp` identity service at the configured `https://openproof.genyleap.com`
issuer, handles OpenProof-backed account authentication and linked Farcaster FID
verification. HyperSnap native custody identity remains a separate registry-backed
principal. The application authentication coordinator owns restoration and
revalidation policy, distinct from Qt lifecycle dispatch. Local identity seals,
protocol signers and private-cloud device keys are independent secure-vault
capabilities. The GenyCaster account API under `services/account-sync/` owns
its own PostgreSQL storage for private product data and enrolled public device keys.
See [security contract](08-security-auth-and-signing.md) and [ADR 0019](adr/0019-independent-authentication-authorities.md).
These are separate runtime services and databases, not dependencies of the
canonical domain model or a substitute for HyperSnap/Snapchain protocol
routes. See [OpenProof and Server Services](OPENPROOF_AND_SERVICES.md).

## Three kinds of decoupling

The project requires all three:

### 1. Provider decoupling

The core depends on capabilities, not vendors.

### 2. Feature decoupling

DMs, Channels, Mini Apps, Wallet, Search, Notifications, etc. are optional modules.

### 3. Derived-infrastructure ownership

Search, ranking, discovery, Mini App indexing, channel directories, trending, recommendations, and similar product intelligence must not silently become permanent managed-backend dependencies.

## Five capability classes

```text
Protocol       — canonical Farcaster primitives and protocol operations
Derived        — indexes/ranking/search/feeds/discovery computed from data
Compatibility  — legacy or flagship/backend-only functionality
Extension      — open optional functionality added by this client/ecosystem
Local          — device/client state requiring no remote provider
```

## Local-first principle

The user interface consumes local canonical state first.

Correct mental model:

```text
UI → local state → immediate rendering
                 ↘ sync engine → capability → provider
```

Incorrect mental model:

```text
UI → provider HTTP request → spinner → response → UI
```

The network updates local state; the local state drives the application.

## Provider policy

The default deployment may prefer Hypersnap for protocol-native functionality.

This means:

```text
Default policy = Hypersnap-first
Architecture   = provider-agnostic
```

A managed backend is never foundational. It may appear behind compatibility capabilities if a required feature currently exists only through that infrastructure.

## Product strategy

Do not clone every flagship backend feature as a core dependency.

Use this hierarchy:

```text
Protocol parity
   +
Compatibility where unavoidable
   +
Independent derived infrastructure
   +
New open product extensions
```
