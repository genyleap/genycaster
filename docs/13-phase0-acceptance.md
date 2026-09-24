# 13 — Phase 0 Acceptance Criteria

Phase 0 is complete when the architecture can be handed to another engineer or AI agent and they can implement Phase 1 without inventing provider coupling or feature coupling.

## Required documents

- [x] architecture overview
- [x] non-negotiable invariants
- [x] capability taxonomy
- [x] dependency/module rules
- [x] provider routing policy
- [x] local-first/sync contract
- [x] feature compartmentalization
- [x] domain normalization policy
- [x] security/auth/signing boundaries
- [x] derived infrastructure policy
- [x] build/deployment profiles
- [x] testing/architecture guards
- [x] roadmap
- [x] performance/UI runtime contract
- [x] initial capability inventory
- [x] ADRs for major decisions
- [x] AI-agent entry instructions

## Architecture review checklist

A reviewer must be able to answer “yes” to all items:

### Provider independence

- Can every managed backend be absent without redesigning the core?
- Can Hypersnap be replaced without changing domain/application contracts?
- Can a self-hosted-only deployment exist?
- Are provider DTOs/endpoints/cursors/errors trapped behind adapters?

### Capability model

- Are capabilities classified as protocol/derived/compatibility/extension/local?
- Can capability support/quality vary per operation?
- Is routing per capability/operation rather than global provider selection?

### Feature modularity

- Can DMs be removed?
- Can Channels be removed?
- Can Mini Apps be removed?
- Can Wallet/Search/Notifications be removed?
- Do optional features depend inward without becoming core dependencies?

### Local-first

- Is local state the app-facing source of truth?
- Is sync separated from rendering?
- Are UI-thread I/O and parsing forbidden?
- Is offline behavior defined?

### Writes/security

- Are protocol writes canonicalized/signed before provider submission where possible?
- Is blind write failover forbidden without idempotency/reconciliation semantics?
- Are keys isolated from QML/provider DTOs?

### Derived infrastructure

- Is search/index/ranking/discovery recognized as a separate strategic layer?
- Is Mini App discovery separated from Mini App execution?
- Is backend-derived moat prevented from becoming hidden core coupling?

## Phase 0 output status

This documentation package is the Phase 0 baseline.

No Phase 1 implementation should begin until this contract is accepted or amended through explicit ADRs.
