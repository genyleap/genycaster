# CMake manifest fix for the Genycaster Phase A/B redesign

The first package added QML files under `ui/` without merging them into the project's strict `farcasterQmlFiles` manifest. Genycaster intentionally fails configure when discovered and registered QML files differ, so this must be fixed before building.

## Preferred fix

From the Genycaster repository root, run:

```bash
python3 <path-to-package>/tools/patch_ui_cmake.py ui/CMakeLists.txt
```

The script makes `ui/CMakeLists.txt.bak`, merges the redesign QML files into `farcasterQmlFiles`, sorts/deduplicates the manifest, and registers the three QML singletons used by the redesign.

Then configure from a clean CMake cache or let Qt Creator reconfigure. If needed:

```bash
cmake --fresh -S . -B build/Qt_6_11_1_macOS-Debug \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/SDK/6.11.1/macos-arm64"
```

Qt Creator may supply additional toolchain flags; using its normal Reconfigure action is fine.

## Manual alternative

Copy the contents of `cmake/GenycasterRedesignIntegration.cmake` into `ui/CMakeLists.txt` after the existing `set(farcasterQmlFiles ...)` and before the code that validates the QML manifest / calls `qt_add_qml_module`. If that block sorts and compares lists itself, the extra `list(SORT ...)` is harmless.

## Why singleton registration matters

These files contain `pragma Singleton`:

- `foundations/Theme.qml`
- `foundations/Breakpoints.qml`
- `demo/DemoSocialData.qml`

For a CMake-backed Qt QML module they must also have `QT_QML_SINGLETON_TYPE TRUE` before `qt_add_qml_module`; otherwise configure may pass but qmltyperegistrar/runtime imports can fail later.
