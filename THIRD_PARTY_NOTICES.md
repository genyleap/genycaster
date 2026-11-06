# Third-party notices

This file acknowledges third-party assets and libraries committed to or used by GenyCaster. The original GenyCaster source code is [GPL-3.0-only](LICENSE); **that license does not override the license of third-party material**. Trademarks and brand marks remain the property of their respective holders.

## Included artwork, icons and fonts

| Material | Source / creator | Licensing and notice |
|---|---|---|
| Noto Animated Emoji Lottie assets | [Google Noto Emoji](https://github.com/googlefonts/noto-emoji) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); full bundled text at `ui/assets/emoji/LICENSE.NotoAnimatedEmoji.txt` |
| Noto Color Emoji / the GenyCaster font subset | Google | [SIL OFL 1.1](ui/assets/fonts/OFL-NotoEmoji.txt); `ui/assets/fonts/GenycasterColorEmoji.ttf` and `GenycasterEmojiFallback.ttf` |
| Vazirmatn typography (fetched/embedded at build time when configured) | [Vazirmatn authors](https://github.com/rastikerdar/vazirmatn) | [SIL OFL 1.1](ui/assets/fonts/OFL-Vazirmatn.txt); follow license and attribution for distributed binaries |
| Solar Icon Set subset | **480 Design** — [upstream](https://github.com/480-Design/Solar-Icon-Set) | Icon artwork is attributed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Adapted/tinted for the GenyCaster UI; see `ui/assets/icons/genyleap/SOLAR_ICON_SET.md` |
| Font Awesome Free (when included) | Fonticons, Inc. | SVG/icons: CC BY 4.0; fonts: SIL OFL 1.1; [bundled notice](ui/assets/icons/LICENSE.Font-Awesome.txt) |
| Third-party brand and network icon references | Respective brand owners | Usage is subject to trademark/brand rules, **not** a transfer of trademark ownership; see [brand notice](ui/assets/icons/genyleap/TRADEMARKS.md) |

GenyCaster includes a bootstrap subset of animated Noto Emoji and may download additional validated assets into a bounded user cache. Unicode emoji characters, font glyphs and animated artwork have independent licensing/provenance. Project-generated logo artwork and the GenyCaster/Genyleap names are not granted as trademarks by GPLv3.

## Bundled WebAssembly JavaScript

| Asset | Project | License file |
|---|---|---|
| `ui/wasm/vendor/hls-1.6.15.min.js` | [hls.js](https://github.com/video-dev/hls.js) | [Apache License 2.0](ui/wasm/vendor/hls-1.6.15.LICENSE) |
| `ui/wasm/vendor/noble-secp256k1.mjs` | [@noble/secp256k1](https://github.com/paulmillr/noble-curves) family | [MIT](ui/wasm/vendor/noble-secp256k1.LICENSE) |
| `ui/wasm/vendor/scure-bip39.mjs` | [@scure/bip39](https://github.com/paulmillr/scure-bip39) | [MIT](ui/wasm/vendor/scure-bip39.LICENSE) |

Other vendored or copied third-party content and transitive package dependencies must remain covered by their corresponding notices and licenses. Do **not** treat this summary as a complete transitive software bill of materials.

## Frameworks and external dependencies

Qt, OpenSSL, Boost, SQLite, Emscripten, QtKeychain, platform SDKs and dependencies installed through package managers retain their respective licenses, including any distribution obligations triggered by the way a release binary is linked or packaged.

### Release due diligence

Before changing the repository to public or distributing a new build, verify the **exact pinned versions**, complete notices/SBOM, font/icon provenance, bundled license texts, Qt deployment obligations, commercial/brand usage permissions and the licenses of transitive npm/C++ dependencies. Resolve any incompatible or unverified assets before claiming legal clearance.

For legal attribution corrections, open a non-sensitive issue with the asset path, credited creator, provenance and license reference. For an actual security issue, follow [SECURITY.md](SECURITY.md).
