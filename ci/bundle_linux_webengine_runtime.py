#!/usr/bin/env python3
"""Bundle Linux Qt WebEngine and its non-glibc runtime dependencies.

The Chromium subprocess and QML plugin are staged separately. This script
makes their dynamic libraries available to both AppImage and DEB without
requiring the target host to have the Qt SDK installed.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess

if __package__:
    from .bundle_linux_cpp_runtime import elf_machine, required_glibc_versions
    from .bundle_linux_tls_runtime import installed_library
else:
    from bundle_linux_cpp_runtime import elf_machine, required_glibc_versions
    from bundle_linux_tls_runtime import installed_library


# Qt 6 WebEngine is built against these externally supplied libraries in
# the official Ubuntu 24.04 extension. Do not bundle libc, libm or ld-linux.
THIRD_PARTY_SONAMES = (
    "libminizip.so.1",
    "libsnappy.so.1",
    "libwebp.so.7",
    "libwebpdemux.so.2",
    "libwebpmux.so.3",
    "libnss3.so",
    "libnspr4.so",
    "libsmime3.so",
    "libplc4.so",
    "libplds4.so",
    "liblcms2.so.2",
    "libXfixes.so.3",
    "libXdamage.so.1",
    "libXcomposite.so.1",
    "libXtst.so.6",
    "libxkbfile.so.1",
)


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def install(appdir: Path, qt_root: Path) -> None:
    usr = appdir / "usr"
    app = usr / "bin/Genycaster"
    require(app.is_file(), "Missing Genycaster in Linux AppDir")
    machine = elf_machine(app)
    target = usr / "lib"
    target.mkdir(parents=True, exist_ok=True)

    # linuxdeploy's Qt plugin may omit Qt libraries loaded only through
    # dynamically created QML. Ensure the Qt WebEngine family is complete.
    module_names = (
        "libQt6WebEngineCore.so.6",
        "libQt6WebEngineQuick.so.6",
        "libQt6WebEngineWidgets.so.6",
        "libQt6WebChannel.so.6",
        "libQt6Positioning.so.6",
        "libQt6SerialPort.so.6",
    )
    for name in module_names:
        source = qt_root / "lib" / name
        require(source.is_file(), f"Missing Qt WebEngine module: {source}")
        require(elf_machine(source) == machine, f"Wrong Qt WebEngine architecture: {source}")
        if not (target / name).is_file():
            shutil.copy2(source.resolve(strict=True), target / name)
    for source in (qt_root / "lib").glob("libQt6WebEngine*.so.6"):
        if not (target / source.name).is_file():
            require(elf_machine(source) == machine, f"Wrong Qt WebEngine architecture: {source}")
            shutil.copy2(source.resolve(strict=True), target / source.name)

    for soname in THIRD_PARTY_SONAMES:
        source = installed_library(soname, machine)
        # installed_library checks ELF machine and Ubuntu 24.04 glibc ceiling.
        dest = target / soname
        if dest.is_symlink():
            dest.unlink()
        if not dest.is_file():
            shutil.copy2(source, dest)
        require(elf_machine(dest) == machine,
                f"Bundled WebEngine library wrong architecture: {dest}")

    process = usr / "libexec/QtWebEngineProcess"
    qml = usr / "qml/QtWebEngine"
    require(process.is_file(), "Missing staged Chromium subprocess")
    require((qml / "qmldir").is_file(), "Missing staged QtWebEngine QML module")

    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(target.resolve())
    binaries = [app, process]
    binaries.extend(p for p in qml.rglob("*.so") if p.is_file())
    binaries.extend(target / name for name in module_names)
    for binary in binaries:
        require(elf_machine(binary) == machine,
                f"Linux WebEngine runtime ELF architecture mismatch: {binary}")
        result = subprocess.run(["ldd", str(binary)], env=env,
                                capture_output=True, text=True, check=False, timeout=30)
        output = result.stdout + result.stderr
        require(result.returncode == 0
                and "not found" not in output
                and "version `GLIBC" not in output,
                f"Missing Linux WebEngine dependency in {binary}:\n{output}")
    print(f"Bundled WebEngine ELF libraries and validated dependencies ({machine})")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    parser.add_argument("--qt-root", required=True, type=Path)
    args = parser.parse_args()
    install(args.appdir, args.qt_root)


if __name__ == "__main__":
    main()
