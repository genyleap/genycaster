# Feed, status, search and embed follow-up

The external Mini App launch described in this historical review is superseded
by `MINIAPP_HOST_REVIEW.md` and the optional internal macOS host implementation.

Responsibility: prevent failed pagination loops and false recovery flashes;
improve filtered feed discovery, search metadata and safe Mini App embed display.
Capability origin: Local presentation; Derived search/preview metadata.
Mandatory or optional: existing surfaces only; Mini App execution remains optional.
Public contract: existing feed/search/relationship and external-opening intents.
Canonical data/invariants: unavailable is not recovered until a terminal successful
read; unknown relationships are not false; filters never fabricate matching posts.
Dependencies: existing read bridge, provider-neutral search snapshots, relationship
gateway, native link resolver and QML presentation. No QML network or WebView I/O.
Expected failures: circuit-open/transport failures stop automatic pagination;
missing metadata retains an honest fallback; invalid Mini App payloads are ignored.
Local-state representation: existing cached rows, profile relationships and link metadata.
Offline behavior: no automatic pagination while disconnected; retain cached views.
Provider policy and disablement: unchanged; no provider identifiers enter core models.
Failover semantics: manual retry remains available; successful reads clear failures.
Tests proving the boundary: source inspection only; user explicitly forbids execution.

Mini App scope: recognize validated fc:miniapp / compatible fc:frame embed metadata
and show its image/button as an external launch affordance. This is NOT an embedded
Mini App host, wallet, SDK bridge or authenticated execution environment. Those
require an independently isolated optional host and explicit capability consent.
Reference: https://miniapps.farcaster.xyz/docs/specification

## Source findings and changes

- Automatic near-end pagination retried failed continuations without checking
  offline/source/error state. Both the scheduler and native admission now stop.
- Starting an intent/cache refresh cleared failure health before recovery was
  known. All four read commit paths now retain failures while busy.
- Local home tab switches restarted the same trending feed, losing accumulated
  pages. Switching between categories now reuses an already-selected source.
- Conversation filtering wrongly excluded threads with media; Visuals omitted
  videos, and NFT filtering omitted the existing token kind. Cached resolver
  classification is now consulted for extensionless media.
- These tabs still filter a shared feed, not independent network-wide indices.
  Sparse data and upstream source failures cannot guarantee populated categories.
  Bounded additional paging and an explicit filtered-empty explanation are exposed.
- Search displays canonical profile counts, channel descriptions/counts and
  both relationship directions. Requests are capped at 12 in flight; incoming
  relationship cache is bounded to 256 entries with a 60-second freshness window.
  Account changes clear search/relationship snapshots; local mutations outrank
  late read-only following results. QML patches stable result delegates.
- Mini App metadata is bounded, JSON parsed, version/action checked, and uses
  public HTTPS image/launch URLs without userinfo. The launch domain and external
  nature are visible. No author verification, WebView, SDK or wallet support is claimed.
- Link images preserve their full contents; single-photo widths are bounded.
- New strings have English/Persian entries; other locales use unfinished English
  fallbacks pending translation.

## Verification evidence

Read the existing provider search/relationship test contracts and read lifecycle
source. No build, app, tests or lint were run. `git diff --check` completed without
errors for tracked changes; untracked UI files were reviewed directly. Runtime
visual smoothness, live provider recovery and populated filter categories remain
unverified. Full Mini App hosting is deliberately not implemented by this patch.
