# ADR 0016 — Protocol-scoped account registration

**Status:** Accepted; HyperSnap native submission is implemented with guarded registry confirmation. Farcaster/Neynar relay is opt-in and not yet a complete user-facing paid enrollment.

## Context

GenyCaster reads and writes through configurable Farcaster/Snapchain and
HyperSnap services. A **read provider** is not an identity registry, and
switching read providers cannot create or convert a FID.

HyperSnap Native registration creates an identity in the HyperSnap native
registry (initially via `NATIVE_ONBOARD` and Hashcash). Farcaster registration
creates or transfers an identity in the Optimism `IdRegistry`; Neynar is an
optional API/transaction relayer for that Farcaster registration, not a
separate identity network. Numeric FIDs cannot safely identify the registry
without an explicit network tag.

## Decision

Use the following ownership layers:

| Component | Ownership |
| --- | --- |
| `NativeCustodyWalletController` | Shared, local-only EVM custody key generation and backup confirmation |
| `farcaster.platform.custody_recovery` | Shared private-key derivation, domain-bound EIP-712 signatures, zeroization |
| HyperSnap native registration | HyperSnap-only anchor, Hashcash, `NATIVE_ONBOARD`, shard-0 confirmation |
| Farcaster onchain registration | Optimism-only `IdRegistry` nonce, transfer signature, onchain custody confirmation |
| Neynar registration adapter | Public reserved-FID request body; first-party server holds API key and `x-wallet-id` |
| `registration_plan.hpp` | Explicit, tagged route preconditions and network-specific confirmation gates |
| Signer service | Shared signer/vault infrastructure, with network-specific authorization and verification |

The onchain `Transfer` EIP-712 domain is `Farcaster IdRegistry`, version
`1`, chain ID `10`, verifying contract
`0x00000000fc6c5f01fc30151999387bb99a9f489b`. It MUST NOT be
substituted with the HyperSnap `HypersnapNativeOnboard` domain.

There are **two different Neynar API registration styles**:

1. Legacy reserved-FID transfer:
   first-party server obtains a fresh FID reservation via
   `GET /v2/farcaster/user/fid` using its `x-wallet-id`; wallet signs the
   exact `Transfer` (fid, to, nonce, deadline); server forwards the signed
   public request to `POST /v2/farcaster/user/`. Reservation is time limited
   to **10 minutes** and must use the same app wallet. The server must query
   the canonical IdRegistry nonce, verify signature and custody, rate-limit
   and authenticate requests, and confirm inclusion.
2. The newer `POST /v2/farcaster/user/register/` has a different `registration`
   request shape, optional `signers` and `storage_units`, and must receive a
   separately specified/verified authorization, **not** the legacy
   `Transfer` signature by assumption. Do not silently exchange these.

`src/platform/custody_recovery.cpp` currently contains two explicitly
domain-bound signing primitives because they share the same ephemeral
OpenSSL-backed custody derivation. Higher layers must not interchange them.
Protocol adapters and DTOs remain distinct.

## Safety invariants

- A BIP39 phrase or derived key stays inside trusted native custody storage.
  No seed/phrase/private key is serialized to QML, telemetry, QSettings, a
  request body, or Neynar.
- Neither a read-API success nor a 200 response from a registration API
  establishes a completed identity. Confirm the precise (network, fid,
  custody-address) tuple from **that network's authoritative registry**.
- Native registration requires an active node, a fresh committed anchor,
  sufficient PoW and a domain-bound native custody signature.
- Neynar registration requires a verified fresh FID reservation, IdRegistry
  nonce, bounded deadline, authenticated and rate-limited first-party
  transaction relayer, and onchain custody confirmation.
- The first-party server may hold its Neynar API credentials and app-wallet
  identifier, never the user's custody private key.
- The device signer must be network-registered and permission-verified before
  treating the newly created account as ready for writes.
- Cancellation or setup dismissal erases any unpersisted wallet seed.
  Users must confirm their offline recovery backup before a broadcast.
- The selected read provider remains an independent setting. Switching
  read nodes never changes the account's registry provenance.

## Implementation status

The shared wallet generator/backup, HyperSnap PoW/EIP-712 and protobuf
encoding, Farcaster onchain reserved-FID Transfer EIP-712, and public Neynar
payload validation have separate native regression tests.

The Qt 6.12 HyperSnap-specific transport has now been implemented and tested
against an in-process HTTP hub: `GET /v1/nativeAccount?custody=...` distinguishes
unregistered, unavailable and confirmed (FID, custody and block number must
match); `POST /v1/submitMessage` sends the canonical **binary** protobuf with
`Content-Type: application/octet-stream`. A successful HTTP 200 represents
**accepted/pending** only, never a confirmed identity. Ambiguous failures do not
automatically replay the request.

The C++ native registration preparation controller also has a tested,
cancellable QtConcurrent PoW worker with a bounded hash budget. Only public
custody address, anchor and difficulty enter the worker. The mnemonic is
read exclusively inside the secure wallet's C++ signing function. A recent
anchor, verified offline backup, and verified *no existing native account for
the same address* are mandatory native preconditions. Prepared unsigned
envelopes remain C++-only and expire after 60 seconds. Setup can check for
a pre-existing FID, prepare a proof, cancel/refresh, and return without leaving
the application; it never exposes signing material to QML.

HyperSnap now has an explicitly consented publish coordinator, independent
native FID confirmation, its own custody recovery path and short-lived
session/one-shot device signer authorization. Local C++ and Qt tests pass;
a funded mainnet onboarding, signer network acceptance and full cross-platform
package are **not** yet independently verified.

The Farcaster/Neynar first-party backend now includes Optimism IdRegistry
safe-chain reads, independent EIP-712 signature recovery, sponsored/paid
voucher authorization bound to custody, a PostgreSQL v13 atomic lease,
no-replay Neynar requests, and onchain confirmation. It remains **disabled
by default** and requires a funded Neynar app wallet plus trusted, external
voucher issuance after actual payment/sponsorship verification. The default
account API must never hand a free public FID allocation to anonymous callers.
See `services/account-sync/NEYNAR_REGISTRATION.md` for the operational contract.

Qt Farcaster registration screens, verified payment issuance, final Signer
onboarding and a full production enrollment on Optimism are still outstanding.
Do not mistake successful mock tests for real mainnet account creation.

## References

- [Farcaster contracts/IdRegistry](https://github.com/farcasterxyz/contracts/blob/main/src/IdRegistry.sol)
- [Neynar wallet integration registration flow](https://docs.neynar.com/docs/guide-to-creating-a-farcaster-account-with-wallet-integration)
- [Neynar reserved-FID registration API](https://docs.neynar.com/reference/register-account)
- [Neynar combined onchain registration API](https://docs.neynar.com/reference/register-account-onchain)
- [HyperSnap native registration](https://github.com/farcasterorg/hypersnap/blob/main/src/native_registration.rs)
