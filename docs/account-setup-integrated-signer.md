# First-run Setup and protocol signer lifecycle

## Contract
- Only a user-initiated sign-in from the visible first-run Setup arms the
  one-shot signer flow. A verified authentication challenge must be observed
  before its successful completion. Session restoration does not arm it.
- Browser: first restore an existing origin-bound signer, verify its Snapchain
  registration, and show the recovery phrase dialog only when no valid signer
  is registered. Never send the mnemonic across the Qt/WASM boundary.
- Native: restore the secure-vault signer before attempting one scoped provision
  for the verified FID, only if the signer is missing or requires authorization.
- An error, cancel or sign-out disarms the flow. Account switching invalidates
  in-flight browser signing. Signer registration is never inferred from login.
- A failed or incomplete KEY_ADD does not enable writes. Recovery and
  authorization can be retried explicitly in Setup/Account Settings.
- Users can continue reading without enabling interaction privileges. This
  flow does not persist or auto-reuse imported custody credentials.

## Security and rollout
The browser explicitly requests a Farcaster-only phrase, and custody address
must match the authenticated FID on Optimism. Only a restricted Ed25519
signer remains after authorization. Browser JavaScript strings cannot be
deterministically wiped, so the origin/device security warning remains
material. Do not ship as audited custody-key handling solely on the basis
of static guards; test desktop secure storage and a real Qt 6.12 WASM
KEY_ADD confirmation with disposable credentials before production.
