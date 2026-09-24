# Phase 4.5C — Rich Cast / Embed / Engagement UI Feature Gate

Responsibility: Productionize the read-only cast surface on top of the Phase 4.5B local-first Following Home.

Capability origin: Protocol for engagement counters and protocol embeds; Local for presentation state, pagination triggering, skeletons, and safe external-open policy.

Mandatory or optional: Rich read presentation is mandatory for the Phase 4.5C UI; no write capability is introduced.

Public contract: `farcaster.domain.cast::Cast` owns provider-neutral aggregate engagement counters and canonical embeds. Presentation exposes normalized row/detail maps only. External URLs are opened through the Qt presentation boundary after HTTP(S) validation.

Canonical data/invariants: Reply/recast/like counts are non-negative canonical snapshot values. Existing URL and cast-reference embeds remain provider-neutral. Provider cursors never enter QML. Cached rows remain authoritative while refresh is in flight.

Dependencies: Domain → Foundation; provider adapter → Domain; Storage persists canonical cast snapshots; Presentation → Application; QML → Presentation only.

Expected failures: Missing/malformed provider engagement fields, unavailable network/provider, exhausted pagination, invalid external URL, and OS failure to open a validated URL.

Local-state representation: SQLite schema migration 6 persists reply/recast/like counters with each cast. Existing embed persistence remains unchanged.

Offline behavior: Cached casts, engagement counters, and embed links render without network access. Skeletons appear only when no authoritative rows exist. External opening is user initiated and delegated to the OS.

Provider policy and disablement: No provider is required by the canonical model. Hypersnap maps its read DTO fields inside its adapter only; provider-free profiles remain valid.

Failover semantics: Existing routed read/circuit behavior is unchanged. Pagination and refresh use existing Application read intents and provider-private continuation resolution.

Tests proving the boundary: Provider contract tests map engagement values; storage behavior round-trips them; architecture guards continue to enforce provider/QML boundaries; UI smoke and strict QML lint cover the graphical composition where Qt is available.
