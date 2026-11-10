#!/usr/bin/env python3
"""Install the matching Qt TLS plugin and OpenSSL 3 pair into a Linux AppDir.

Qt 6.12 loads OpenSSL with dlopen; linuxdeploy therefore cannot infer this
dependency by following DT_NEEDED on the executable. Shipping only libcrypto
(or relying on the host's different libssl) silently breaks all HTTPS.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess

if __package__:
    from .bundle_linux_cpp_runtime import elf_machine, required_glibc_versions
else:
    from bundle_linux_cpp_runtime import elf_machine, required_glibc_versions


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def installed_library(soname: str, architecture: str) -> Path:
    cache = subprocess.run(
        ["ldconfig", "-p"], capture_output=True, text=True, check=True, timeout=15,
    )
    # Match an exact SONAME and the ELF machine, not the first host entry
    # (which could be a 32-bit/multiarch compatibility library).
    for row in cache.stdout.splitlines():
        if "=>" not in row or row.split(None, 1)[0] != soname:
            continue
        candidate = Path(row.split("=>", 1)[1].strip())
        if candidate.is_file() and elf_machine(candidate) == architecture:
            require(
                all(version <= (2, 39) for version in required_glibc_versions(candidate)),
                f"{soname} requires glibc newer than Ubuntu 24.04",
            )
            return candidate
    raise ValueError(f"System OpenSSL {soname} for {architecture} is unavailable")


def install(appdir: Path, qt_root: Path) -> None:
    usr = appdir / "usr"
    executable = usr / "bin/Genycaster"
    require(executable.is_file(), "Linux AppDir is missing Genycaster")
    machine = elf_machine(executable)

    openssl_plugin = qt_root / "plugins/tls/libqopensslbackend.so"
    require(openssl_plugin.is_file(), "Qt 6.12 OpenSSL TLS plugin not found")
    require(elf_machine(openssl_plugin) == machine,
            "Qt TLS plugin architecture does not match Genycaster")
    plugin_destination = usr / "plugins/tls/libqopensslbackend.so"
    plugin_destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(openssl_plugin, plugin_destination)

    runtime_libraries = usr / "lib"
    runtime_libraries.mkdir(parents=True, exist_ok=True)
    for soname in ("libssl.so.3", "libcrypto.so.3"):
        source = installed_library(soname, machine)
        target = runtime_libraries / soname
        if target.is_symlink():
            target.unlink()
        shutil.copy2(source, target)
        require(target.is_file() and elf_machine(target) == machine,
                f"OpenSSL runtime architecture mismatch: {soname}")
        print(f"Bundled {soname} ({machine}) from {source}")

    # AppRun supplies LD_LIBRARY_PATH for Qt's runtime dlopen calls.
    # qt.conf directs plugin discovery to this shipped application, never
    # the developer's Qt installation or an unrelated host Qt runtime.
    config = usr / "bin/qt.conf"
    config.write_text(
        "[Paths]\\nPrefix = ..\\nPlugins = plugins\\nQml2Imports = qml\\nQmlImports = qml\\n",
        encoding="utf-8",
    )
    print(f"Bundled Qt TLS/OpenSSL runtime: {plugin_destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    parser.add_argument("--qt-root", type=Path, required=True)
    arguments = parser.parse_args()
    install(arguments.appdir, arguments.qt_root)


if __name__ == "__main__":
    main()
