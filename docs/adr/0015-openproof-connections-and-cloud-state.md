# ADR 0015 — Account connections and app-owned state

**Status:** Accepted architectural boundary. Runtime support and platform qualification are tracked in the [Roadmap](../12-roadmap.md).

## Context

Farcaster is the authoritative source for canonical social messages and relationships. GenyCaster also supports account-specific product features that do not belong in the protocol: saved content and collections, profile details, preferences and optional connected-account metadata.

These features need consistent ownership and durability across devices. An external identity service may verify a linked account, but GenyCaster must not duplicate that provider's credentials or establish a second identity authority.

## Decision

1. **Canonical protocol remains independent.** Casts, follow relationships and signatures belong to Farcaster; the product extension database never rewrites their identity.
2. **Account ownership is verified server-side.** The service resolves a signed-in principal from a trusted identity session and linked Farcaster FID, not from a caller-supplied FID, subject or wallet address.
3. **App-owned data has a separate lifecycle.** Bookmarks, saved lists, user status, drafts and preferences are stored by the product service where enabled. Their remote availability is invisible to ordinary UI workflows; users do not have to perform a manual sync operation.
4. **Local-first continuity is retained.** The client may display known data offline and reconcile after reconnection using revisions, idempotent mutations and deletion tombstones. A stale local result cannot overwrite newer authenticated server state.
5. **Provider connections are not shadow OAuth accounts.** Account linking, credential refresh and disconnection remain with their owning identity provider. The GenyCaster UI receives only the scoped, verified metadata required to display a connection.
6. **Security boundaries remain explicit.** Account session, wallet custody, protocol signer and Mini App wallet permissions are independent privileges. Private keys, recovery phrases, provider OAuth tokens and session cookies must not be placed in QML, ordinary SQLite state or client-readable logs.
7. **Services are optional adapters.** Guest reads and core protocol functions do not depend on the account-extension API. An outage may limit app-specific data or linking without redefining a Farcaster message.
8. **Deployment is configurable.** Operators supply identity and account API origins, TLS configuration and server secrets. Public documentation uses placeholders such as `https://identity.example.com` and `https://api.example.com`; they are not real deployment endpoints.

## Service contract

- Authenticate the account principal for every private endpoint.
- Scope every read/write by server-derived owner; check revocation and expiry.
- Preserve monotonic revision and owner-specific change streams; distinguish a missing record from an unavailable service.
- Apply rate limits, input bounds, privacy restrictions and rollback/migration controls on the server.
- Keep optional profile enrichment, token entitlements and other derived product state out of canonical protocol write objects.

## Failure handling

Expired sessions, mismatched linked accounts, provider outages, conflicting revisions and partial network responses must fail safely. An identity-provider outage cannot silently restore a previously authorized signer or reuse permissions across accounts. An unknown authorization state is not permission to submit a write.

## Validation

The owning account-service tests, local state regression tests, UI account switching tests and runtime acceptance matrix are required. Implementation and current completeness are tracked in the [Feature Guide](../FEATURES.md), [Roadmap](../12-roadmap.md) and [Security](../../SECURITY.md).

This decision is architectural; dated deployment transcripts and one-off operational verification records are intentionally excluded.
