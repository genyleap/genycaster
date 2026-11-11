# Linux distribution theme artwork

The Linux platform appearance preset uses `QSysInfo::productType()` (the
`ID` from `/etc/os-release`) to select a bundled brand mark and accent color.
Only explicitly supported distribution IDs use their own branding. Unknown
systems use the generic Linux mark; `ID_LIKE` does **not** silently imply that
a derivative is Ubuntu, Fedora, Debian, or Arch.

The platform SVG files added for individual distributions originate from the
[Simple Icons](https://github.com/simple-icons/simple-icons) project's
`icons/<slug>.svg` artwork (CC0-1.0). SVG roots have been normalized to a
24×24 monochrome vector for GenyCaster's existing theme-aware icon renderer.
The geometric paths are retained, and the accent swatch is supplied by the
centralized distribution mapping in
`src/presentation/qt/appearance_controller.cpp`.

The generic Linux and previously bundled platform icons remain local GenyCaster
assets. Supported distribution aliases (for example `arch` → Arch Linux and
`pop` → Pop!_OS) refer to one shared SVG per brand. This avoids raster images,
OS-specific resource paths, internet requests, and an Ubuntu default on
non-Ubuntu Linux systems.

Run the lightweight asset and profile-link regression check with:

```sh
python3 -m unittest ci/test_linux_distro_assets.py
```
