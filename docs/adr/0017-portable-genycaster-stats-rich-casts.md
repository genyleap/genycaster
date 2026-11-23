# ADR-0017 — Portable Genycaster Stats Rich Casts

**Status:** In progress, source implementation (23 Nov 2026)

## Classification and responsibility

- **Responsibility:** Share a measured account-Stats snapshot as a branded,
  readable social post without introducing a proprietary protocol message.
- **Capability origin:** Derived application analytics plus local presentation;
  canonical Cast publishing remains protocol-native.
- **Optionality:** Optional Genycaster Pro share workflow. Reading the public
  post is not paywalled and is independent of the creating account's Pro status.
- **Public contract:** The existing Farcaster/HyperSnap Cast text and one
  standard image URL Embed. Genycaster's first-party authored format uses the
  plain-text header `Genycaster Stats · <period>` and a literal
  `Powered by Genycaster` footer. Other clients need not implement anything.
- **Internal handoff:** `genycaster.stats/v1` identifies the UI-produced
  draft bundle (snapshot caption and a temporary PNG to be uploaded). It is not
  a new wire message, an on-chain property, or an externally verifiable proof.

## Canonical data and invariants

1. Snapshot metrics come from the loaded `StatsController` summary for the
   authenticated account and selected range. Clone them on opening the preview.
2. Missing engagement remains unavailable (represented by an em dash), not
   fabricated as zero. Engagement reflects responses counted to date on
   posts from the period, not a historical time series.
3. The card is a CPU-rasterized 960x600 PNG using QImage/QPainter and the
   official vector Genycaster mark. Its QML preview displays that *exact*
   file rather than a second rendering. This bypasses Qt Quick/WebGL
   grabToImage callback stalls (which previously left Preparing… indefinitely).
   The text caption repeats key quantities, including a deliberate blank
   line before its Powered by footer, even if the image is unreadable.
   The author reviews it in the normal Composer before granting publication.
4. The existing signer-authorized Media Gateway creates the sole stable public
   image URL. The source temporary file path is never embedded in a Cast.
5. Upload progress blocks publishing until the image is attached. If capture or
   upload fails, the user sees an error and can retry or choose text-only.
6. The Cast's canonical protocol text and image remain intact. Genycaster may
   decorate matching posts with a visual `Genycaster Stats` label, but this
   label **does not verify** the metrics, author, image origin, or a server
   signature. Any user may type the same public text format.

## Boundaries, failure and offline behavior

- **Dependencies:** StatsController (derived data); Qt Quick image capture
  (presentation); existing MediaUploadController, signer and media gateway
  (application/transport); canonical Cast writer.
- **Local state:** A transient snapshot and an owned temporary PNG for the
  share attempt. Unsent/previous style files are deleted on cancellation,
  switching styles, completion or adapter destruction. There is no new
  database or duplicated social graph.
- **Expected failures:** Stats unavailable; Pro entitlement unresolved;
  CPU rasterization/PNG encoding failed; temporary filesystem unavailable;
  user signer unavailable; media upload cancelled, timed out or refused;
  publishing rejected or ambiguously acknowledged. No indefinitely pending
  grabToImage callback exists. A failed attachment visibly offers Retry or
  deliberate Text only, without silently publishing an incomplete Cast.
- **Offline:** Existing cached Stats may remain visible, but publication and
  upload require connectivity. No Cast is automatically published later as a
  side effect of preparing its image.
- **Provider policy:** HyperSnap/Farcaster writes continue through the
  canonical writer's existing capability routing; no direct QML provider call.
  No write fallback or resubmission is introduced.
- **Failover:** Capture failure leaves the share preview open. User-directed
  text-only sharing is available. Media failure remains visible in Composer,
  with its existing media retry and publication controls. No automatic
  alternate protocol submission occurs.

## Verification gates

- `tests/wasm/rich_stats_cast_contract_20261123.test.mjs`
- `tests/wasm/home_stats_market_quality.test.mjs`
- `ci/check_media_upload_contract.py`
- CMake/WASM compilation and visual desktop/browser regression checks are
  distinct release gates, not implied by passing the source tests.

## Next evolution

Once cross-client static image visibility is qualified, a separately
versioned immutable public metadata service may be added for richer native
interactive renderers. That future record must be bound to the confirmed Cast
hash and author FID with verifiable provenance; do not infer trust from the
human-readable caption or hosted image URL.
