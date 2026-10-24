# Provider-neutral node profiles in Qt settings

## Feature implementation gate

| Item | Contract |
|---|---|
| Responsibility | Let users register/remove compatible endpoint instances and choose Read/Write providers independently |
| Capability origin | Local configuration and routing preferences; protocols remain in adapters |
| Mandatory/optional | Optional UI; app can browse using registered defaults without custom profiles |
| Public boundary | `ProviderProfilesController` (Qt MOC-compatible, presentation/composition only) |
| Canonical data | Stable provider instance ID, display label, adapter contract, endpoint URL, self-hosted operator assertion |
| Dependencies | Qt Core settings and JSON; composition reads same four QSettings keys |
| Failure modes | Invalid/credential-bearing URL, unknown contract, duplicate/reserved ID, profile limit, unsupported compiled adapter, failed persistence |
| Local state | QSettings JSON arrays and preferred provider IDs, no auth credentials |
| Offline | Adding/removing profiles requires no network; actual operational capabilities depend on live connection |
| Policy and disablement | Adapter support exposed by compilation flags; write options disabled in web/read-only builds |
| Failover | Read remains governed by router policy; chosen writer is pinned for **new** messages; queued canonical messages keep original persisted endpoint |
| Tests | `ui.provider_profiles_contract`, `ui.node_selection_contract`, `tests/wasm/provider_profiles_ui_contract.test.mjs`, translation compilation for all 12 locales |

## Data contract

The controller owns **only presentation configuration**, not runtime HTTP, provider DTOs, canonical Farcaster data, or route policy. It writes the exact keys the existing runtime reads during composition:

- `network/readProviderProfilesJson`: JSON array with `id`, `label`, `contract`, `endpoint`, `selfHosted`.
- `network/writeProviderProfilesJson`: same fields with optional `readEndpoint`.
- `network/preferredReadProviderId`: ID of a registered read provider, empty to use existing policy defaults.
- `network/preferredWriteProviderId`: ID of a registered write provider, empty to use existing default writer.

Adapter contract selection is explicit. The installed read-adapter options are `snapchain-v1` for raw protocol data and `hypersnap-v2` for compatible indexed reads. The installed write adapter option is `snapchain-v1`; additional independent write profiles can be added if supported by the build. These **contract names**, not vendor identity, determine which compiled adapter is used.

The controller verifies safe endpoint syntax (HTTP(S), host required, no embedded username/password, query or fragment) and permits up to sixteen entries per access type. Profile IDs are length-bounded, unique across Read and Write instances, and cannot shadow the built-in runtime identities. Only non-secret metadata is persisted. Users must never place API keys in endpoint URLs.

## UI integration

The existing language section is reserved for language/region settings. The former network-choice, primary validator URL, and proxy controls were relocated unchanged to the **Content Loading** section. A separate `ProviderProfilesPanel` above them exposes compatible provider lists, optional forms for adding nodes, per-node selection/removal, and restart-required warnings. Controls use theme-aware icons and direction-aware text fields; strings are fully translated into all twelve supported locales.

This change does **not** automatically mutate the currently running HTTP transport, so the UI correctly advertises that a restart is required. It also does not silently migrate previously pinned/queued writes when a provider profile is removed.

## Security and migration boundaries

The `selfHosted` value is an operator assertion used by deployment policy, **not automatic proof** of hosting ownership. Arbitrary hosts are never probed as proof of compatibility. The user chooses an installed contract; unsupported contracts are rejected. Provider credentials must be isolated in per-provider secure settings before introducing authenticated remote profiles.

The profile/selection removal is written in one QSettings synchronization step, avoiding a removed provider lingering as the selected preference after a partial update. Persistence failures restore the previously loaded values where possible and surface a localized error.

CLI/environment overrides retain higher priority than QSettings; the UI edits saved settings for the next launch rather than silently overriding a deployment-managed configuration. Full interactive capability discovery, authenticated per-provider tokens, node health probing, live route switching and a self-owned aggregate indexing system remain separate requirements before describing the overall multi-node architecture as complete.

## Acceptance

The profile CRUD and preference controller has direct Qt C++ tests using a real temporary QSettings file, covering save/restart, invalid endpoints, reserved/duplicate IDs, optional reconciliation endpoints, removal and preference clearing. The QML contract test verifies that the new panel is reachable from network/content settings and all 28 provider translation keys are present with nonempty translations in all twelve locales. The old Hypersnap node selector has its own unchanged regression test.
