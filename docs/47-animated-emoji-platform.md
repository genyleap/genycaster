# Animated emoji platform (Unicode-first, Noto Animated, Lottie)

Feature implementation gate and architecture record for the GenyCaster emoji
platform.

## Responsibility

Render standard Unicode emoji as animated vector assets when a validated asset
is available, while keeping the Unicode sequence the only canonical content
identity for composition, serialization, clipboard, accessibility, and
Farcaster publishing. The platform is provider-neutral and prepared for Geny
custom emoji packs and alternative animated providers.

Capability origin: Extension (rendering) over Protocol (Unicode identity).
Mandatory or optional: Optional rendering enhancement; the core never depends
on it. Disabling or removing the platform leaves every surface rendering
ordinary system Unicode emoji.
Public contract: `farcaster.capability.animated_emoji` (provider-neutral
`IAnimatedEmojiProvider`, `IEmojiAssetCache`, asset descriptors) consumed by
the presentation boundary.
Canonical data/invariants: A cast containing `🔥` stays `🔥` everywhere —
composer text, persisted RichCast documents, clipboard, and the Farcaster
wire. Animation is a pure presentation overlay keyed by the normalized
Unicode asset key.
Dependencies: domain (descriptors) → capability (ports) → application
(normalizer, catalog, repository, motion policy) → providers (Noto mapping,
Lottie validation, disk cache) → Qt presentation boundary. Provider
implementation details never enter application or domain code.
Expected failures: unsupported emoji, missing/corrupt/oversized asset,
network failure, cache corruption, revision mismatch, no Lottie renderer.
Local-state representation: Unicode-only recent-emoji ranking persisted by
the Qt controller; validated assets live in a bounded disk cache keyed by
provider/key/revision.
Offline behavior: bundled bootstrap assets (15 common emoji) render without
network; previously cached assets render offline; everything else shows the
system glyph.
Provider policy and disablement: Noto is the default provider and the only
provider composed today; the CDN URL lives in provider configuration.
Failover semantics: validated animated asset → static provider asset (future)
→ system Unicode glyph. The UI never shows an empty box.
Tests proving the boundary: provider-free core builds and passes; architecture
guards reject provider semantics outside `src/providers` and QML I/O;
`tests/unit/emoji_tests.cpp` covers normalization, provider, validator,
cache, repository, and motion policy.

## Module layout

- `src/domain/emoji.cppm` — `AnimatedEmojiAsset`, `StaticEmojiFallback`,
  `EmojiPresentation`, and the future `CustomEmojiRef` shape.
- `src/capabilities/animated_emoji.cppm` — provider and cache ports plus
  request/availability vocabulary.
- `src/application/emoji/unicode_normalizer.*` — canonical lowercase-hex keys
  (`1f602`, `2764_fe0f`) and UTF-8 emoji-cluster scanning for ZWJ chains,
  skin tones, flags, keycaps, and tag sequences.
- `src/application/emoji/emoji_catalog.*` — immutable catalog derived from the
  vendored upstream manifest plus search/ranking and bounded recent-emoji
  ranking (Unicode identity only).
- `src/application/emoji/animation_policy.*` — visibility/motion rules.
- `src/application/emoji/asset_repository.*` — I/O-free rendering decisions,
  in-memory registry, failure cooldown.
- `src/providers/emoji/noto_animated_emoji.*` — Google Noto Animated Emoji
  mapping and CDN resolution.
- `src/providers/emoji/emoji_asset_validator.*` — bounded Lottie JSON
  validation (size, schema, layers, dimensions, frame rate/duration, no 3D /
  image / text layers, no external URLs, SHA-256).
- `src/providers/emoji/emoji_cache.*` — deterministic revision-aware disk
  cache with atomic writes, sidecar digests, corruption eviction, and a byte
  budget.
- `src/presentation/qt/emoji_controller.*` — Qt MOC boundary: async fetch
  pipeline, recents persistence, picker model, diagnostics.
- `src/presentation/qt/rich_inline_text_item.*` — Qt MOC boundary: native
  `QTextDocument` renderer that treats emoji as real inline objects and
  reports their layout rectangles.
- QML: `AnimatedEmoji.qml`, `AnimatedEmojiLottie.qml`, `RichInlineText.qml`,
  `EmojiPicker.qml`.

## Unicode identity and Farcaster interoperability

Standard emoji are never replaced in the portable text. The composer inserts
plain Unicode; `RichCastDocument` and the markdown importer carry emoji as
ordinary text (with an optional `emoji` inline classification for rendering
hints). Copying an animated emoji copies the Unicode sequence because the
clipboard path never sees the overlay. Non-GenyCaster clients always observe
standard Unicode.

## Renderer

`RichInlineTextItem` (QML name `RichInlineTextItem`, used by
`RichInlineText.qml` and `RichTextView.qml`) builds a `QTextDocument`, marking
each emoji cluster with a U+FFFC inline object whose intrinsic size is the
font em box. Layout, wrapping, BiDi, mixed direction, alignment, and link hit
testing therefore come from Qt's text engine rather than pixel hacks. The
object handler always paints the system Unicode glyph as the fallback, and the
item reports each emoji rectangle (`emojiRects`) in item coordinates. The QML
wrapper overlays one `AnimatedEmoji` (a Qt Labs Lottie `LottieAnimation`
loaded lazily through `AnimatedEmojiLottie.qml`) at exactly that rectangle, so
there is no layout shift when an asset arrives.

## Motion policy and accessibility

Playback is centralized and visibility-aware: overlays activate only when the
block is visible, the window is active, motion reduction is off, and a
validated asset exists; deactivation unloads the Lottie renderer immediately.
Feed/profile casts, detail and active animated-picker cells all loop continuously.
Healthy loaded artwork stays animated instead of returning to Unicode after
one iteration. Scrolling, app inactivity and reduced motion pause the
in-progress animation without changing its configured infinite loop count.
List recycling destroys offscreen delegates.

The composer picker deliberately separates the complete **Emoji** catalog from
the curated **Animated** special set. The ordinary grid exposes all 881 entries
from the vendored Noto catalog as Unicode-only cells; it creates no animated
renderer or asset request and therefore stays cheap while scrolling or moving
the pointer. Animated mode exposes only the 15 validated bundled special emoji,
has its own bounded search, and owns exactly one Noto Animated preview renderer
above the grid. Pointer movement only changes lightweight selection state;
after a short dwell, the single preview changes asset. This bounds
Lottie/VectorImage construction to one scene-graph subtree and prevents
viewport-sized network bursts or completion fan-out.

Reduced Motion preserves provider artwork in the Animated preview and rich-text
surfaces but pauses motion. Emoji remain ordinary text to assistive technology
— no asset filenames or provider ids are ever exposed.

## Adding a future provider or custom packs

A new animated provider implements `IAnimatedEmojiProvider` plus an
`IEmojiAssetCache`; the repository, composer, renderer, picker, and cache are
provider-neutral and unchanged. Future custom packs add a `CustomEmojiRef`
identity (`assetId`, `packId`, fallback, `revision`) beside standard Unicode
in the rich model without mixing identities. The picker's category model and
the `EmojiStyle` selection point are structured so alternative visual styles
(Fluent-style providers, system emoji) can slot in without production-UI
churn.

## Licensing

Google Noto Animated Emoji assets are CC BY 4.0 and are licensed separately
from GenyCaster application code. See
`ui/assets/emoji/LICENSE.NotoAnimatedEmoji.txt`, the root
`THIRD_PARTY_NOTICES.md`, and `ui/assets/emoji/bootstrap_manifest.json` for
the bundled bootstrap set and its checksums.
