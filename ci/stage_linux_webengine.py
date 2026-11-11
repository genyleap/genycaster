#!/usr/bin/env python3
"""Stage Qt WebEngine's subprocess and resources before linuxdeploy.

The WebEngine Qt modules can be linked successfully without installing their
out-of-process Chromium executable, ICU data, locale packs or QML plugin. A
release must carry all of them, without depending on the developer Qt SDK.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil

if __package__:
    from .bundle_linux_cpp_runtime import elf_machine
    from .preflight_linux_miniapp_host import missing_host_files
else:
    from bundle_linux_cpp_runtime import elf_machine
    from preflight_linux_miniapp_host import missing_host_files


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def install(appdir: Path, qt_root: Path) -> None:
    app = appdir / "usr/bin/Genycaster"
    require(app.is_file(), "Mini App staging requires a built Genycaster")
    qt_root = qt_root.resolve(strict=True)
    missing = missing_host_files(qt_root)
    require(not missing, "Qt WebEngine SDK is incomplete: " + ", ".join(missing))
    machine = elf_machine(app)

    process = next(
        p for p in (qt_root / "libexec/QtWebEngineProcess",
                    qt_root / "bin/QtWebEngineProcess") if p.is_file()
    )
    require(elf_machine(process) == machine, "QtWebEngineProcess wrong architecture")
    destination = appdir / "usr/libexec/QtWebEngineProcess"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(process, destination)
    destination.chmod(0o755)

    source_resources = next(
        p for p in (qt_root / "resources", qt_root / "lib/resources")
        if (p / "icudtl.dat").is_file()
    )
    resource_dir = appdir / "usr/resources"
    resource_dir.mkdir(parents=True, exist_ok=True)
    # Include the complete immutable Chromium resource set (some Qt 6.140
    # builds also ship V8 snapshots and DevTools resource packs). Omitting
    # one may work on the build host but fail at runtime on another machine.
    for resource in source_resources.iterdir():
        if resource.is_file() and resource.suffix.lower() in {".pak", ".bin", ".dat"}:
            shutil.copy2(resource, resource_dir / resource.name)
    require((resource_dir / "icudtl.dat").is_file(),
            "Chromium ICU data was not staged")

    locale_source = next(
        p for p in (qt_root / "translations/qtwebengine_locales",
                    qt_root / "resources/qtwebengine_locales")
        if p.is_dir() and any(p.glob("*.pak"))
    )
    locale_dest = appdir / "usr/translations/qtwebengine_locales"
    locale_dest.mkdir(parents=True, exist_ok=True)
    for locale in locale_source.glob("*.pak"):
        shutil.copy2(locale, locale_dest / locale.name)

    # The drawer QML is loaded from a compiled resource; Qt's deploy scanner
    # does not necessarily discover its dynamically imported QtWebEngine QML.
    quick_src = qt_root / "qml/QtWebEngine"
    quick_dest = appdir / "usr/qml/QtWebEngine"
    shutil.copytree(quick_src, quick_dest, dirs_exist_ok=True,
                    symlinks=False)

    for plugin in quick_dest.rglob("*.so"):
        require(elf_machine(plugin) == machine,
                f"QtWebEngine QML plugin wrong architecture: {plugin}")
    print(f"Staged Qt WebEngine subprocess, QML, resources, locales ({machine})")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    parser.add_argument("--qt-root", type=Path, required=True)
    args = parser.parse_args()
    install(args.appdir, args.qt_root)


if __name__ == "__main__":
    main()
