# Emoji rendering: prepared frame cache

Responsibility: remove per-cell Lottie parsing, vector-item construction and animated path tessellation from the GUI thread.
Capability origin: Local rendering over the existing optional animated-emoji provider.
Mandatory or optional: optional artwork; Unicode selection remains available without network or a decoder.
Public contract: EmojiController exposes local sprite metadata and an asynchronous request with backpressure and completion/failure signals.
Canonical data/invariants: Unicode remains content identity; provider keys and raster URLs remain in the provider adapter. No QML network or filesystem I/O beyond displaying prepared local images.
Dependencies: existing emoji runtime, provider, bounded executor, async transport, cache; Qt Gui image decoder and Qt Quick sprite rendering.
Expected failures: unsupported decoder/key, network timeout, malformed/oversized raster, cache write/read failure; display selectable Unicode instead of an indefinite placeholder.
Local-state representation: digest-checked PNG atlas with embedded frame metadata, in the existing bounded cache under a separate format/version key.
Offline behavior: prepared atlases can be read without network. A cold offline cache displays Unicode. Only the selected large preview may reuse a bundled or disk-cached Lottie while its sprite is unavailable; grid cells never instantiate vector renderers.
Provider policy and disablement: Noto raster endpoint is owned by the provider; the catalog remains manifest-derived. Missing GIF support disables sprite requests.
Failover semantics: 30-second native failure cooldown, at most two delayed automatic retries per visible sequence, plus explicit user retry. The preview's single legacy renderer is cache-only and cannot trigger an extra download. Grid cells do not fall back to vector rendering.
Tests proving the boundary: configure/build/runtime/performance checks deliberately not run per the user's instruction. Existing Lottie-specific UI fixtures do not cover the new picker renderer; sprite-specific coverage is still needed. Static review is not runtime verification.

## Inspection evidence

- Qt 6.11.1 source `qquickvectorimage.cpp`: `setSource()` calls `loadFile()` synchronously; this calls plugin `generate()` and constructs a `QQuickItemGenerator`. The old asynchronous Loader assigned the asset source only in `onLoaded`, after construction.
- `AnimatedEmojiLottie.qml` used `restart()` to pause. Hiding/recycling cells therefore restarted animation groups before pausing them.
- Timers spaced construction but never capped the total vector complexity or concurrent animation count; retaining renderers retained their vector trees.
- The existing disk cache stores source files, not decoded frames. It cannot eliminate renderer construction on scroll.

## New resource envelope

- At most two sprite jobs in flight, 32 accepted requests total, one decode worker shared with existing emoji work; rejected admissions are retried only by visible callers. A 150 ms viewport dwell avoids admitting transient categories. Queue entries and network operations are cancelled when the last visible consumer releases interest; decoding checks cancellation between input frames.
- Raster input at most 8 MiB, 512 × 512, 600 frames and 20 seconds.
- Output frames 96 × 96, at most 120 frames, sampled at at most 25 fps while preserving total duration. Atlas at most 12 columns; at most about 4.3 MiB decoded per asset.
- One prepared PNG includes timing metadata and is atomically stored with a SHA-256 digest in a separate 128 MiB `sprites-v1` cache. Source Lottie retains its existing 96 MiB budget. In-memory sprite metadata is bounded to 512 entries. No per-frame disk reads.
- QML preloads the atlas through asynchronous Image loading before giving the cached image to AnimatedSprite; playback switches texture coordinates, not vector paths.
- Retained ready sheets bypass native requests entirely on re-entry, avoiding repeated cache hashing as well as repeated downloads/decoding. Cold or evicted sheets still resolve asynchronously.
- Cells play once automatically; only the large preview loops. Offscreen, pooled and backgrounded cells pause; two extra rows retain warm sheets. The complete manifest remains browsable through More. Changing category can reload a PNG from disk after its decoded image is evicted, but does not repeat GIF decoding while the prepared disk entry survives. Cache budgets intentionally do not retain the entire catalog in RAM.
- No profiler was attached and no claim of measured CPU improvement is made.

## Recovery follow-up

- Asset preparation no longer waits for GraphicsInfo readiness; only GPU presentation does.
- Failure details preserve the network/HTTP/decode/atlas/cache/queue stage and are logged once per failed native job, not once per animation frame. Cancellation remains silent.
- GIF transfers have a 30-second overall deadline for their existing two transient-read attempts; byte and concurrency limits are unchanged.
- The selected preview shows loading/retry affordances. Corrupt image loads enter the same bounded retry path. A terminal failure no longer leaves an admitted sequence permanently unretryable while the picker stays open.
- This follow-up is source-reviewed only; network, decoder and UI execution are still deferred by user instruction. The observed empty sprite cache alone does not establish the original failing stage.
