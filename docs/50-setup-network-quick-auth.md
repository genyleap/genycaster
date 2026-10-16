# Genycaster dual-network Setup: implementation notes (2026-10-16)

## Authorities (never conflate)

1. OpenProof issuer/subject plus linked FID establishes the verified native application session.
2. Farcaster Quick Auth validates a signed wallet's relationship to a FID; it does not establish an OpenProof session or provision an FID.
3. A separately provisioned Snapchain signer authorizes protocol writes. Protocol Quick Auth alone never authorizes publishing.

## Network preference

The initial Setup and the signed-in Language and region settings allow Farcaster or Hypersnap selection. This persists under connection/preferredNetwork, defaults to hypersnap, and is limited to these two validated strings. It is not a global identity toggle: switching does not erase FID, signer secrets, local caches or verified session state.

The live capability router uses atomic preference affinity only after deployment, capability and health eligibility checks. Hypersnap v2 is the only current registered protocol read adapter. On Farcaster preference, the sign-in method goes through existing OpenProof Farcaster sign-in; protocol reads continue on Hypersnap until a Farcaster read adapter is registered. Writes still use Snapchain. **A fully operational network data-source switch is not yet available.**

## Profile handling

Existing profiles follow the current OpenProof PKCE + linked FID ceremony. New profiles open the verified Farcaster signup page (https://farcaster.xyz/~/signup), which routes users to a compatible app, and offer a return-to-connect action. Genycaster currently has no native FID registration implementation. QML never asks for seed/recovery material.

## Quick Auth sidecar

services/quick-auth pins @farcasterorg/quick-auth 0.2.0, with Node 20+ and localhost-only HTTP. An internal caller sends POST /v1/protocol-auth/verify with the Authorization Bearer token and its private X-Genycaster-Internal-Secret; accepted proofs return fid/method/address, rejected proofs return HTTP 401, and hub failures HTTP 503. The domain must be explicit. The verifier handles sdk.protocolAuth.getToken() wallet-signature tokens and classic sdk.quickAuth.getToken() JWTs. No signing keys or persistent user sessions are stored.

**Not yet linked to native authentication:** The OpenProof backend must verify the protocol proof against its logged-in subject and linked FID and enforce anti-replay/session controls before changing account state. This opt-in verifier is intentionally not auto-started or public-facing. Qt native does not automatically have the Mini App JS SDK; native wallet proof acquisition requires a separate authorized integration.

## Validation

- npm ci --ignore-scripts, node --test services/quick-auth/handler.test.mjs
- cmake --preset ui-debug -DCMAKE_PREFIX_PATH=/opt/Qt/current/gcc_64
- cmake --build --preset ui-debug and ctest --preset ui-debug
- Render signed-out Setup on small/wide RTL/LTR windows with both account intents, language changes, cancel and error states, and verify preference persistence after restart.
- Test healthy route affinity, absent Farcaster adapter fallback, ineligible-provider policy, offline/read retry, and unchanged signer/account authority.
- On hosts without QtKeychain >=0.17, signing must remain unavailable (no plaintext fallback).
