# Storage

This is the production provider-neutral persistence boundary. The current SQLite implementation includes schema migrations through **migration 23** and stores local-first read state, continuations, sessions/signer metadata, durable write outbox state, optimistic interaction projections, and related application persistence.

Provider-specific protocol/backend code does **not** belong here. Storage exposes canonical/local persistence semantics to Application and composition while concrete provider adapters remain under `src/providers/`.

See `STATUS.md`, `docs/03-dependency-and-module-rules.md`, `docs/04-provider-boundary-and-routing.md`, and the storage/write ADRs for current constraints.
