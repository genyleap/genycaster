# Platform packaging properties

GenyCaster uses a single shared Modern C++ and Qt Quick implementation. This
directory owns **platform-specific deployment properties and static package
artifacts**, not separate implementations of the application.

| Directory | Owner | Source of truth |
|---|---|---|
| [Android](Android/) | Qt Android package | Manifest, adaptive/legacy launcher icons and Android resources |
| [iOS](iOS/) | Qt iOS app bundle | AppIcon asset catalog |
| [macOS](macOS/) | macOS `.app` bundle | ICNS icon; CMake/Qt generate `Info.plist` and maintain the executable name |
| [Windows](Windows/) | Windows executable resources | Version-information `.rc.in` template and ICO icon |
| [Linux](Linux/) | Linux desktop package | freedesktop desktop entry and application icon |
| [WebAssembly](WebAssembly/) | Browser/PWA publication | Web App Manifest, copied to the public deployment root |

Platform **code** remains in `src/platform/` and narrowly scoped adapters in
`ui/app/`; the shared Qt interface remains in `ui/components/`. Browser-only
bootstrap JS/HTML, the service worker and runtime artifacts stay in `ui/wasm/`.
Moving an asset here must not change a browser URL, Android package identity,
Apple bundle identifier, or compiled Qt resource alias.

## Generation and build

The icons come from one canonical design master:
`ui/wasm/genycaster-512.png`.

```sh
python3 cmake/generate_app_icons.py --check
# Regenerate only after updating the master artwork:
python3 cmake/generate_app_icons.py
```

`ui/CMakeLists.txt` imports [`ConfigureTarget.cmake`](ConfigureTarget.cmake),
which selects the active platform's own `Configure.cmake`. Platform-specific
CMake bundle metadata and Windows `.rc` version resources are therefore not
inlined into the shared Qt application composition. Qt generates Apple `Info.plist` files
so Qt's required bundle keys and signing context are preserved. The browser
manifest is deployed with the **same** public filename,
`genycaster.webmanifest`; runtime source paths are not public URL paths.

Do not copy GenyConnect's VPN permissions, background services or old Apple
deployment keys into GenyCaster. Package properties follow GenyCaster's actual
capabilities and current Qt target contracts.
