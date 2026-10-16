#!/usr/bin/env python3
"""Run the OpenProof provider contract tests with the build compiler's C++ runtime."""
import os
from pathlib import Path
import subprocess
import sys

base = Path(__file__).resolve().parents[1]
test = base / "build/ui-debug/tests/providers/openproof_auth/openproof_auth_contract_tests"
if not test.is_file():
    sys.exit("OpenProof contract test binary is missing; build the target first")
environment = os.environ.copy()
compiler_runtime = Path("/opt/gcc-16.2.0/lib64")
if (compiler_runtime / "libstdc++.so.6").is_file():
    environment["LD_LIBRARY_PATH"] = str(compiler_runtime) + ":" + environment.get("LD_LIBRARY_PATH", "")
result = subprocess.run([str(test)], env=environment, check=False, timeout=45)
print("OpenProof contract:", "PASS" if result.returncode == 0 else "FAIL")
sys.exit(result.returncode)
