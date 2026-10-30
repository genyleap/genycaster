#!/usr/bin/env python3
"""Fast guard for accidental per-commit CI/release triggers and release bypasses."""
from __future__ import annotations

from pathlib import Path
import re


def main() -> None:
    workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
    assert re.search(r"^on:\n  workflow_dispatch:", workflow, re.M)
    # Explicit matrix requests, not arbitrary branch pushes, trigger CI.
    assert re.search(
        r"^  push:\n    branches:\n      - main\n    paths:\n"
        r"      - 'ci/matrix-run-request\.txt'\n    tags:\n      - 'v\*'",
        workflow, re.M,
    )
    assert not re.search(r"^  (pull_request|schedule|release):", workflow, re.M)
    assert re.search(r"^  publish:\n", workflow, re.M)
    assert "github.event_name == 'push' && needs.gate.outputs.release == 'true'" in workflow
    assert "needs: [gate, core, linux, macos, windows, android, web]" in workflow
    assert "FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON" in workflow
    assert "linux_gcc_arm64" in workflow
    assert "macos-15-intel" in workflow
    assert "windows-11-arm" in workflow
    assert "android_armv7" in workflow
    assert "name: iOS arm64 unsigned device app" in workflow
    assert "name: NixOS" in workflow
    assert "GenyCaster-linux-arm64" in workflow
    assert "GenyCaster-windows-arm64" in workflow
    assert "GenyCaster-android-armeabi-v7a" in workflow
    assert "            vcpkg_triplet: x64-windows" in workflow
    assert "            vcpkg_triplet: arm64-windows" in workflow
    assert "            vcpkg_triplet: ${{ matrix.vcpkg_triplet }}" not in workflow
    assert "ANDROID_KEYSTORE_BASE64" in workflow
    assert "if-no-files-found: error" in workflow
    assert "Refuse overwriting an existing release" in workflow
    assert "ci/release_gate.py" in workflow
    assert "ci/release_artifacts.py" in workflow
    assert not Path("tools").exists(), "legacy tools/ directory must not return"
    assert not Path("docs-site").exists(), "obsolete docs-site/ directory must not return"
    assert "tools/linuxdeploy" not in workflow
    assert "build/ci-linuxdeploy/" in workflow
    assert "version=3.5.9" in workflow
    assert "tools/build_android_openssl.sh" not in workflow
    assert "Prepare Android OpenSSL 3.5.9 libraries" in workflow
    assert "$ANDROID_HOME/cmdline-tools/latest/bin" in workflow
    assert "$ANDROID_HOME/cmdline-ci" not in workflow
    assert "sdkmanager_bin/sdkmanager" in workflow
    assert 'export LDFLAGS="-Wl,-z,max-page-size=16384' in workflow
    # OpenSSL must emit the final Android SONAMEs itself; post-link
    # PatchELF corrupted DT_VERNEED/PT_LOAD and caused QtLoader startup crashes.
    assert '"android-genycaster-arm64" =>' in workflow
    assert 'inherit_from => [ "android-arm64" ]' in workflow
    assert 'shlib_variant => "_3"' in workflow
    assert './Configure "android-genycaster-${{ matrix.openssl_arch }}" shared no-tests' in workflow
    assert '"android-genycaster-arm" =>' in workflow
    assert "arm64-v8a" in workflow and "armeabi-v7a" in workflow
    assert "patchelf --set-soname" not in workflow
    assert "patchelf --page-size" not in workflow
    assert 'llvm-readelf" -d "$libdir/$lib" > "$libdir/$lib.dynamic.txt"' in workflow
    assert 'grep -Fq "Library soname: [$lib]" "$libdir/$lib.dynamic.txt"' in workflow
    assert "grep -Fq 'Shared library: [libcrypto_3.so]' " in workflow
    assert '"$ANDROID_HOME/build-tools/35.0.0/zipalign" -c -P 16' in workflow
    assert "validate_android_elf_alignment" in Path(
        "ci/verify_platform_package.py"
    ).read_text(encoding="utf-8")
    assert "add_link_options(" in Path("CMakeLists.txt").read_text(encoding="utf-8")
    assert "$ANDROID_HOME/build-tools/35.0.0/apksigner" in workflow
    assert "genycaster-ci-test-signed.apk" in workflow
    assert '"$signer" verify --verbose --print-certs "$selected"' in workflow
    assert "sha256sum --check" in workflow
    for module_path, target in (
        ("src/presentation/media/CMakeLists.txt", "farcaster_presentation_media"),
        ("src/providers/hypersnap/CMakeLists.txt", "farcaster_provider_hypersnap"),
        ("src/providers/public_data/CMakeLists.txt", "farcaster_provider_public_data"),
    ):
        module_cmake = Path(module_path).read_text(encoding="utf-8")
        assert f"add_library({target} STATIC)" in module_cmake, (
            f"{target} must retain its implementation symbols on Windows"
        )
    print("CI trigger, source layout and release gate invariants passed")


if __name__ == "__main__":
    main()
