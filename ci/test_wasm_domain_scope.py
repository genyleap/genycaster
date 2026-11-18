#!/usr/bin/env python3
"""The browser release is deployable exclusively to app.genycaster.xyz."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "ci/build_deploy_wasm_server.sh"


class WasmDomainScopeTest(unittest.TestCase):
    def test_web_release_origin_is_explicitly_scoped(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        web = workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0]
        self.assertIn("-DGENYCASTER_PUBLIC_ORIGIN=https://app.genycaster.xyz", web)
        self.assertIn("-DGENYCASTER_SIWF_ORIGIN=https://app.genycaster.xyz", web)
        self.assertNotIn("-DGENYCASTER_PUBLIC_ORIGIN=https://genycaster.xyz", web)
        from ci.release_artifacts import EXPECTED
        self.assertEqual(len(EXPECTED), 7)
        self.assertNotIn("GenyCaster-web-wasm", EXPECTED)
        self.assertNotIn("GenyCaster-android-armeabi-v7a", EXPECTED)

    def test_dedicated_https_vhost_keeps_landing_page_and_cookies_separate(self):
        nginx = (ROOT / "ci/app_genycaster_nginx.conf").read_text()
        self.assertEqual(nginx.count("server_name app.genycaster.xyz;"), 2)
        self.assertIn("root /var/www/app.genycaster.xyz/public;", nginx)
        self.assertNotIn("root /var/www/genycaster.xyz/public;", nginx)
        self.assertIn("proxy_set_header Host $host;", nginx)
        self.assertIn("ssl_certificate /etc/letsencrypt/live/app.genycaster.xyz/fullchain.pem", nginx)
        self.assertIn("location ^~ /api/v1/cloud/", nginx)
        self.assertIn("genycaster.html", nginx)

    def test_web_deployment_rejects_other_hosts_and_web_roots_before_build(self):
        cases = (
            ("https://app.genycaster.xyz", "/var/www/genycaster.xyz/public", ()),
            ("https://genycaster.xyz", "/var/www/app.genycaster.xyz/public", ()),
            ("https://app.genycaster.xyz", "/var/www/app.genycaster.xyz/public",
             ("--define", "GENYCASTER_PUBLIC_ORIGIN=https://genycaster.xyz")),
            ("https://app.genycaster.xyz", "/var/www/app.genycaster.xyz/public",
             ("--define", "GENYCASTER_SIWF_ORIGIN=https://genycaster.xyz")),
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
                self.assertNotIn("Build-only mode", result.stdout)


if __name__ == "__main__":
    unittest.main()
