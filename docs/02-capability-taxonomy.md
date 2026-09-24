# 02 — Capability Taxonomy

## Why capability classification exists

The word “Farcaster feature” is too broad. A Cast, a For You feed, a Mini App index, a referral system, a wallet on-ramp, and a Direct Message do not have the same architectural origin.

Every capability must declare its class.

## Capability classes

### Protocol

Canonical protocol identity/data/operations.

Typical examples:

- cast read;
- cast/reply/recast publication;
- reactions;
- follows/social graph;
- profile protocol data;
- identity/FID resolution;
- signer/key registry operations.

### Derived

Computed or indexed functionality built from canonical data or behavioral signals.

Typical examples:

- following feed materialization;
- trending;
- For You ranking;
- global search;
- Mini App discovery index;
- channel directory/search;
- recommendations;
- notification aggregation;
- reputation/ranking signals.

Derived capabilities may be fully self-hosted and should generally be owned independently when strategically important.

### Compatibility

Features required for interoperability with existing flagship/legacy infrastructure but not suitable as core dependencies.

Typical examples may include:

- legacy messaging;
- backend-specific QR login;
- referrals;
- legacy account settings;
- managed-backend-only metadata;
- hosted workflows not represented by open protocol semantics.

### Extension

Optional open functionality introduced by this project or adopted as an interoperable extension.

Potential examples:

- secure-chat/v1;
- typing/v1;
- presence/v1;
- read-receipts/v1;
- voice-message/v1;
- shared-miniapp/v1;
- live-room/v1.

An extension should be specified by open semantics and capability negotiation rather than being hidden inside a proprietary backend.

### Local

Device/client-only functionality.

Examples:

- drafts;
- theme/font preferences;
- local collections/bookmarks if not synchronized;
- media cache;
- local full-text index;
- local privacy settings;
- local session/display state.

## Capability granularity

Capabilities should be operation-level when behavior differs materially.

Bad:

```text
Feeds = supported
```

Better:

```text
feed.following.read
feed.trending.read
feed.channel.read
feed.for_you.read
```

A provider may implement some natively and others partially or not at all.

## Capability quality

A capability descriptor must distinguish availability from quality.

Suggested quality states:

```text
Native
Derived
Emulated
Partial
Experimental
Unavailable
```

An endpoint existing is not evidence that a capability is semantically native or complete.

## Capability descriptor — conceptual form

```cpp
struct CapabilityDescriptor {
    CapabilityId id;
    CapabilityVersion version;
    CapabilityClass kind;
    CapabilityQuality quality;

    bool readable;
    bool writable;
    bool realtime;
    bool historical;
    bool self_hostable;
};
```

The exact C++ API is deferred until implementation, but these semantics are mandatory.
