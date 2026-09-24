# Live insights, token embeds, profile quality and adaptive media

Feature implementation gate for the live-insights/UI refinement work.

## Market Pulse and token-address embeds

Responsibility: Resolve configured crypto assets and contract-token references into live, provenance-bearing market snapshots used by Token Pulse and rich cast embeds.
Capability origin: Extension / Derived.
Mandatory or optional: Optional extension; disabled builds retain feed/profile functionality and show unavailable market state rather than fabricated values.
Public contract: `farcaster.capability.market.IMarketDataSource` over provider-neutral `MarketAsset`, `TokenReference`, and `MarketSnapshot` domain values.
Canonical data/invariants: Prices and changes are numeric snapshots with observation time and source; contract addresses are EVM 20-byte hex strings paired with a network; no synthetic prices are emitted.
Dependencies: domain/foundation, provider-neutral HTTP transport, public market adapter at composition time.
Expected failures: transport timeout, rate limit, unsupported asset/network, malformed provider payload, unavailable pool/quote.
Local-state representation: selected assets and custom token references are persisted in QSettings by the Qt composition controller; live snapshots are memory-cached and expire naturally.
Offline behavior: last in-process snapshot remains visible and is marked stale/unavailable on failed refresh; no generated fallback numbers.
Provider policy and disablement: public market provider can be compiled out; provider URLs remain inside `src/providers/public_data` configuration.
Failover semantics: CoinGecko is used for native BTC/ETH market snapshots; GeckoTerminal is used for EVM contract assets and token-card liquidity/pool metadata. A failed source does not affect core Farcaster reads.
Tests proving the boundary: provider-free core configuration remains buildable; UI design/architecture guards reject QML network I/O and provider URLs outside the adapter.

## Profile quality score fallback

Responsibility: Surface the canonical profile score when supplied by the Farcaster read provider and fill missing values from Neynar's public onchain score reader.
Capability origin: Derived.
Mandatory or optional: Optional enhancement.
Public contract: `farcaster.capability.profile_quality.IProfileQualitySource` returns a normalized score plus source and observation time.
Canonical data/invariants: score is normalized to [0, 1]; zero from the onchain reader means unavailable, not a zero-quality user.
Dependencies: identity/foundation and provider-neutral HTTP transport.
Expected failures: RPC unavailable, no score, invalid response.
Local-state representation: short-lived in-memory cache in the Qt composition controller.
Offline behavior: existing canonical profile score remains usable; missing score stays unavailable.
Provider policy and disablement: Base RPC and Neynar score contract live only in the public-data adapter configuration.
Failover semantics: canonical profile payload wins; onchain lookup is fallback-only.
Tests proving the boundary: provider-free profile model remains unchanged and architecture checks keep provider concerns out of QML/application code.

## Community Pulse, media and mobile/editor refinements

Responsibility: Present live feed-derived community activity, aspect-aware media, refined composer controls, stronger drawer depth, and a five-destination mobile dock.
Capability origin: Derived / Local.
Mandatory or optional: Presentation behavior.
Public contract: existing presentation maps and QML component APIs.
Canonical data/invariants: Community Pulse is computed only from loaded canonical feed rows; image geometry preserves source aspect and never invents media metadata.
Dependencies: presentation only.
Expected failures: missing media dimensions or empty feed sample degrade to neutral states.
Local-state representation: none beyond existing settings and presentation rows.
Offline behavior: cached rows/media remain renderable.
Provider policy and disablement: none.
Failover semantics: unknown image aspect falls back to current safe media dimensions.
Tests proving the boundary: QML manifest/design checks and source-level architecture guards.
