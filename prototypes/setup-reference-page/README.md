# GenyCaster Glass G Arc — interactive Setup design preview

**Review-only, not production onboarding:** the 4-step design sample does not
read user identities, accept a recovery phrase, or change the native/WASM Qt
application. The real 8-step Setup remains unchanged.

## Design direction

The only hero object is a 3D reconstruction of the **actual GenyCaster G and
directional arrow paths** in `ui/wasm/genycaster-icon.svg`. The geometry is
extruded with beveled edges, glass/metal Physical Materials, and a locally
generated studio-light environment. Previous unrelated spheres, planes, ribbon
waves, particles and fog were intentionally removed.

- Dark: luminous ivory glass and cool turquoise arrow, on a quiet navy panel.
- Light: satin blue-grey glass and teal arrow on an icy-white panel.
- Same camera, same composition, stable negative space in both modes.
- Pointer-responsive restrained parallax; click pulse; no perpetual animation.
- Reduced-motion system preference disables parallax and pulsing.
- Original logo, footer and version are accessible DOM overlays.
- A large but subtle original-SVG fallback appears when WebGL is unavailable.

## Files

- `index.html`, `styles.css`: real responsive design-review page
- `page.js`: 4 example steps, Back/Continue, Light/Dark controls
- `art.js`: WebGL 2 / locally vendored Three.js GPU scene
- `compare.html`: both themes rendered side by side
- `assets/genycaster-icon.svg`: exact icon from project
- `assets/three.module.js`: pinned Three.js release, local/no CDN
- `assets/THREE_LICENSE.txt`: MIT license

Browser entry: https://caster.genyleap.com/preview/brand3d/index.html

This is a GPU-based **brand sculpture**, not a claim of pixel-perfect physically
correct ray-traced glass. Integration in the native Qt Setup requires a separate
cross-platform implementation and user design approval.

## Verification

```sh
node --test tests/wasm/setup_reference_preview.test.mjs tests/wasm/setup_reference_page.test.mjs
```

Use WebGL 2 Chromium/SwiftShader to validate visual appearance, theme switching,
parallax, navigation and responsiveness. Production Setup is not modified.
