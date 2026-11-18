#!/usr/bin/env python3
"""Run local/server release acceptance without triggering hosted GitHub builds.

The static gate checks source, security and packaging contracts. Native mode
additionally requires a *real* built Linux executable and exercises the
headless QML shell and deterministic local fake-provider runtime. Acceptance
does not establish macOS/Windows/iOS/Android ABI compatibility or signing:
those require their platform runners/devices and verified deliverables.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

CHECKS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("CI source layout", ("python3", "ci/verify_ci.py")),
    ("Nginx prelaunch hold preserves the authenticated cloud API",
     ("python3", "-m", "unittest", "-q", "ci.test_prelaunch_cloud_nginx")),
    ("genycaster.xyz canonical account API proxy leaves landing intact",
     ("python3", "-m", "unittest", "-q", "ci.test_genycaster_xyz_cloud_api")),
    ("Core provider-boundary lexical guards",
     ("python3", "-m", "unittest", "-q", "ci.test_source_comments")),
    ("Core architecture and module documentation", ("python3", "ci/check_architecture.py")),
    ("All supported locale catalogs", ("python3", "ci/check_localization_catalog.py")),
    ("Product-specific palette semantic roles",
     ("node", "--test", "tests/wasm/semantic_product_palette_contract.test.mjs")),
    ("Theme palette and RTL-neutral localized process labels",
     ("node", "--test", "tests/wasm/setup_theme_palette_l10n_contract.test.mjs")),
    ("Semantic market and feedback scene-graph shapes",
     ("node", "--test", "tests/wasm/market_feedback_semantic_shapes_20261118.test.mjs")),
    ("Shared Drawer motion and BidiTextArea selection grip geometry",
     ("node", "--test", "tests/wasm/drawer_selection_semantic_surface_contract_20261118.test.mjs")),
    ("Mini App, navigation, composer and media QSG surface geometry",
     ("node", "--test", "tests/wasm/semantic_small_surfaces_20261118.test.mjs")),
    ("Deferred mixed-media cast supporting-card admission",
     ("node", "--test", "tests/wasm/cast_mixed_supporting_admission_20261118.test.mjs")),
    ("Profile link and node progress retained QSG shapes",
     ("node", "--test", "tests/wasm/profile_node_retained_round_surfaces_20261118.test.mjs")),
    ("Market, statistics and workspace retained QSG round indicators",
     ("node", "--test", "tests/wasm/market_stats_workspace_rounding_20261118.test.mjs")),
    ("Connections and settings retained QSG shapes",
     ("node", "--test", "tests/wasm/connection_settings_retained_controls_20261118.test.mjs")),
    ("Async resource limits", ("python3", "ci/check_async_resource_contract.py")),
    ("Data storage policies", ("python3", "ci/check_data_storage_contract.py")),
    ("Media upload policies", ("python3", "ci/check_media_upload_contract.py")),
    ("Previous release regression contract", ("python3", "ci/check_r5_release_contract.py")),
    ("Rich publishing regression contract", ("python3", "ci/check_r7_release_contract.py")),
    ("Release notes/version gate", ("python3", "ci/release_gate.py")),
    ("Browser/native platform boundary",
     ("node", "--test", "tests/wasm/native_executable_and_platform_scope.test.mjs")),
    ("Provider-neutral identity presentation and native signer authorization",
     ("node", "--test", "tests/wasm/provider_neutral_identity_presentation.test.mjs")),
    ("Verified profile FID namespace and localized fallback",
     ("node", "--test", "tests/wasm/profile_fid_namespace_label_contract.test.mjs")),
    ("Native custody worker lifetime contracts",
     ("node", "--test", "tests/wasm/native_custody_worker_lifetime.test.mjs")),
    ("Protocol-scoped HyperSnap account Streak and CSRF acceptance",
     ("node", "--test", "services/account-sync/test/native_signer_session.test.mjs")),
    ("Browser startup and Mini App identity contracts",
     ("node", "--test", "tests/wasm/wasm_startup_modes.test.mjs",
      "tests/wasm/miniapp_sdk_contract.test.mjs")),
    ("Browser scroll and provider isolation contracts",
     ("node", "--test", "tests/wasm/wheel_bridge.test.mjs",
      "tests/wasm/wheel_pull_refresh_notifications.test.mjs",
      "tests/wasm/provider_neutral_signer_routing.test.mjs",
      "tests/wasm/provider_neutral_routes.test.mjs",
      "tests/wasm/provider_neutral_write_routing.test.mjs",
      "tests/wasm/provider_profiles_ui_contract.test.mjs",
      "tests/wasm/setup_authenticated_desktop_fps_20261104.test.mjs",
      "tests/wasm/compatible_read_fallback_consent.test.mjs",
      "tests/wasm/profile_block_protocol_contract.test.mjs")),
    ("Native update integrity and package regression suite",
     ("python3", "-m", "unittest",
      "ci.test_runtime_configuration", "ci.test_updates",
      "ci.test_packaging", "ci.test_pipeline",
      "ci.test_selfupdate_verification")),
)


def execute(label: str, args: tuple[str, ...], *,
            env: dict[str, str] | None = None,
            timeout: int = 120) -> bool:
    print(f"[acceptance] {label}:", flush=True)
    try:
        result = subprocess.run(args, cwd=ROOT, env=env, timeout=timeout,
                                text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, check=False)
    except (OSError, subprocess.TimeoutExpired) as failure:
        print(f"  FAIL: {failure}", flush=True)
        return False
    if result.returncode != 0:
        print(f"  FAIL (exit {result.returncode})", flush=True)
        for stream in (result.stdout, result.stderr):
            if stream.strip():
                print(stream[-6000:], flush=True)
        return False
    print("  PASS", flush=True)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--native-executable", type=Path,
        help="Run actual headless shell and fake-provider startup checks")
    parser.add_argument(
        "--strict-design", action="store_true",
        help="Include the full visual design/style debt gate")
    args = parser.parse_args()

    errors = []
    for name, command in CHECKS:
        if not execute(name, command):
            errors.append(name)

    if args.strict_design:
        if not execute("Cross-platform Release feature parity",
                       ("python3", "ci/verify_release_feature_contract.py", "--strict")):
            errors.append("Cross-platform Release feature parity")
        if not execute("Full UI design audit", ("python3", "ci/check_ui_design.py")):
            errors.append("Full UI design audit")

    if args.native_executable:
        binary = args.native_executable.expanduser().resolve()
        if not binary.is_file():
            errors.append("Native executable not built")
            print("[acceptance] FAIL: native executable missing", flush=True)
        else:
            env = os.environ.copy()
            env["QT_QPA_PLATFORM"] = "offscreen"
            env["QT_QUICK_BACKEND"] = "software"
            env["QSG_RHI_BACKEND"] = "software"
            # The installed project can use a caller-supplied Qt/GCC library
            # path. Do not overwrite LD_LIBRARY_PATH or touch user settings.
            with tempfile.TemporaryDirectory(prefix="genycaster-acceptance-") as temp:
                settings = str(Path(temp) / "settings.ini")
                if not execute("Headless QML shell", (
                    str(binary), "--smoke-test", f"--settings-path={settings}"),
                        env=env, timeout=45):
                    errors.append("Headless QML shell")
            if not execute("Real runtime against isolated fake providers", (
                "python3", "tests/ui/runtime_smoke.py", str(binary)),
                    env=env, timeout=120):
                errors.append("Real runtime against isolated fake providers")

    if errors:
        print("\nACCEPTANCE FAILED: " + "; ".join(errors), flush=True)
        return 1
    print("\nACCEPTANCE PASSED: all requested source/runtime checks successful",
          flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
