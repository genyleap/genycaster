#!/usr/bin/env python3
"""Enforce repository architecture and Modern C++ module policy in CI."""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
SRC = ROOT / "src"

TEXT_SUFFIXES = {".cpp", ".cppm", ".hpp", ".h", ".qml", ".cmake", ".txt"}
MODULE_NAME = re.compile(r"^[a-z][a-z0-9]*(?:\.[a-z][a-z0-9]*)+$")
MODULE_DECL = re.compile(r"^\s*(?:export\s+)?module\s+(farcaster(?:\.[a-z0-9]+)+)\s*;", re.MULTILINE)
EXPORT_MODULE_DECL = re.compile(r"^\s*export\s+module\s+(farcaster(?:\.[a-z0-9]+)+)\s*;", re.MULTILINE)
EXPORT_NAMESPACE = re.compile(r"^\s*export\s+namespace\s+([A-Za-z0-9_:]+)\s*\{", re.MULTILINE)
IMPORT_DECL = re.compile(r"^\s*import\s+(farcaster(?:\.[a-z0-9]+)+)\s*;", re.MULTILINE)
ENUM_CLASS = re.compile(r"enum\s+class\s+[A-Za-z][A-Za-z0-9]*[^\{]*\{(?P<body>.*?)\};", re.DOTALL)
ENUMERATOR = re.compile(r"(?:^|,)\s*([A-Za-z][A-Za-z0-9_]*)", re.MULTILINE)
QT_DEPENDENCY = re.compile(r"#\s*include\s*[<\"]Q|\bimport\s+Qt")
PROVIDER_SEMANTICS = re.compile(
    r"\bhypersnap\b|client\.farcaster\.xyz|api\.farcaster\.xyz|ws\.farcaster\.xyz",
    re.IGNORECASE,
)
QML_IO = re.compile(
    r"\bXMLHttpRequest\b|\bWebSocket\b|\bopenDatabaseSync\b|\bLocalStorage\b|\bfetch\s*\(",
    re.IGNORECASE,
)

# The user's provider selector and diagnostics are intentional presentation
# boundaries: their UI displays provider names, IDs and configured node types.
# Other QML surfaces must remain completely provider-neutral, and no QML may
# initiate network/database I/O. Keep this list narrow and auditable.
PROVIDER_SELECTION_QML = {
    "ui/components/NodeManagementPanel.qml",
    "ui/components/ProviderProfilesPanel.qml",
}
PROVIDER_AWARE_PRESENTATION = {
    "ui/mobile/provider_profiles_controller.cpp",
    "ui/mobile/channel_picker_controller.cpp",
    "ui/mobile/genypulse_activity_controller.cpp",
    "ui/tests/provider_profiles_controller_tests.cpp",
}

# Textual project headers are exceptional boundaries, never the default architecture.
# Isolated external module/BMI compatibility boundaries may use the dialect required by
# the reviewed third-party SDK. They are not part of the C++26 Farcaster core.
ALLOWED_CXX23_CMAKE = {
    pathlib.Path("services/media-gateway/CMakeLists.txt"),
}

ALLOWED_HPP = {
    pathlib.Path("src/storage/sqlite_boundary.hpp"),
    pathlib.Path("src/platform/signer_keychain_boundary.hpp"),
    # Reviewed third-party scure-bip39 English wordlist, compiled as inert data
    # inside the OpenSSL/C interface's global module fragment (never an API).
    pathlib.Path("src/platform/custody_bip39_english.hpp"),
    pathlib.Path("src/presentation/qt/presentation_bridge.hpp"),
    pathlib.Path("src/presentation/qt/localization_controller.hpp"),
    pathlib.Path("src/presentation/qt/appearance_controller.hpp"),
    pathlib.Path("src/presentation/qt/rendering_controller.hpp"),
    pathlib.Path("src/presentation/qt/rich_composer_controller.hpp"),
    pathlib.Path("src/presentation/qt/media_upload_controller.hpp"),
    pathlib.Path("src/presentation/qt/media_classifier_controller.hpp"),
    pathlib.Path("src/presentation/qt/media_playback_coordinator.hpp"),
    pathlib.Path("src/presentation/qt/emoji_controller.hpp"),
    pathlib.Path("src/presentation/qt/rich_inline_text_item.hpp"),
}

