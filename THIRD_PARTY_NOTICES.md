# Third-party notices

This file attributes third-party assets that ship inside the GenyCaster
application tree. It is separate from the GenyCaster application license and
does not imply ownership of any upstream artwork.

## Google Noto Animated Emoji

- Upstream project: https://github.com/googlefonts/noto-emoji (animated set)
- License: **CC BY 4.0** — https://creativecommons.org/licenses/by/4.0/
- Assets: `ui/assets/emoji/lottie/*.json` (Lottie animation JSON) plus the
  bootstrap manifest `ui/assets/emoji/bootstrap_manifest.json`.
- Full license text: `ui/assets/emoji/LICENSE.NotoAnimatedEmoji.txt`.

GenyCaster bundles a small bootstrap subset of Noto Animated Emoji for
offline rendering and downloads additional validated assets on demand into a
bounded user cache. Rendering animated emoji is purely additive: the Unicode
emoji always remains the canonical content, and no upstream artwork is
incorporated into GenyCaster source code licensing.
