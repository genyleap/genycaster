# Cross-platform CMake ownership

Platform-neutral configuration remains in the repository root
`CMakeLists.txt` and the shared helpers directly under `cmake/`.
Platform-specific compiler, SDK, linker and packaging-related helpers live
here and are only included for their corresponding target system.

| Platform | Folder | Responsibilities |
|---|---|---|
| Android | `Android/` | NDK C compilation, ABI-compatible OpenSSL, Boost headers, Qt TLS, tests and 16 KiB page alignment |
| WebAssembly | `WebAssembly/` | Browser-safe feature composition and portable Boost headers |
| macOS | `macOS/` | Native .app filesystem-case normalization |
| Windows | `Windows/` | Target-specific Boost.JSON linking declarations |
| Linux | `Linux/` | ELF PIC requirements for the Qt presentation build |

Deployment properties such as icons, manifests and installer metadata are
stored separately under [`properties/`](../../properties/README.md);
the shared UI remains in `ui/`.

**Invariants:** never link host macOS/Windows dependencies into an Android
or WASM target; never weaken signer authorization as a packaging workaround;
do not suppress compiler warnings globally or selectively to hide
deprecated SDK usage. A new platform-specific build fix belongs in its
own directory, and every changed target needs its relevant CI coverage.
