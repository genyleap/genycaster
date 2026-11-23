# ADR-0018: Smart Media Pipeline — optimized image storage

**Status:** Smart Media Pipeline deployed; 2/5 MiB entitlement limits and cancellation updates require the subsequent backend rollout; 2026-11-23

## Decision

Smart Media Pipeline is the image optimization boundary attached to the
existing Media Gateway. The primary requirement is **lower QStorage retained
bytes**, not merely lighter previews.

- For images, the default server upload policy is **Auto**.
- A signed `Original` mode preserves exact bytes for a future user setting.
- The backend stages only a bounded amount of raw input in volatile memory,
  validates/re-encodes it with Sharp/libvips, then writes **one canonical
  optimized object** to QStorage.
- Existing URL/multipart/ownership contracts remain stable.
- Public delivery can optionally resize an already-optimized canonical image,
  with on-demand bounded cache instead of permanent extra cloud variants.
- Videos use the existing direct multipart path.
- On-device source preparation accepts up to 25 MiB / 32 MP; only the
  compressed network bytes count against the Basic 2 MiB and Pro 5 MiB
  gateway limits. Only verified Pro
  subscriptions from the first-party account database permit the larger
  size. A dedicated timestamped HMAC backend-to-backend request enforces
  this independently of the client. Unavailable verification fails closed.
- CPU-rendered square Stats cards remain original PNG at 1920×1920 to preserve
  legibility; ordinary images remain automatically optimized.

## Social sharing of stats

The stats card has a square 1:1 composition and a high-fidelity 1920×1920
original PNG, rendered once with the account avatar and reused for both
preview and Cast embed. If a configured avatar cannot be loaded, image
sharing is blocked with a retry instead of publishing an ambiguous placeholder.

The stats dialog offers a native Genycaster Cast with image and readable
fallback text, X/Telegram **text** share intents, and **Share image**.
On WebAssembly Share image uses Web Share with the real PNG when the browser
supports file sharing, otherwise offers a local PNG download. On desktop it
exports the PNG to Pictures and opens it with the system image viewer; there
is no claim of automatic cross-app publishing. Instagram is available only
where the operating system's installed-app Share Sheet offers it. No user's
social account credentials are collected by Genycaster.

## Rationale

Preserving every original indefinitely in QStorage and merely resizing at
read time does not reduce storage charges. Persisting original+optimized
also doubles cloud-side duplication and cleanup obligations. Instead, the
source exists temporarily only in the gateway, while cloud-side storage
contains exactly one final image per managed asset.

## Operational invariant and migration

- Old v2 in-flight signed upload tickets remain valid for direct multipart
  completion. New image tickets are v3 even for legacy clients that omit the
  optional policy; response `mode: gateway-multipart`, source-sized progress
  accounting, FID/signer binding and opaque IDs do not change.
- Original mode deliberately produces a v2 ticket until a versioned
  replacement is justified.
- `sizeBytes` in completion remains the received source size for backward
  compatibility. `storedSizeBytes` is the real QStorage byte count.
- Source MIME and final MIME are identical to avoid extension/URL
  mismatch in existing Cast embeds.
- No user-visible quality selector is introduced by this ADR.
- Existing QStorage images are **not retroactively transformed**; migration
  would require a separately authorized, auditable lifecycle job.

## Failure modes

- Unrecognized MIME, invalid raster bytes, decompression bomb or excess
  animation frames: reject before storage.
- Codec unavailable, processing backlog full, input memory budget exceeded:
  fail with typed, retryable errors, do not quietly upload the original.
- Provider upload error: attempt multipart abort; pending volatile session
  may retry until its signed TTL expires.
- Cancellation: no object is written if still staging; once finalization
  starts, cancellation cannot revoke a transaction that may have committed.
- Container restart during staging: incomplete session disappears. Client
  must repeat the upload; the permanent asset URL is only authoritative
  after completion is acknowledged.
- A lost completion response can be retried with a fresh signed proof. A
  bounded metadata-only receipt prevents duplicate QStorage writes; a
  subsequent signed abort deletes the finalized object if it was not used.
- Animation uses lossless pass-through where frame-correct transcoding
  is unavailable, not an accidentally flattened single-frame preview.

## Performance and privacy

- Bounded gateway staging memory: 64 MiB total / 3 sessions; source
  preparation on the client is bounded to 25 MiB, and the network image
  upload is capped at 2 MiB (Basic) or 5 MiB (Pro); pixel decode cap 32 MP.
- Serialized transformations, bounded queue, validation of MIME signatures.
- Raw media and EXIF are not logged. Re-encoded images omit EXIF/GPS by
  default; Original mode preserves original metadata intentionally.
- Full-frame thumbnail/feed/full derivatives are transient. No third-party
  URL ingest, no user-selected arbitrary resize dimensions, and no private
  provider credentials in clients.
- Security still depends on signed proofs and existing owner-scoped
  management tokens.

## Verification and deployment

The module has offline integration tests covering the signed upload path,
stored byte reduction, exact original fallback, invalid-input refusal,
multipart signature checks, memory reservation and canonical GET compatibility.
Docker installs the pinned Sharp/libvips runtime dependency.

Backend code being committed is **not** equivalent to a live rollout.
Deploying Media Gateway, native/web client release builds, and a visual
cross-client check are separate release gates. The user explicitly requested
no automatic client builds or site publication.
