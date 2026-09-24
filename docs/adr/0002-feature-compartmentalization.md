# ADR 0002 — Product Features Are Optional Compartments

- Status: Accepted for Phase 0

## Context

Historically, flagship Farcaster clients bundled protocol functionality with channels, DMs, Mini Apps, wallet flows, referrals, settings, discovery, and other backend-driven capabilities.

A competing client should not inherit that bundle as a mandatory architecture.

## Decision

Non-core features are independent optional modules.

DMs, Channels, Mini Apps, Wallet, Search, Notifications, and future extensions may be omitted without redesigning core social functionality.

## Consequences

- multiple product profiles can share the same core;
- compatibility features can be retired without architectural surgery;
- build/runtime capability discovery is required;
- feature dependency discipline becomes important.
