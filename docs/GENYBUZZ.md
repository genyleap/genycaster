# GenyBuzz — application-owned attention signal (v1)

GenyBuzz is a **GenyCaster product capability**, not a protocol operation.
It does not publish a Cast, direct message, signature, notification message on
HyperSnap/Snapchain, wallet transaction, or OpenProof identity mutation.

## Boundaries

- **OpenProof/verified account session:** verifies sender ownership. Native
  HyperSnap registration uses its separately verified account-session issuer.
- **GenyCaster account API:** owns consent, throttling, event storage,
  acknowledgement and private inbox.
- **UI:** `profile` currently triggers a nudge; `dm` is reserved as a future
  entry context into the **same** service, not a separate backend transport.
- **Delivery:** currently first-party authenticated app polling (30 seconds
  while visible). Push/background notification, cross-client interoperability,
  audio and haptics are not promised by v1.

Target accounts are identified by a trusted issuer and verified account
subject. The client passes only a target FID, a recognized account-kind hint
(`farcaster` or `hypersnap`) and a bounded source context. The server derives
the trusted target issuer. HyperSnap-native FIDs must be in the native registry
range; this **only disambiguates account identity** and never constitutes a
protocol signature or cross-protocol DM.

## REST (all routes authenticated)

| Route | Method | Effect |
|---|---|---|
| `/api/v1/cloud/buzz/preferences` | GET | Private opt-in state, including revision |
| `/api/v1/cloud/buzz/preferences` | PUT | `{enabled:boolean,revision:integer}` with optimistic concurrency |
| `/api/v1/cloud/buzz` | POST | `{target_fid:string,target_kind:string,context:"profile"\|"dm"}` |
| `/api/v1/cloud/buzz/inbox` | GET | At most 20 unacknowledged, unexpired events |
| `/api/v1/cloud/buzz/ack` | POST | `{id:uuid}`, recipient-owned acknowledgement |

These routes use the same bearer/cookie+CSRF safeguards as other account
mutations. No anonymous send, GET with side effects, user-submitted issuer,
client-provided sender FID, or subscription privilege is accepted.

## Consent and abuse policy

- **Off by default.** Missing settings row means reception is disabled.
- Recipient must explicitly enable it. No public availability endpoint exposes
  another user's preference or registration status.
- A recipient who muted the sender, or a sender who muted the recipient, cannot
  send. Both app-owned mute sets are checked on the server.
- A sender may send at most **one Buzz per recipient every 60 seconds**,
  **five total per hour**. A recipient may receive at most **ten per hour**.
  Limits are transactionally serialized with ordered PostgreSQL advisory locks.
- Disabled users do not receive polling results. DND/system notification rules
  are not bypassed: v1 uses only an in-app, dismissible notification.
- Events expire from inbox queries after **24 hours**. Expired event rows
  require scheduled/operational cleanup (see maintenance note below).
- The event contains only verified sender account identity and context, no
  arbitrary message body or untrusted links. There is no Pro-only bypass.

Mute is an app-owned policy. Provider-level blocks that are not reflected
in this policy are **not yet** a server-enforced nudge blocklist; before
general-audience rollout, add explicit block synchronization or an additional
product-owned sender allowlist.

## Migration and rollout

1. Deploy account API code and **apply** `014_genybuzz.sql` as an additive
   PostgreSQL migration (using the authorized DB-admin role). The service
   replies `503 buzz_unavailable` until schema v14 is detected on restart.
2. Verify the API's authorization and consent behavior with the supplied
   Node tests; confirm sender/receiver accounts separately for each supported
   identity issuer.
3. Deploy corresponding native/WASM clients. In-app notifications do not work
   with an older, unupdated client.
4. Schedule server-side cleanup, e.g. delete expired `genybuzz_events` rows
   in bounded batches through a restricted maintenance role. Expiry is enforced
   by all reads independently of cleanup.
5. Test account switching, logout, muted recipients, concurrent send limits,
   responsive Pro/GenyBuzz menus, low-motion mode, and both RTL/LTR interfaces.

The server does not automatically run schema migrations or enable reception.
Do not advertise remote delivery until a live two-account test passes.

## Future DM integration

A DM view may call `sendGenyBuzz(targetFid,targetKind,"dm")` through the
existing Qt controller. The contract remains identical. A later chat-specific
consent policy can additionally verify conversation membership before a
`dm` entry is permitted, without modifying the storage/event model.

No DM content, message receipts, presence or protocol key material should
ever enter this feature.