CORE_NO_QT = (
    "src/foundation/",
    "src/domain/",
    "src/capabilities/",
    "src/provider/",
    "src/local_state/",
    "src/application/",
    "src/routing/",
    "src/sync/",
    "src/storage/",
    "src/transport/",
)
QT_TRANSPORT_BOUNDARY = "src/transport/qt_network/"
MISPLACED_QT_TRANSPORT = SRC / "platform" / "qt_network"

FORBIDDEN_IMPORTS_BY_LAYER = {
    "src/domain/": ("farcaster.capability.", "farcaster.provider.", "farcaster.local.", "farcaster.application.", "farcaster.routing.", "farcaster.sync.", "farcaster.storage.", "farcaster.presentation."),
    "src/capabilities/": ("farcaster.provider.", "farcaster.local.", "farcaster.application.", "farcaster.routing.", "farcaster.sync.", "farcaster.storage.", "farcaster.presentation."),
    "src/local_state/": ("farcaster.provider.", "farcaster.application.", "farcaster.routing.", "farcaster.sync.", "farcaster.storage.", "farcaster.presentation."),
    "src/application/": ("farcaster.provider.", "farcaster.routing.", "farcaster.storage.", "farcaster.presentation."),
    "src/routing/": ("farcaster.storage.", "farcaster.presentation."),
    "src/sync/": ("farcaster.provider.", "farcaster.routing.", "farcaster.storage.", "farcaster.presentation."),
}

violations: list[str] = []


def relative(path: pathlib.Path) -> str:
    return path.relative_to(ROOT).as_posix()


def add(path: pathlib.Path, message: str) -> None:
    violations.append(f"{relative(path)}: {message}")


if MISPLACED_QT_TRANSPORT.exists():
    add(
        MISPLACED_QT_TRANSPORT,
        "Qt Network implements the transport port and must live under src/transport/qt_network",
    )


# Vague ownership buckets are forbidden by ARC-005/ARC-007.
for directory in SRC.rglob("*") if SRC.exists() else []:
    if directory.is_dir() and directory.name.lower() in {"utils", "helpers", "common", "misc"}:
        add(directory, "vague ownership directory is forbidden")

# Project-owned classic headers are forbidden except explicit compatibility boundaries.
for path in SRC.rglob("*.h") if SRC.exists() else []:
    add(path, "project-owned .h file is forbidden")

for path in SRC.rglob("*.hpp") if SRC.exists() else []:
    rel = path.relative_to(ROOT)
    if rel not in ALLOWED_HPP:
        add(path, ".hpp is allowed only at an explicit Qt/third-party/C/ABI boundary")
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    if "boundary" not in text.lower():
        add(path, "allowed .hpp boundary must document why textual compatibility is required")

