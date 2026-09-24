# 15 — Initial Capability Inventory

## Purpose

This is the initial architecture-level inventory. It is **not** a final provider support matrix.

Provider support must be verified by provider contract tests and current documentation during implementation phases. Never infer semantic completeness merely from endpoint existence.

Legend:

- `P` — Protocol
- `D` — Derived
- `C` — Compatibility
- `E` — Extension
- `L` — Local

## Core social/identity

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| identity.fid.resolve | P | yes | protocol provider / self-hosted |
| profile.read | P | yes | Hypersnap-first policy, provider-neutral core |
| cast.read | P | yes | Hypersnap-first policy |
| cast.thread.read | P/D | yes | protocol data + local/thread materialization |
| cast.publish | P | later | local canonical message/signing + submission transport |
| cast.reply.publish | P | later | same write architecture |
| reaction.read | P | yes | protocol provider |
| reaction.write | P | later | canonical signed write |
| social.following.read | P | yes | protocol provider |
| social.followers.read | P | yes | protocol/derived index depending implementation |
| social.follow.write | P | later | canonical signed write |
| signer.registry.read | P | later | protocol/on-chain/provider adapter |
| signer.registry.write | P | later | local/security-owned flow, provider optional |

## Feeds and ranking

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| feed.following.read | D/P | yes for full social client | Hypersnap-first initially; independently materializable |
| feed.trending.read | D | optional | provider/own derived service |
| feed.channel.read | D/P | optional with Channels | provider-neutral feed facade |
| feed.for_you.read | D | optional | own ranking engine long-term |
| ranking.personalized | D | optional | own/self-hosted derived infrastructure |
| ranking.trending | D | optional | own/provider-derived |

## Search/discovery

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| search.network | D | optional | own/self-hosted index |
| search.local | L | optional | SQLite FTS5 |
| miniapp.manifest.resolve | P/Open spec | optional | direct/spec resolver |
| miniapp.runtime | E/Open spec | optional | local host/runtime |
| miniapp.discovery | D | optional | own index; managed provider only as compatibility/fallback |
| miniapp.search | D | optional | own index |
| miniapp.ranking | D | optional | own engine |
| channel.directory | D | optional | own/provider-neutral index |
| channel.search | D | optional | own index |

## Channels

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| channel.identity.read | P/D | optional | provider-neutral |
| channel.membership.read | P/D | optional | capability adapter |
| channel.membership.write | P/C | optional | exact semantics to verify |
| channel.moderation | P/C | optional | separate moderation capability |
| channel.feed | D/P | optional | feed facade |
| channel.chat | C/E | optional | messaging subsystem, not channel core |

## Messaging

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| messaging.legacy.read | C | no | compatibility adapter only |
| messaging.legacy.write | C | no | compatibility adapter only |
| messaging.secure | E | no | own open E2EE extension |
| messaging.typing | E | no | negotiated extension |
| messaging.presence | E | no | negotiated extension |
| messaging.read_receipts | E | no | privacy-aware negotiated extension |
| messaging.voice | E | no | secure messaging extension |

## Notifications

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| events.ingest | P/D | optional | protocol/webhook/realtime source |
| notifications.aggregate | D | optional | own engine |
| notifications.push | C/E | optional | platform/provider transport |
| notifications.preferences | L/C | optional | local + optional sync |

## Authentication/session

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| identity.protocol | P | yes | independent of client session |
| auth.client_session | L/C | depends on deployment | own/application session boundary |
| auth.qr_compatibility | C | no | compatibility provider only |
| auth.device_pairing | E | no | open/own mechanism |
| auth.passkey | E/L | no | platform adapter + application service |

## Wallet/financial interactions

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| wallet.account | L/E | no | user-controlled/local/external wallet |
| wallet.sign_transaction | L/E | no | local/platform/external signer |
| chain.rpc | E | no | independent provider capability |
| assets.read | D/E | no | independent source |
| valuation.asset_price.read | D | no | provider-neutral timestamped market quote source |
| valuation.fx.read | D | no | provider-neutral timestamped fiat FX source |
| swap.quote/execute | C/E | no | optional service capability |
| onramp | C | no | optional third-party capability |

## Settings/product backend

| Capability | Class | Mandatory? | Preferred direction |
|---|---:|---:|---|
| settings.device | L | no | local only |
| settings.client_preferences | L/E | no | local + optional sync |
| settings.protocol_account | P | depends | protocol capability |
| referrals | C | no | compatibility/product module only |
| managed reputation score | D/C | no | derived signal, never canonical profile field |

## Phase 1/3 priority subset

The first real implementation should prove only a narrow subset:

```text
identity.fid.resolve
profile.read
cast.read
social.following.read
social.followers.read
feed.following.read
feed.trending.read
feed.channel.read (if available/verified)
```

Everything else stays out until the foundation is validated.
