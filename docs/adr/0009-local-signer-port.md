# ADR 0009 — Local Signer Is a Platform-Safe Port, Never Raw-Key Material

- Status: Accepted for Phase 5

## Context

Protocol-native writes require an Ed25519 signature over canonical signed bytes. If
private keys are strings/copies passed through Application, QML, logs, configuration, or
ordinary SQLite, the security boundary collapses and there is no place to later plug in
Apple Keychain, Secure Enclave, Android Keystore, or Windows/Linux credential stores.

## Decision

Define a small provider-neutral signing port that accepts canonical message bytes and
returns only a completed signature plus a stable signer identifier:

```text
INativeSigner.sign(SignedBytes bytes) -> Result<CanonicalSignature>
```

The port carries no raw key bytes, never signs QML-provided control data directly, and its
implementation owns key retrieval and storage. The canonical bytes to sign are produced by
the message builder; signatures are deterministic test vectors in tests, not real keys.

## Consequences

- raw private keys never enter Domain, QML, logs, provider config, or outbox tables;
- platform-secure adapters implement the port without redesigning Application or Domain;
  see [ADR 0019](0019-independent-authentication-authorities.md) for current Apple,
  QtKeychain and browser key separation;
- tests use fixed deterministic signing vectors and never require real secrets;
- signature verification is independent of the submit transport that carries it.