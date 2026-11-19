#!/usr/bin/env python3
"""Canonical genycaster.xyz is the only primary WASM deployment origin."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "ci/build_deploy_wasm_server.sh"


class WasmDomainScopeTest(unittest.TestCase):
    def test_web_release_origin_is_explicitly_scoped(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        web = workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0]
        self.assertIn("-DGENYCASTER_PUBLIC_ORIGIN=https://genycaster.xyz", web)
        self.assertIn("-DGENYCASTER_SIWF_ORIGIN=https://genycaster.xyz", web)
        self.assertNotIn("-DGENYCASTER_PUBLIC_ORIGIN=https://app.genycaster.xyz", web)
        from ci.release_artifacts import EXPECTED
        self.assertEqual(len(EXPECTED), 7)
        self.assertNotIn("GenyCaster-web-wasm", EXPECTED)
        self.assertNotIn("GenyCaster-android-armeabi-v7a", EXPECTED)

    def test_primary_vhost_preserves_landing_and_account_api(self):
        nginx = (ROOT / "ci/primary_genycaster_nginx.conf").read_text()
        self.assertEqual(nginx.count("server_name genycaster.xyz;"), 2)
        self.assertIn("root /var/www/genycaster.xyz/public;", nginx)
        self.assertIn("location = /about/", nginx)
        self.assertIn("location = /download", nginx)
        self.assertIn("try_files /genycaster.html =404;", nginx)
        self.assertIn("proxy_set_header Host $host;", nginx)
        self.assertIn("ssl_certificate /etc/letsencrypt/live/genycaster.xyz/fullchain.pem", nginx)
        self.assertIn("location ^~ /api/v1/cloud/", nginx)
        self.assertIn('return 200 "genycaster-main-v1";', nginx)

    def test_web_deployment_rejects_wrong_host_or_root_before_build(self):
        cases = (
            ("https://app.genycaster.xyz", "/var/www/genycaster.xyz/public", ()),
            ("https://genycaster.xyz", "/var/www/app.genycaster.xyz/public", ()),
            ("https://genycaster.xyz", "/var/www/genycaster.xyz/public",
             ("--define", "GENYCASTER_PUBLIC_ORIGIN=https://app.genycaster.xyz")),
            ("https://genycaster.xyz", "/var/www/genycaster.xyz/public",
             ("--define", "GENYCASTER_SIWF_ORIGIN=https://app.genycaster.xyz")),
        )
        for host, root, extra in cases:
            with self.subTest(host=host, root=root, extra=extra):
                env = {**os.environ, "GENYCASTER_SITE_URL": host}
                result = subprocess.run(
                    ["bash", str(SCRIPT), "--web-root", root, *extra],
                    cwd=ROOT, env=env, text=True, capture_output=True, timeout=5,
                )
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("WASM deployment", result.stderr)

    def test_deployment_rejects_wrong_https_vhost_before_build(self):
        nginx = (ROOT / "ci/primary_genycaster_nginx.conf").read_text()
        self.assertIn("location = /.well-known/genycaster-ready", nginx)
        self.assertIn('return 200 "genycaster-main-v1";', nginx)
        source = SCRIPT.read_text()
        self.assertIn("--proto '=https'", source)
        self.assertIn("--max-redirs 0", source)
        self.assertIn("/.well-known/genycaster-ready", source)

        with tempfile.TemporaryDirectory() as temp:
            fake_curl = Path(temp) / "curl"
            fake_curl.write_text("#!/bin/sh\nprintf '%s' 'Genyleap Status'\n")
            fake_curl.chmod(0o755)
            env = os.environ.copy()
            env["GENYCASTER_SITE_URL"] = "https://genycaster.xyz"
            env["PATH"] = temp + os.pathsep + env.get("PATH", "")
            env.pop("QT_ROOT_DIR", None)
            env.pop("QT_HOST_PATH", None)
            args = ["bash", str(SCRIPT), "--web-root",
                    "/var/www/genycaster.xyz/public"]
            result = subprocess.run(args, cwd=ROOT, env=env,
                                    text=True, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn("not serving the canonical HTTPS WASM vhost",
                          result.stderr)
            fake_curl.write_text("#!/bin/sh\nprintf '%s' 'genycaster-main-v1'\n")
            result = subprocess.run(args, cwd=ROOT, env=env,
                                    text=True, capture_output=True, timeout=5)
            self.assertNotIn("not serving the canonical HTTPS WASM vhost",
                             result.stderr)
            self.assertIn("QT_ROOT_DIR", result.stderr)


if __name__ == "__main__":
    unittest.main()
