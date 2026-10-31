# GenyCaster — interactive Setup concept page

This is the live **page**, not a rendered image. It follows the approved split
composition with GPU-accelerated curved glass artwork on the left, real HTML
content and navigation on the right, and separately designed light/dark themes.

- Preview: https://caster.genyleap.com/preview/setup-concept/index.html
- Compare both themes: https://caster.genyleap.com/preview/setup-concept/compare.html

## Structure

- `index.html` — accessible real UI with actual buttons, links and
  a canvas for the GPU artwork.
- `styles.css` — dark/light visual system and responsive layout.
- `page.js` — four-step **design demonstration**, working Continue/Back
  navigation and a theme switch. It does **not** authenticate, store or alter
  account information.
- `art.js` — WebGL / Three.js procedural curved glass ribbon, depth and
  pointer interaction. Reduced-motion preference is respected and a static
  gradient remains visible when WebGL is unsupported.
- `compare.html` — simultaneous dark/light view through live page frames.
- `assets/three.module.js` — pinned self-hosted Three.js source, MIT license
  in `THREE_LICENSE.txt`. No external JS/CDN dependencies.
- `assets/genycaster-icon.svg` — from the project icon package.

The production GenyCaster onboarding has more stages than this *four-step
visual prototype*; no change to `InitialSetup.qml` or the production WASM
bundle is made until design and UX are approved.

## Tests

```bash
node --test tests/wasm/setup_reference_page.test.mjs tests/wasm/setup_reference_preview.test.mjs
```

Browser QA on Chromium exercised rendering (WebGL 2), the two themes,
Continue/Back and mobile sizing without JS errors. Native Quick 3D integration
is a separate task requiring module availability in all targeted Qt kits.
