# Explicit, capability-compatible Read fallback

## Feature gate

| Question | Decision |
|---|---|
| Responsibility | Restore indexed Explorer/search reads when a selected raw protocol node cannot supply them, without silent cross-provider data disclosure |
| Capability origin | Derived read / routing policy, not a new Farcaster protocol capability |
| Mandatory/optional | Optional, disabled unless the user/operator explicitly consents |
| Public contract | Existing `CapabilityRouter::setPreferredProviderForAccess(Read, ProviderPreference)` and Qt provider settings |
| Canonical data | Unchanged: provider-neutral `Cast`, `Profile`, `FeedPage`, `ContinuationToken` |
| Dependencies | Capability registry, health policy, Qt QSettings, installed read adapters |
| Expected failures | No eligible compatible fallback, strict private/self-hosted policy rejection, selected adapter missing, invalid persisted registration |
| Local-state representation | `network/allowCompatibleReadFallback` (boolean), restart required |
| Offline behavior | Cached reads remain available; no new network request occurs without runtime route admission |
| Provider policy/disablement | No consent means strict selected-read pin; strict self-hosting still prohibits managed endpoints even with consent |
| Failover semantics | When consent is on, Read may use an eligible compatible adapter for unsupported/unhealthy capabilities. Canonical Write and Signer remain strictly selected and durable queued writes are endpoint-pinned |
| Tests | `core.behavior`, `routing.durable_write_provider`, `routing.signer_provider_selection`, `ui.provider_profiles_contract`, `ui.node_selection_contract`, `ui.provider_translation_contract`, `tests/wasm/compatible_read_fallback_consent.test.mjs` |

## Default and opt-in behavior

A typed custom provider declares only its actually implemented capabilities. In particular,
a raw `snapchain-v1` reader supports identity/profile and FID-scoped casts, **not**
Trending/Recent/Search. The application previously pinned a selected raw reader
with `require=true` for *every* Read capability. That caused Explorer and search
to fail even when a separately registered indexed `hypersnap-v2` compatible
reader could legally satisfy these independent, derived capabilities.

The new setting **Allow compatible read fallback** is `false` by default.
The UI explains that enabling it permits reads, including Explorer and search,
to contact other eligible providers, potentially managed services. The toggle
is explicitly consent-based, is not treated as node capability auto-detection,
and takes effect on next runtime creation (restart). The current UI exposes
the option in provider/network settings, with full translations for the twelve
existing locales and a visible restart-required warning.

- Consent **off**: selected Read instance remains `require=true`. Unsupported
  capabilities fail closed with the existing detailed capability error.
- Consent **on**: selected Read instance remains the preferred source for each
  operation it implements; `require=false` permits an alternate eligible
  registered source only when needed.
- Strict Self-Hosted: the router's deployment/privacy eligibility filter remains
  mandatory. Opting into alternate Read sources cannot authorize a managed one.
- Write: always `require=true`; durable outbox bindings are not relocated,
  and Signer management continues to use selected Write affinity.

The application and GUI startup both honor the same consent snapshot.
The legacy network-selection UI can still change the base network preference
without moving canonical writes. Existing native provider profile settings
continue to be consumed only on restart, preserving snapshot consistency.

## Configuration

- User setting: `network/allowCompatibleReadFallback=true` in existing QSettings.
- Explicit CLI opt-in: `--allow-compatible-read-fallback`.
- Explicit environment opt-in: `GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK=1`
  (when set, this environment override takes precedence over the saved setting).
- If the flag is absent, the environment variable is unset, and the setting is
  absent/false, provider read pinning remains strict.

Other provider credentials remain isolated to their configured adapter
instances. The fallback setting neither copies API keys to other hosts nor
changes read/write transport redirection policy.

## Boundaries and follow-up

This work **does not** implement a network-wide independent index. It allows
an installed compatible indexed provider, if one exists and is eligible, to
handle queries the selected raw node cannot serve. The indexed destination's
privacy properties must be evaluated before enabling consent.

It does not replace explicit per-capability routing UI, authenticated arbitrary
provider credentials, node health tooling, multi-node live testing, full WASM
release verification, or the final architecture acceptance gates.
