# GenyCaster · Isolated 3D Setup Brand Prototype

This prototype experiments with the *left brand rail only*. It does **not** replace, alter, or bypass `InitialSetup.qml`, the real onboarding steps, or authentication. Its main scene uses the third approved visual concept: softly lit, curved glass architecture and floating highlights, with light and dark material variants.

## Artifacts

- `Prototype.qml` — real native `View3D` scene with `RuntimeLoader`, camera, two lights, pointer parallax and a crisp 2D overlay for the brand/copyright.
- `assets/brand-{dark,light}.glb` — generated glTF 2.0 binary models with matching nine-mesh geometry, different PBR color sets, and **no network textures**.
- `generate_assets.py` — deterministic Python 3 mesh generator using only standard-library modules.
- `index.html` — standalone **WebGL 2** GPU preview of exactly those GLB assets; no Three.js CDN or external scripts.
- `Capture.qml` / `verify_native.py` — optional headless native sanity check and screenshot capture.
- `tests/wasm/brand_3d_prototype.test.mjs` — static transport, GLB and interaction tests.

## View and test

Live isolated browser preview: <https://caster.genyleap.com/preview/brand3d/index.html>

Move the pointer across the left panel, switch Dark / Light, or choose Reduce motion. Rendering is event-driven, not an endless animation. On browsers without WebGL 2 a clearly labeled error replaces the 3D view.

For a Qt Quick 3D-capable **native** Qt installation, run:

```sh
python3 generate_assets.py
qmlscene Prototype.qml
```

On the current build server, a full native Qt Quick 3D module is installed with **Qt 6.11.2**. The GenyCaster Qt **6.12.0 Native and WASM kits are missing the module**. A native 6.11.2 QML smoke test loaded both GLB files; its software-rendered screenshots are illustrative, not proof of GPU quality on user hardware.

## Before integration

1. Add a matching Qt Quick 3D module to *each target kit* (desktop and WASM), or provide a separate WebGL renderer for the browser edition.
2. Benchmark real WebGL 2 devices and browser memory use, check the color/material response of glass and keep a static non-WebGL fallback.
3. Preserve the existing Setup composition and copyright link as accessible 2D QML controls.
4. Wire reduced-motion to the actual application appearance setting.
5. Obtain design approval for this independent preview before changing production `InitialSetup.qml`.

This prototype does not promise a pixel-perfect match to the original concept render: source lighting, depth, reflections, and compositing have to be tuned against real GPU screenshots. Static concept images are not proof of production 3D rendering.
