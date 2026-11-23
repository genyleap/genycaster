# Genyleap Icon System

A reusable, product-grade SVG icon set for web and native UI.

## Version

- Current version: `0.2.1`
- Total icons: **163**

## Design rules

- Default canvas: `24 × 24`
- Outline stroke: `1.75`
- Line cap: `round`
- Line join: `round`
- Color: `currentColor`
- Default rendered sizes: `16 / 18 / 20 / 24 / 32 px`
- Filled variants are reserved for active or semantic states.
- The `quilibrium` family uses its own native logo geometry with `viewBox="0 0 180 180"` and currentColor fills.

## Categories

- `actions`
- `navigation`
- `system`
- `media`
- `social`
- `brands`
- `platforms`
- `languages`
- `reactions`
- `crypto`
- `quilibrium`

## Reaction strategy

You can now map reactions semantically instead of always using a heart:

- default content → `heart`
- Farcaster content → `farcaster` or `fc`
- Degen content → `degen`
- code / language content → `cplusplus`, `qml`, `typescript`, `javascript`, `php`
- platform-specific content → `linux`, `windows`, `macos`, `ios`, `android`
- social/source references → `github`, `youtube`, `telegram`
- crypto content → `bitcoin`, `ether`, `solana`, `eth-base`
- art / collectible content → `art`, `gem`, `nft`, `wow`
- Quilibrium ecosystem → `quilibrium`, `qstore`, `qkms`, `qping`, `hypersnap`, `quark`

## Included custom icons in v0.2.0

- Genyleap / Geny DNA marks
- Farcaster / FC
- Degen
- Linux / Windows / macOS / iOS / Android
- C++ / QML / TypeScript / JavaScript / PHP
- GitHub / YouTube / Telegram
- Wow / GEM / Art / NFT
- Bitcoin / Ether / Solana / ETH Base
- Quilibrium / QStore / QKMS / QPing / Hypersnap / Quark

## Web

Prefer inline SVG when runtime color/state changes are needed. All outline assets use `currentColor`.

## Qt / QML

Qt Quick Controls can consume SVG files through `icon.source`.

```qml
ToolButton {
    icon.source: "qrc:/icons/brands/farcaster.svg"
    text: "Farcaster"
    display: AbstractButton.IconOnly
    Accessible.name: text
}
```

## Accessibility

Icon-only controls must always expose an accessible label. Do not rely on icon shape alone to communicate destructive or irreversible actions.

## License

Most starter icons are original. The `media/sticker.svg` glyph is adapted from **Solar Icons – Sticker Smile Square**, sourced from https://www.svgrepo.com/svg/527550/sticker-smile-square and credited under SVG Repo's **CC Attribution** license; retain its source and attribution in redistributed builds. Other assets retain their respective individual licenses.


## v0.2 — Contextual reactions

The icon package now supports content-aware reaction glyphs.

New groups:
- `icons/genyleap`
- `icons/brands`
- `icons/tech`
- `icons/crypto`
- `icons/quilibrium`
- `icons/reactions`
- `reactions/reaction-rules.json`
- `reactions/matcher.js`

Examples:

```text
"I am building a native Farcaster client" -> brands/farcaster
"C++23 modules on Linux"                  -> tech/cplusplus
"Running a Quilibrium node"               -> quilibrium/quilibrium
"Normal post"                             -> actions/heart
```
