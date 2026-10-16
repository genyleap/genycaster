# Protocol Quick Auth verifier (internal sidecar)

This is an opt-in internal verifier for \`@farcasterorg/quick-auth@0.2.0\`.
It validates both protocolAuth EIP-4361 wallet tokens and legacy Quick Auth JWTs
against the configured audience domain and the Farcaster protocol hub (when applicable).
It does **not** provision an FID, log in a Genycaster native client, replace
OpenProof OAuth/PKCE, manage signers, or grant publishing permissions.

Run \`npm ci\` after generating and reviewing the package lock. Set
\`GENYCASTER_QUICK_AUTH_DOMAIN\` (bare relying-party hostname),
\`GENYCASTER_QUICK_AUTH_INTERNAL_SECRET\` (random 32+ char secret),
optionally \`GENYCASTER_QUICK_AUTH_HUB_URL\` and
\`GENYCASTER_QUICK_AUTH_PORT\`. Run \`node server.mjs\`; listening
is localhost-only.

Send POST \`/v1/protocol-auth/verify\`, with
\`Authorization: Bearer <protocolAuth.getToken() token>\` and
\`X-Genycaster-Internal-Secret\`. On success returns only verified
\`{fid, method, address}\`. Verification failure is 401; missing/unreachable
hub is 503, so callers can retry without silently falling back to trust.

**Integration boundary**: The native app's canonical session currently
requires OpenProof issuer+subject and a linked FID. The trusted OpenProof
integration must perform the FID link/challenge and correlate the proof to
its authenticated subject before committing a session. Do not make this
HTTP verification response into a native session or bearer credential.
\`sdk.protocolAuth.getToken()\` exists in Farcaster Mini Apps, not
automatically in a Qt native process. Native callers need an authorized
wallet-driven proof ceremony, rather than executing JS in QML.
