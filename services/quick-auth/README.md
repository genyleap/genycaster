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

**Integration boundary**: Native Genycaster desktop sign-in has two independent
routes: OpenProof OAuth/device authorization for Genyleap accounts, and a
Farcaster Relay SIWF ceremony whose EIP-191 signature and FID ownership are
verified against Optimism in the native client. This internal Quick Auth
sidecar verifies Mini App protocol tokens; its response **cannot** substitute
for an authenticated desktop session, signer provisioning, or FID custody.
Do not treat an arbitrary supplied FID as a credential.
\`sdk.protocolAuth.getToken()\` exists in Farcaster Mini Apps, not
automatically in a Qt native process. Native callers need an authorized
wallet-driven proof ceremony, rather than executing JS in QML.
