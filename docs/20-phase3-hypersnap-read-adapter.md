# 20 — Phase 3 Hypersnap Read Adapter

## Scope

Phase 3 adds the first production-shaped read provider behind the provider-neutral capability contracts. Hypersnap is an adapter, not an application dependency.

Reference contract inspected for this phase:

```text
repository: https://github.com/farcasterorg/hypersnap
revision:   2eee4c9f2a7833ce7971dfef028480abbe9c4720
```

The implementation is based on the public v2 HTTP response shapes and endpoint smoke tests at that revision. No Hypersnap source code is copied into this repository.

## Implemented capability ports

The core exports operation-level interfaces for:

- identity resolution;
- profile reads;
- cast reads;
- followers/following reads;
- following feed reads;
- trending feed reads;
- channel feed reads.

The Hypersnap registration advertises only operations implemented and supported by the inspected contract. In particular, `feedForYouRead()` is not advertised by this adapter.

## Provider isolation

The dependency flow is:

```text
application/routing
      ↓
capability interfaces + canonical domain
      ↑
Hypersnap adapter
      ↓
provider-private request/codec/continuation
      ↓
provider-neutral HTTP transport
```

Hypersnap JSON field names, URLs, HTTP status mapping, raw pagination cursors, Boost.JSON, and endpoint composition terminate inside `src/providers/hypersnap/`.

## Modules

The provider is deliberately split into responsibility-focused named modules:

```text
farcaster.provider.hypersnap.config
farcaster.provider.hypersnap.request
farcaster.provider.hypersnap.codec
farcaster.provider.hypersnap.continuation
farcaster.provider.hypersnap.registration
farcaster.provider.hypersnap.feed
farcaster.provider.hypersnap.profile
farcaster.provider.hypersnap.cast
farcaster.provider.hypersnap.social
farcaster.provider.hypersnap.identity
```

This avoids a monolithic provider client and keeps compiler/module dependency fan-in bounded.

## Canonical mapping

Provider DTOs never escape the adapter. The codec maps v2 JSON into canonical entities:

- `User` → `domain::profile::Profile`;
- `Cast` → `domain::cast::Cast`;
- cast URL embeds → canonical `EmbedKind::Url`;
- cast references → canonical `EmbedKind::CastReference`;
- `parent_hash` → canonical parent cast;
- `parent_url` → canonical protocol parent URL;
- RFC3339 timestamps → `foundation::time::Timestamp`.

The mapper rejects malformed JSON and an invalid cast carrying both `parent_hash` and `parent_url`.

## Pagination isolation

Raw provider cursors never enter application contracts.

```text
Hypersnap next.cursor
        ↓
continuation::Store
        ↓
SQLite binding
(capability id + provider id + raw cursor)
        ↓
opaque ContinuationToken
```

On the next request the token is resolved and checked for both provider affinity and capability affinity. Cross-capability token reuse fails before a network request is issued.

## HTTP transport

`farcaster.transport.http` is the provider-neutral contract. Concrete transports remain outer adapters.

`farcaster.transport.curl` remains the provider/core contract implementation. The runnable Qt UI now composes `farcaster.transport.qtnetwork`, which uses Qt Network and platform proxy/TLS facilities without changing provider code. The concrete Qt implementation lives under `src/transport/qt_network/`; `src/platform/` is reserved for adapters to application/platform ports.

On Apple, the provider/core libcurl build still links the system curl dylib directly instead of importing SDK `/usr/include` paths. The graphical runtime does not depend on that libcurl TLS path.

On non-Apple Unix, libcurl prefers the pkg-config imported target so static-libcurl transitive dependencies are preserved when distributions expose a static `/usr/local/lib/libcurl.a`. See `docs/28-qt-network-ui-transport.md`.

## Local schema changes

Schema migration 3 adds `casts.parent_url` while preserving Phase 2 databases. Migration checks `PRAGMA table_info(casts)` and executes:

```sql
ALTER TABLE casts ADD COLUMN parent_url TEXT;
```

only when required.

The existing embed migration remains intact and canonical cast persistence now round-trips protocol parent URLs.

## Contract-test coverage

`provider.hypersnap.contract` proves:

- user/profile mapping;
- cast mapping;
- URL and cast-reference embeds;
- RFC3339 timestamps;
- malformed JSON failure;
- endpoint/query construction and percent encoding;
- profile/identity/cast reads;
- HTTP error mapping;
- feed pagination and opaque cursor binding;
- provider/capability continuation affinity;
- following/trending/channel feed behavior;
- followers/following relation direction;
- exact advertised capability set;
- absence of unsupported for-you capability;
- libcurl transport construction/linkability;
- channel `parent_url` preservation.

## Non-goals for Phase 3

Phase 3 does not implement:

- protocol writes or signing;
- durable write outbox;
- authentication/sign-in UI;
- messaging;
- realtime subscription/gap repair;
- Mini App hosting;
- wallet functionality;
- recommendation/for-you infrastructure;
- UI composition against the provider.

Those remain later phases; the next architectural step is the read-only local-first vertical slice.
