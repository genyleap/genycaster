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
