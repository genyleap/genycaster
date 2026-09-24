# ADR 0004 — Own Strategically Important Derived Infrastructure

- Status: Accepted for Phase 0

## Context

Important product experiences such as Mini App discovery, search, ranking, recommendations, trending, and directories may not be protocol primitives.

Historically, these layers can become a flagship-client moat because third-party clients otherwise need to consume private/managed endpoints or rebuild expensive indexes themselves.

## Decision

Derived capabilities are modeled separately from protocol capabilities.

Strategically important derived capabilities should migrate toward independently owned/self-hostable infrastructure rather than permanent compatibility dependencies.

## Consequences

- additional backend/indexing infrastructure is expected;
- protocol data remains cleanly separated from product intelligence;
- product differentiation becomes controllable by this project;
- Mini App runtime can remain independent from Mini App discovery.
