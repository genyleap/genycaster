# 06 — Feature Compartmentalization

## Principle

A Farcaster client is not required to include every feature historically present in a flagship application.

Feature modules are independent product capabilities layered on the core.

> Don't want DMs? Don't use them. Don't want Channels? Don't use them. Don't want Mini Apps? Don't use them.

This must be true architecturally, not merely visually.

## Core versus optional modules

A minimal social client may consist of:

```text
foundation
identity
casts
profiles
social graph
local storage
sync
```

Optional feature modules may include:

```text
feed
channels
miniapps
legacy messaging
secure chat
wallet
search
notifications
spaces/live
```

## Feature dependency rule

An optional feature may depend on core modules, but unrelated core/feature modules must not depend on it.

For example:

```text
miniapps → social/core   allowed
core     → miniapps      forbidden
chat     → wallet        forbidden unless explicitly optional through a capability
```

## Build profiles

Later CMake configuration may expose options such as:

```text
FARCASTER_ENABLE_CHANNELS
FARCASTER_ENABLE_MINIAPPS
FARCASTER_ENABLE_LEGACY_DM
FARCASTER_ENABLE_SECURE_CHAT
FARCASTER_ENABLE_WALLET
FARCASTER_ENABLE_SEARCH
```

These flags are useful only if the dependency graph already respects the boundaries.

## Derived cast-view analytics gate

`FARCASTER_ENABLE_DERIVED_CAST_VIEWS` is an optional Qt composition flag
(default ON). It controls the external, public view-count HTTP enrichment
adapter; it does not remove canonical casts, protocol reactions, replies,
or offline reading. Disabled builds leave count values unknown rather
than inventing zero. The same flag applies on native and WebAssembly.
The Qt adapter owns viewport interest, single-flight batch admission
(16 casts), a 96-item waiting limit, 384 retained per-cast observables,
three-minute positive freshness, 90-second unknown retry, and a six-second
transport deadline. Mobile and WASM builds instead use a 48-target queue and
192 retained observables to reduce memory pressure while keeping the same
batching/data semantics. The gateway independently imposes a six-second logical
work budget and global upstream concurrency of three.

The controller is a Qt MOC compatibility boundary under `ui/mobile`, not
a new stable core service or generic helper library. Every QML consumer
only observes an individual count and explicitly retains/releases interest.

The existing native link-preview resolver remains an independent provider
adapter. It admits at most two concurrent metadata requests and 64 queued
preview URLs, retaining at most 320 cached previews. Newer pending links
replace the oldest unstarted work, with a 15-minute successful-preview
freshness policy. Active requests are never evicted from its metadata cache.
On the single-threaded browser path it retains only safe URL-derived seeds,
without native worker threads or blocking waits.

## Mini App decomposition

Mini Apps must not be represented as one monolithic provider capability.

Separate concerns:

```text
Manifest Resolution
Runtime / Host
Discovery Index
Search
Ranking
Notifications
```

A Mini App URL should be runnable via manifest resolution even if no discovery/index provider exists.

Therefore:

```text
Mini App execution != Mini App discovery
```

## Messaging decomposition

Messaging must support independent transports/capabilities.

Conceptually:

```text
MessagingFacade
   ├── LegacyMessaging compatibility
   └── SecureMessaging extension
```

A conversation may declare/use a transport rather than the entire application selecting one messaging backend globally.

## Channel decomposition

Avoid a monolithic `IChannelProvider` if semantics diverge.

Possible concerns:

- channel identity/directory;
- channel membership;
- moderation;
- channel feed;
- channel search/ranking;
- channel chat.

The feed capability should remain a feed concern rather than being duplicated inside channel infrastructure.

## Settings decomposition

Do not create a generic remote `ISettingsProvider` for everything.

Separate:

- local device settings;
- synchronized client preferences;
- protocol/account configuration;
- provider-specific compatibility settings.