for path in ROOT.rglob("*"):
    if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
        continue
    if "build" in path.parts:
        continue

    text = path.read_text(encoding="utf-8", errors="replace")
    rel = relative(path)

    if path.suffix in {".cpp", ".cppm"}:
        if re.search(r"\bimport\s+std\s*;", text):
            add(path, "import std is forbidden; use minimal standard headers in the global module fragment")

        module_match = MODULE_DECL.search(text)
        if module_match:
            module_name = module_match.group(1)
            if not MODULE_NAME.fullmatch(module_name):
                add(path, f"module name '{module_name}' is not dotted lowercase")

            # Includes in a named module unit must remain before the named module declaration.
            declaration_offset = module_match.start()
            for include in re.finditer(r"^\s*#\s*include\b", text, re.MULTILINE):
                if include.start() > declaration_offset:
                    add(path, "#include appears inside named-module purview")

            if "#include" in text:
                prefix = text[:declaration_offset]
                if not re.search(r"^\s*module\s*;", prefix, re.MULTILINE):
                    add(path, "module unit with textual includes is missing a global module fragment")

        export_match = EXPORT_MODULE_DECL.search(text)
        if export_match and path.suffix == ".cppm":
            namespace_match = EXPORT_NAMESPACE.search(text)
            if namespace_match:
                expected_namespace = export_match.group(1).replace(".", "::")
                actual_namespace = namespace_match.group(1)
                # A gateway module may export additional types in its owning
                # namespace (routing::write, routing::signer) without creating
                # a fictitious routing::write::gateway namespace in its ABI.
                matches_module = actual_namespace == expected_namespace
                owning_gateway = (
                    expected_namespace.endswith("::gateway")
                    and actual_namespace == expected_namespace.removesuffix("::gateway")
                )
                if not (matches_module or owning_gateway):
                    add(path, f"export namespace '{actual_namespace}' must mirror module identity '{expected_namespace}'")
            else:
                add(path, "exported module interface must expose a namespace matching its module identity")

        for enumMatch in ENUM_CLASS.finditer(text):
            enumBody = re.sub(r"/\*.*?\*/|//.*?$", "", enumMatch.group("body"), flags=re.DOTALL | re.MULTILINE)
            for enumerator in ENUMERATOR.findall(enumBody):
                if "_" in enumerator or not enumerator[:1].isupper():
                    add(path, f"enum-class enumerator '{enumerator}' must use PascalCase")

        if (
            rel.startswith(CORE_NO_QT)
            and not rel.startswith(QT_TRANSPORT_BOUNDARY)
            and QT_DEPENDENCY.search(text)
        ):
            add(path, "Qt dependency leaked into non-presentation core")

        for layer, forbidden_prefixes in FORBIDDEN_IMPORTS_BY_LAYER.items():
            if rel.startswith(layer):
                for imported in IMPORT_DECL.findall(text):
                    if imported.startswith(forbidden_prefixes):
                        add(path, f"forbidden dependency direction imports '{imported}'")

        # Concrete vendor/backend names are allowed only inside provider adapters.
        if (
            PROVIDER_SEMANTICS.search(text)
            and not rel.startswith("src/providers/")
            and not rel.startswith("tests/providers/")
            and rel not in {
                "ui/mobile/main.cpp",
                "ui/mobile/runtime_composition.cpp",
                "ui/mobile/runtime_composition.cppm",
            } | PROVIDER_AWARE_PRESENTATION  # Explicit presentation/composition boundaries.
        ):
            add(path, "vendor/flagship-backend semantic leaked outside provider adapter boundary")

    if path.suffix == ".qml":
        if PROVIDER_SEMANTICS.search(text) and rel not in PROVIDER_SELECTION_QML:
            add(path, "provider/backend semantic leaked into QML")
        if QML_IO.search(text):
            add(path, "QML must not own network, websocket, or database I/O")

# Root build baseline is part of the pinned engineering contract.
root_cmake = ROOT / "CMakeLists.txt"
if root_cmake.exists():
    root_text = root_cmake.read_text(encoding="utf-8", errors="replace")
    minimum_match = re.search(r"cmake_minimum_required\s*\(\s*VERSION\s+(\d+)\.(\d+)", root_text, re.IGNORECASE)
    if not minimum_match or (int(minimum_match.group(1)), int(minimum_match.group(2))) < (3, 30):
        add(root_cmake, "cmake_minimum_required must be VERSION 3.30 or newer")
    if not re.search(r"set\s*\(\s*CMAKE_CXX_STANDARD\s+26\s*\)", root_text):
        add(root_cmake, "root project must select the C++26 reference path")

# CMake must not regress the project to an older language level or experimental std modules.
for path in [ROOT / "CMakeLists.txt", *ROOT.rglob("CMakeLists.txt"), *ROOT.glob("cmake/*.cmake")]:
    if not path.exists() or "build" in path.parts:
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    if "cxx_std_23" in text or "CMAKE_CXX_STANDARD 23" in text:
        relative_cmake = path.relative_to(ROOT)
        if relative_cmake not in ALLOWED_CXX23_CMAKE:
            add(path, "C++23 regression detected; this repository's reference path is C++26")
        elif "Quilibrium C++ SDK C++23 BMI compatibility boundary" not in text:
            add(path, "allowed C++23 compatibility boundary must document the external SDK/BMI reason")
    if any(term in text for term in ("CMAKE_EXPERIMENTAL_CXX_IMPORT_STD", "CMAKE_CXX_MODULE_STD", "CXX_MODULE_STD")):
        add(path, "experimental standard-library module configuration is forbidden")

if violations:
    print("Architecture guard failed:")
    for item in sorted(set(violations)):
        print(f"  - {item}")
    raise SystemExit(1)

print("Architecture guard passed.")
