# Genycaster UI fonts

Genycaster does not assume that a Persian/Arabic UI font is installed on the host.
A shipping build embeds the pinned Vazirmatn static weights into the Qt resource system.

## Persian / Arabic

Resolution order at configure time:

1. `-DGENYCASTER_UI_FONT_DIR=/path/to/fonts/ttf` when explicitly supplied.
2. Vendored `ui/assets/fonts/Vazirmatn-*.ttf` files when a developer keeps a local copy.
3. The pinned upstream files fetched when `GENYCASTER_FETCH_UI_FONT=ON`.

For offline/reproducible release builds, provide all required Vazirmatn weights locally.
The resulting application embeds them; end users do not need to install the font.

## Devanagari / CJK

Devanagari and CJK font acquisition is intentionally **not a configure-time requirement**.
Developer/source builds use the native shaping-capable platform families when optional
Noto resources are unavailable. This keeps a slow or filtered network from breaking
CMake configuration.

Optional embedding can be supplied with:

- `-DGENYCASTER_SCRIPT_FONT_DIR=/path/to/script-fonts`
- `-DGENYCASTER_FETCH_SCRIPT_FONTS=ON -DGENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS=ON`

Release/CI packaging can enforce embedded coverage with
`-DGENYCASTER_REQUIRE_SCRIPT_FONTS=ON`.

## Icons

Primary application icons use bundled Font Awesome Free Regular/Solid resources when
available through the configured icon-font source. They do not depend on a font being
installed by the end user.
