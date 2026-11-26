# Public node discovery and routing (v1)

Settings → Node Management and Setup share ProviderProfilesController's directory.
The independent worker serves GET/HEAD /api/v1/nodes, /status and per-node details.
Its operator-owned config may add, remove or disable records and must increment
revision for changes. Registry entries declare read interfaces, not write authority.

The manifest carries an Ed25519 signature over the exact base64url payload.
Native signing builds use OpenSSL; WASM uses browser WebCrypto. Builds without
a verifier or unsupported browsers fail closed to embedded origins. Only the
signed payload is parsed for admission. A valid, unexpired cached manifest
is accepted offline; otherwise only the existing configured connections are used. The client checks
schema, revision, timestamps, origin shape and duplicate IDs. Cached data is
not a guarantee of connectivity. Server observations are advisory, separate
from device-local registration preflight and local read checks.

The default public HyperSnap read configuration registers enabled directory
peers at Runtime startup. The existing capability router can choose them for
future safe reads after a circuit opens. Custom pins, strict self-hosted mode,
credentials and operation-specific authority preferences are not bypassed.
Existing provider-bound cursors are not forwarded to another node. New peers
require Runtime recomposition (currently a restart), not a new binary.
Manual selection and write routes stay independent.

Startup runs bounded, parallel registration preflights of origins independently approved by signed registrationAllowed policy and enabled in the registry: valid nativeRegistration challenge,
shard-0 delay <=120 seconds and real nativeAccount lookup of a fixed public
probe address. Status controller, publisher and native runtime are then bound
to the same origin. Pending registration journals retain their origin across
restart. New directory entries default to registrationAllowed=false.
No automatic retargeting of a running registration or signed write is implemented.

Deployment: install node_directory.mjs and node_directory_start.mjs into
/opt/genycaster/node-directory-service with executable-readable files. Store
the Ed25519 key as directory-key.pem with mode 0600; its public key must match
the pinned client key. Never regenerate the key without a coordinated client
trust update. Install the systemd unit example and Nginx locations with admin
access, validate nginx -t and check public endpoints. The worker binds only
127.0.0.1:8799. The operator-owned nodes.json is the sole public inventory. Optional GENYCASTER_NODE_DIRECTORY_FILE overrides its location; increment revision and restart the worker after updating it. GET /api/v1/nodes?format=2 includes raw HyperSnap monitoring-only records and HTTPS port 3381 Farcaster readers. The legacy manifest omits these to avoid rejecting the entire directory on older clients. Do not
send private credentials to the directory or put the private key in Git.

Remaining: hot admission/removal of runtime peers, automatic safe pre-registration
recomposition after runtime startup, full backend migration, per-capability live
read-route reporting and end-to-end Android/WASM validation.
