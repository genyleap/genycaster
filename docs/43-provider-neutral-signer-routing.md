# Routed signer-key management (increment)

## Capability contract
- Responsibility: route signer lookups, gasless KEY_ADD and self-revocation KEY_REMOVE through an explicitly registered, capability-eligible protocol writer.
- Origin: Farcaster protocol, with no SDK assumptions in Routing or Application.
- Mandatory/optional: signer management remains optional; guest read mode works without it.
- Public contract: `capability::signer::ISignerRegistry` and `routing::signer::RoutedSignerRegistry`.
- Canonical invariants: only the secure vault holds private keys; the selected writer must explicitly declare `protocol.signer.manage` with Write access. Signer management never inherits permission solely from `protocol.message.submit`.
- Dependencies: provider-neutral Router, Registry, signer capability contract. Concrete signer adapters exist only in composition.
- Failure modes: missing adapter; unknown/unsupported provider; health or deployment policy rejection; writer-only provider lacking signer contract. All fail closed without falling back to another signer adapter.
- Local state: existing signer metadata, registered state and signed-key lifecycle. No private key or URL is added to the domain.
- Offline: provider unavailability blocks protocol key changes, not viewing cached content.
- Provider policy: the same Write affinity as signed Cast/Reaction/Link submission applies to KEY_ADD/REMOVE and signer reads; the signer-specific capability is evaluated separately.
- Failover: no fallback after a selected adapter is chosen. Each blocking signer operation delegates only to its selected adapter and retains that adapter through that method invocation.
- Tests: `routing.signer_provider_selection`, writer-only capability rejection, policy/health fail-closed behavior, pre-composition readiness, previous provider signer contract tests.

## Composition

`RoutedSignerRegistry` is initialized before SignerService so it can be passed
through the existing provider-neutral ISignerRegistry contract. Registered
adapter instances are owned by Runtime, and source pointers remain valid
through all signer operations. Router is injected after all provider
registrations are installed but before the runtime is returned.

Each configured Snapchain-compatible writer instance receives its own
SignerRegistry adapter with its own **read origin** and **write/validator origin**.
No read credentials are automatically propagated to another write host.
The application uses the selected `protocol.signer.manage` capability
(and `CapabilityAccess::Write`) for inspection, registration and revocation.
Only configuration advertises which protocol adapters are installed.

For example, when provider `operator-west` is selected for Write and
advertises both `protocol.message.submit` and `protocol.signer.manage`,
signed Casts and signer key mutation can target the same operator, with
independently configured read/reconciliation URLs. A writer-only provider
can still publish signed messages but cannot silently receive signer
lifecycle calls.

## Important remaining work

- This increment does not introduce a native indexed Snapchain feed adapter
  or a general multi-protocol schema-discovery service.
- No automatic trusted proof of self-hosted status is inferred from a URL.
- Signer KEY_ADD/REMOVE require live wallet authorization and provider-specific
  semantics. Unlike ordinary canonical write outbox messages, the current
  `SignerService` does not persist a per-registration target across a process
  crash. Production switching during an in-flight wallet-signing workflow
  must remain disabled until route locking/migration is designed.
- The UI currently requires explicit provider profile configuration;
  it does not yet expose a full provider directory and policy editor.
- Cross-device / two-independent-validator end-to-end checks and complete
  Qt/WASM release builds remain necessary for an architecture-complete claim.
