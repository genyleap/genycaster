# 10 — Build and Deployment Profiles

## Purpose

Provider and feature independence must be executable policy rather than documentation-only intent.

## Current verification presets

### `core-debug`

Provider-free Debug core with tests and no Qt requirement.

### `strict-self-hosted`

Same provider-free core with `FARCASTER_SELF_HOSTED_ONLY=ON`. `configuredDeploymentProfile()` must report `StrictSelfHosted` and routing tests prove managed providers are ineligible under strict policy.

### `minimal-core`

All optional product composition flags are disabled. The flags are compiled into `farcaster.application.configuration.configuredFeatures()`; no empty feature libraries are created merely to represent them.

### `provider-auth`

Headless compatibility-auth profile. It keeps the read provider disabled, enables `FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS`, builds the SIWF adapter and its contract tests, and does not require Qt. This profile exists so authentication can be verified independently from the graphical composition and independently from live read-provider availability.

### `secure-signer`

Headless protocol-signer profile. It keeps read/auth providers and Qt disabled while enabling `FARCASTER_ENABLE_PROTOCOL_SIGNING`. On macOS this composes the Keychain/OpenSSL Ed25519 vault boundary; on platforms without an implemented secure vault the adapter compiles but returns `Unsupported` rather than persisting secrets insecurely.

### `ui-debug`

Qt Quick debug surface with tests. Requires Qt 6.12+ and must additionally pass strict QML lint and warning-fatal smoke verification. Use `cmake --preset ui-qt612-debug` and `cmake --build --preset ui-qt612-debug` on the Genycaster build server to select the version-pinned `/opt/genycaster/Qt/6.12.0/gcc_64` host toolchain and GCC 16. The `ui-debug` preset is retained for generic development, but it also requires Qt 6.12 or newer; do not reuse a Qt 6.11 CMake build directory because compiler module artifacts are toolchain-specific. The preset explicitly compiles the Hypersnap adapter because it is the only implemented live-read UI provider in the current checkpoint.

### Fresh IDE/default UI configuration

`FARCASTER_BUILD_UI=ON` now defaults `FARCASTER_ENABLE_HYPERSNAP_PROVIDER=ON`. Core-only presets still set both UI and provider support OFF explicitly. A runnable UI with provider support explicitly disabled fails at configure time instead of producing an executable whose first read can never route. Strict-self-hosted UI composition is also rejected until a self-hosted read adapter is implemented; the provider-free strict core preset remains valid.

## Future provider-policy profiles

Once real adapters exist, deployment policy may express:

```text
Standard:
  preferred protocol provider allowed
  self-hosted providers allowed
  compatibility providers optional

Private:
  privacy-acceptable protocol providers only
  compatibility providers disabled unless explicitly allowed

Strict Self-Hosted:
  public managed providers disabled
  compatibility providers disabled
  self-hosted/local services only
```

## Required future provider independence tests

When the relevant adapters exist, CI must prove removal/disablement without core redesign:

```text
NO_MANAGED_BACKEND
NO_HYPERSNAP
SELF_HOSTED_ONLY
```

Until those adapters exist, the stronger Phase 2 statement is that the core has **zero production provider implementation dependency at all**.

## Missing capability behavior

If no eligible provider exists for an optional capability:

- unrelated functionality continues;
- local cached state remains available;
- UI shows unavailable/disabled state where applicable;
- no hidden fallback to a managed backend is permitted.
