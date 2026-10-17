# First-run Setup and signer permissions

## Account paths

1. **Full-access recovery (WebAssembly):** first-run Setup asks for the
   Farcaster 12/24-word phrase in an indexed, numbered, local-only form. The
   custody address is derived in the browser, checked in Optimism IdRegistry,
   and its FID is bound to a scoped signer that must be confirmed by Snapchain
   before full interaction privileges become ready.
2. **Viewer sign-in (WebAssembly and native):** Farcaster SIWF and
   Genyleap/OpenProof login establish a verified identity only. The app can
   display posts and profiles, but cannot post, follow or react unless a
   previously authorized signer is separately restored and verified.
   An ordinary sign-in does **not** prompt for the phrase or automatically
   provision a new signer.
3. **Upgrade later:** Account exposes Enable Publishing/Interactions when
   authenticated without a ready signer. In the web build this explicitly opens
   the local recovery phrase dialog. There is no custody mnemonic storage or
   auto-grant after guest sign-in.

## Session vs onboarding

- The six-step onboarding completion and legal consent are non-secret durable
  preferences. Sign-out, refresh, session expiry, and signer revocation never
  reset them.
- Browser sessions remain ephemeral. Refresh may return users to guest viewing,
  and it must not silently re-enable publishing without a new verified
  authenticated identity and independently restored signer registration.
- Sign-out immediately clears write readiness. Late signer or recovery events
  from an earlier identity must not re-enable writes or cause a new KEY_ADD.
- Browser phrase input and derived custody key never cross the Qt/WASM bridge,
  nor enter QSettings, IndexedDB, localStorage, logs or network payloads.
  Numbered fields are cleared on cancel/submit; JavaScript strings cannot
  guarantee deterministic memory erasure.

## Rollout

This is not a security audit or proof of production readiness. Validate a
real Qt WebAssembly build, device storage, a disposable Farcaster custody
account's KEY_ADD and write confirmation, mobile on-screen keyboard and HTTPS
deployment before releasing phrase-based recovery publicly.
