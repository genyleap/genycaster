# Phase 0 Review Sheet

Use this file to approve or amend Phase 0 before implementation starts.

## Proposed architecture in one sentence

> A protocol-first, local-first, capability-driven, provider-agnostic, feature-compartmentalized native Farcaster platform, with Hypersnap as a preferred routing policy for suitable protocol-native capabilities, compatibility providers isolated behind optional adapters, and strategically important derived infrastructure owned independently.

## Decisions to approve

- [ ] Capability is the architectural abstraction; provider is an implementation detail.
- [ ] Hypersnap-first is a default routing policy, not a dependency.
- [ ] Managed/flagship backends are compatibility providers only.
- [ ] Features such as DMs/Channels/Mini Apps/Wallet/Search are independently optional.
- [ ] The UI reads local canonical state first.
- [ ] Provider data is normalized into canonical domain models.
- [ ] Provider cursors/errors/tokens never leak across the adapter boundary.
- [ ] Protocol writes are locally canonicalized/signed where possible before transport selection.
- [ ] Search/ranking/discovery/indexing are a distinct derived-infrastructure layer.
- [ ] Strict self-hosted/private operation is an architectural target.
- [ ] Phase 1 begins only after these constraints are accepted or amended through ADRs.

## Amendments / Notes

Add requested changes below before Phase 1:

```text
-
```
