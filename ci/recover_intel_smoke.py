#!/usr/bin/env python3
"""Run the *already built and signed* Intel app's real QML startup smoke.

This script is only for recovering a preserved v1.0.0 macOS Intel bundle.
Never treat a timeout, crash or failed QML readiness check as a passing test.
The same proven Qt Quick backend must work both before and after DMG mounting.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile


# First try the native GPU path on the macOS 15 Intel runner, then two
# bounded alternatives. None changes the distributed application.
BACKENDS = {
    "metal-basic": (None, None),
    "software-basic": ("software", None),
    "opengl-basic": (None, "opengl"),
}
MAX_SECONDS = 75

UNTRUSTED_QT_PATHS = (
    "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH",
    "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_FONTDIR", "DYLD_LIBRARY_PATH",
    "DYLD_FRAMEWORK_PATH", "DYLD_FALLBACK_LIBRARY_PATH",
    "DYLD_FALLBACK_FRAMEWORK_PATH",
)


def smoke(executable: Path, *, backend: str, stage: str, diagnostics: Path) -> bool:
    qt_backend, rhi_backend = BACKENDS[backend]
    log = diagnostics / f"{stage}-{backend}.log"
    with tempfile.TemporaryDirectory(prefix="genycaster-intel-smoke-") as home:
        env = os.environ.copy()
        for key in UNTRUSTED_QT_PATHS:
            env.pop(key, None)
        env.update({
            "HOME": home,
            "QT_QPA_PLATFORM": "cocoa",
            "QSG_RENDER_LOOP": "basic",
            "QSG_INFO": "1",
            "QML_DISABLE_DISK_CACHE": "1",
        })
        env.pop("QT_QUICK_BACKEND", None)
        env.pop("QSG_RHI_BACKEND", None)
        if qt_backend:
            env["QT_QUICK_BACKEND"] = qt_backend
        if rhi_backend:
            env["QSG_RHI_BACKEND"] = rhi_backend
        command = [
            str(executable.resolve()), "--smoke-test",
            f"--settings-path={Path(home) / 'settings.ini'}",
        ]
        timed_out = False
        with log.open("wb") as output:
            process = subprocess.Popen(
                command, env=env, stdout=output, stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            try:
                code = process.wait(timeout=MAX_SECONDS)
            except subprocess.TimeoutExpired:
                timed_out = True
                # Best-effort stack capture. The failure still fails closed.
                try:
                    with (diagnostics / f"{stage}-{backend}.sample.txt").open("wb") as sample:
                        subprocess.run(
                            ["/usr/bin/sample", str(process.pid), "3"],
                            stdout=sample, stderr=subprocess.STDOUT,
                            timeout=12, check=False,
                        )
                except (OSError, subprocess.TimeoutExpired):
                    pass
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=10)
                code = -1
        report = log.read_text(encoding="utf-8", errors="replace")
        valid = (not timed_out and code == 0 and
                 "QQmlApplicationEngine failed to load component" not in report)
        print(f"Intel {stage} QML smoke [{backend}]: "
              f"{'PASS' if valid else ('TIMEOUT' if timed_out else 'FAIL')} "
              f"(exit={code})", flush=True)
        print(report[-5000:], flush=True)
        return valid


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("app", type=Path)
    parser.add_argument("--stage", required=True, choices=("bundle", "mounted"))
    parser.add_argument("--diagnostics", required=True, type=Path)
    parser.add_argument("--selection-file", required=True, type=Path)
    args = parser.parse_args()
    executable = args.app / "Contents/MacOS/Genycaster"
    if not executable.is_file():
        parser.error(f"Missing signed Genycaster executable: {executable}")
    args.diagnostics.mkdir(parents=True, exist_ok=True)
    if args.selection_file.exists():
        chosen = args.selection_file.read_text(encoding="ascii").strip()
        if chosen not in BACKENDS:
            parser.error("Invalid previously selected Qt Quick backend")
        candidates = (chosen,)
    else:
        if args.stage != "bundle":
            parser.error("Mounted DMG cannot choose a new untested backend")
        candidates = tuple(BACKENDS)
    for backend in candidates:
        if smoke(executable, backend=backend, stage=args.stage,
                 diagnostics=args.diagnostics):
            if args.stage == "bundle":
                args.selection_file.write_text(backend + "\n", encoding="ascii")
            return
    raise SystemExit(f"macOS Intel {args.stage} packaged QML smoke failed; "
                     f"diagnostics in {args.diagnostics}")


if __name__ == "__main__":
    main()
