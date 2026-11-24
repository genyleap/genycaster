# Third-party notices

This file acknowledges third-party assets and libraries committed to or used by GenyCaster. The original GenyCaster source code is [GPL-3.0-only](LICENSE); **that license does not override the license of third-party material**. Trademarks and brand marks remain the property of their respective holders.

## Included artwork, icons and fonts

| Material | Source / creator | Licensing and notice |
|---|---|---|
| Noto Animated Emoji Lottie assets | [Google Noto Emoji](https://github.com/googlefonts/noto-emoji) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); full bundled text at `ui/assets/emoji/LICENSE.NotoAnimatedEmoji.txt` |
| Noto Color Emoji / the GenyCaster font subset | Google | [SIL OFL 1.1](ui/assets/fonts/OFL-NotoEmoji.txt); `ui/assets/fonts/GenycasterColorEmoji.ttf` and `GenycasterEmojiFallback.ttf` |
| Noto Sans Math (downloaded at a pinned SHA-256 for WASM styled Unicode fallback) | [The Noto Project Authors](https://github.com/notofonts/math) | [SIL OFL 1.1](ui/assets/fonts/OFL-NotoSansMath.txt); preserves Mathematical Alphanumeric Symbols in public profile names and bios |
| Vazirmatn typography (fetched/embedded at build time when configured) | [Vazirmatn authors](https://github.com/rastikerdar/vazirmatn) | [SIL OFL 1.1](ui/assets/fonts/OFL-Vazirmatn.txt); follow license and attribution for distributed binaries |
| Solar Icon Set subset | **480 Design** — [upstream](https://github.com/480-Design/Solar-Icon-Set) | Icon artwork is attributed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Adapted/tinted for the GenyCaster UI; see `ui/assets/icons/genyleap/SOLAR_ICON_SET.md` |
| Font Awesome Free (when included) | Fonticons, Inc. | SVG/icons: CC BY 4.0; fonts: SIL OFL 1.1; [bundled notice](ui/assets/icons/LICENSE.Font-Awesome.txt) |
| Sticker toolbar icon | Genycaster project artwork | Original vector glyph with a folded corner and smile in the messaging-app design language; not a third-party brand logo |
| Third-party brand and network icon references | Respective brand owners | Usage is subject to trademark/brand rules, **not** a transfer of trademark ownership; see [brand notice](ui/assets/icons/genyleap/TRADEMARKS.md) |

GenyCaster includes a bootstrap subset of animated Noto Emoji and may download additional validated assets into a bounded user cache. Unicode emoji characters, font glyphs and animated artwork have independent licensing/provenance. Project-generated logo artwork and the GenyCaster/Genyleap names are not granted as trademarks by GPLv3.

## Linked, non-bundled sticker media

The Njal sticker picker references images from
[Njal's Sticker Pack](https://github.com/cryptonjal/njals-sticker-pack), credited
to Njal. Its public
[manifest](https://raw.githubusercontent.com/cryptonjal/njals-sticker-pack/refs/heads/main/njals-sticker-pack.json)
defines the WebP URLs and hashes. As of 22 November 2026, that repository does **not**
publish a license for the artwork. GenyCaster does not vendor, mirror, transform or
claim ownership of the images; the catalog references the author-provided IPFS
URLs. A separate redistribution permission would be required before bundling or
self-hosting those images. Historical first-party sticker URLs remain served to
avoid breaking previously published content.

## Cream Cat sticker pack (linked, credit required)

The **208 Cream Cat WebP posters** used in the current `cream-cat-pro`
selection are published by [mssgs-stickers.com](https://mssgs-stickers.com/packs/cream-cat)
under its [free-with-credit licence](https://mssgs-stickers.com/license).
The licence permits use in commercial applications and hotlinking, but forbids
claiming ownership or selling the stickers as if they were Genycaster artwork.
The application links to the publisher's original hosted files and visibly
credits **"Stickers by mssgs-stickers.com"** in the pack details; the source
package and this notice also carry attribution. The poster WebP images are
used for interoperability with normal cast embeds. Original APNG motion
assets are not bundled.

## Former Twemoji pack (historical Cast compatibility, CC BY 4.0)

The eight graphics that *previously* appeared in the `twemoji-pro`
pack derive from
[Twemoji by Twitter, Inc. and contributors, now maintained by jdecked](https://github.com/jdecked/twemoji),
source tag `v17.0.3` and
[graphics license CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
They have been **rasterized from original SVG into transparent 512×512 PNG**
without changing the geometry, then published under
`https://genycaster.xyz/stickers/twemoji-pro-v1/`. The generated PNGs are
not presented as Genycaster-original designs. The pack is retired; these
immutable URLs remain available solely so existing public Casts do not break.
This attribution and the CC BY 4.0 notice apply to any subsequent rehosting
or redistribution.

## Bundled WebAssembly JavaScript

| Asset | Project | License file |
|---|---|---|
| `ui/wasm/vendor/hls-1.6.15.min.js` | [hls.js](https://github.com/video-dev/hls.js) | [Apache License 2.0](ui/wasm/vendor/hls-1.6.15.LICENSE) |
| `ui/wasm/vendor/noble-secp256k1.mjs` | [@noble/secp256k1](https://github.com/paulmillr/noble-curves) family | [MIT](ui/wasm/vendor/noble-secp256k1.LICENSE) |
| `ui/wasm/vendor/scure-bip39.mjs` | [@scure/bip39](https://github.com/paulmillr/scure-bip39) | [MIT](ui/wasm/vendor/scure-bip39.LICENSE) |

Other vendored or copied third-party content and transitive package dependencies must remain covered by their corresponding notices and licenses. Do **not** treat this summary as a complete transitive software bill of materials.

## Frameworks and external dependencies

Qt, OpenSSL, Boost, SQLite, Emscripten, QtKeychain, platform SDKs and dependencies installed through package managers retain their respective licenses, including any distribution obligations triggered by the way a release binary is linked or packaged.

**Connected identity service, not a vendored library:** [OpenProof](https://github.com/genyleap/openproof) (`opp`, configured at `https://openproof.genyleap.com`) is an independently deployed identity server used by GenyCaster's account-authentication routes. Its source and third-party licensing are governed by that separate repository. GenyCaster's own `services/account-sync/` stores product state in its independent PostgreSQL storage; it must not be confused with OpenProof's credential store. See [Server Service Architecture](docs/OPENPROOF_AND_SERVICES.md).

### Release due diligence

Before changing the repository to public or distributing a new build, verify the **exact pinned versions**, complete notices/SBOM, font/icon provenance, bundled license texts, Qt deployment obligations, commercial/brand usage permissions and the licenses of transitive npm/C++ dependencies. Resolve any incompatible or unverified assets before claiming legal clearance.

For legal attribution corrections, open a non-sensitive issue with the asset path, credited creator, provenance and license reference. For an actual security issue, follow [SECURITY.md](SECURITY.md).
