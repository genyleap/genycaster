# Public node discovery and routing (v1, experimental)

The existing Settings → Node Management owns discovery and provider profile editing.
The health worker serves GET/HEAD /api/v1/nodes, /status and per-node details.
The directory records all four requested public origins; a successful read probe is
not proof of a working signer, validator, or registration submission.

The server may sign exact manifest bytes using an Ed25519 private key supplied by
the operator. Until a pinned verifier is provisioned, clients only recognize
compiled origins; untrusted directory records cannot expand the routing pool.
The embedded directory remains available when the service is offline.

For the default public HyperSnap read configuration, the existing capability
router may select the independently verified ImgNai read adapter after the
primary circuit becomes ineligible. Explicit custom pins, credentials, strict
self-hosted mode, and operation-specific identity affinity are not bypassed.
Existing continuation tokens remain provider/origin scoped. Start a new page
sequence on the new node; never forward the previous provider's opaque cursor.
No same-request retry of an uncertain write is introduced.

Startup runs parallel, bounded read-only HyperSnap registration preflights:
valid nativeRegistration challenge, shard-0 delay <= 120 seconds, and a real
nativeAccount query with a fixed public probe address. Before Runtime starts,
the status controller, publisher and native signer/registry configuration are
bound to the same selected origin. The preferred origin wins if ready. Existing
pending registration journals retain their previous origin across restart.
No PoW, custody secret, signed envelope, or POST is sent by preflight.
This does not implement automatic rebind of a running registration transaction.

Deployment requires the health worker and nginx path, operator signing key,
and verification of WASM CORS and real on-device registration. Dynamic admission
of arbitrary remotely introduced hosts, signed client manifests, per-capability
live active route display and full backend migration remain separate follow-up work.
