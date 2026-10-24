# Explicit per-indexed-provider credentials (environment-only phase)

## Feature gate

| Topic | Contract |
|---|---|
| Responsibility | Support authenticated independent indexed read nodes without leaking keys to unrelated read providers |
| Capability origin | Adapter-local configuration; not Farcaster protocol identity, account login or write signer |
| Mandatory/optional | Optional operator-injected key; no key by default |
| Public contract | Existing indexed adapter `Config.apiKey`; composition takes an ephemeral map indexed by exact provider ID |
| Data invariants | Canonical Cast, Profile, provider profile JSON and saved QSettings are untouched |
| Dependencies | QJsonDocument at startup, Qt async/HTTP transport and typed indexed v2-compatible adapter |
| Expected failures | Invalid/unknown/provider mismatch or insecure endpoint rejects startup; API key with controls/out-of-range rejected before HTTP |
| Local state | **None**: keys are not stored in profiles, SQLite or settings |
| Offline | Cached Read remains available, key is needed only for authenticated network requests |
| Provider disablement | No key attached to unconfigured, raw Snapchain or other provider instance; explicit registration/route still mandatory |
| Redirect | Qt transport uses ManualRedirectPolicy on requests carrying sensitive headers |
| Tests | Indexed native contract (isolated GET/POST headers, invalid HTTPS/control), WASM structural regression, Qt compilation |

## How to configure (operator environment)

Set `GENYCASTER_READ_PROVIDER_API_KEYS_JSON` to a JSON object mapping
**exact user-defined read provider IDs** to their indexed API credentials:

```text
GENYCASTER_READ_PROVIDER_API_KEYS_JSON={"my-private-index":"<secret>"}
```

This is **not** a CLI argument. Supply the JSON through a protected runtime
environment injection method, not a committed shell script or checked-in
config file. The provider must already exist in the ordinary
`network/readProviderProfilesJson` settings or startup profile JSON with:

- `id`: matching the JSON key exactly;
- `contract`: `hypersnap-v2`;
- `endpoint`: HTTPS URL with no userinfo, query or fragment.

Only 16 secrets may be supplied, each 1..256 ASCII printable non-whitespace
bytes (no CR/LF/header injection). The runtime binds keys by exact provider ID,
not network preference, hostname guessing, adapter-name fallback or a shared
global header. Supplying a key for an unknown, raw, or HTTP node fails startup
without printing the secret. The existing legacy
`GENYCASTER_FARCASTER_READ_API_KEY` remains independent and pinned to its
canonical hosted service.

The asynchronous adapter and blocking compatibility adapter both now
validate credentials and attach the same `x-api-key` header. Qt disables
automatic redirects for credentialed requests, so a redirect response is
never followed with that credential to another host.

## Security boundary and future work

**Process environment variables are not an OS credential vault.** They can be
read by sufficiently privileged local processes and are unsuitable as a
general-purpose user secret storage mechanism. This step enables explicitly
configured operator-owned read nodes; it does not provide in-app credential
entry, Keychain/Secure Enclave/Android Keystore integration, rotation/revoke
UX, mobile secret synchronization or a self-hosted credential broker.

Read credentials must never be copied into canonical write or signer routes.
The endpoint must be trusted and TLS-certificate validated. Complete
multi-node authenticated live smoke tests and a production secret vault remain
release acceptance tasks.
