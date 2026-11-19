#!/usr/bin/env python3
"""Release-safe canonical WASM routing and adaptive native-download regression."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from ci import activate_primary_wasm_site as primary
from ci import generate_native_release_catalog as native
from ci.release_artifacts import EXPECTED

ROOT = Path(__file__).resolve().parent.parent


class PrimaryMigrationTests(unittest.TestCase):
    def test_primary_nginx_is_exactly_scoped_and_preserves_landing_and_api(self):
        config = primary.check_template()
        self.assertIn("root /var/www/genycaster.xyz/public;", config)
        self.assertIn("location = /about/", config)
        self.assertIn("location = /download", config)
        self.assertIn("location ^~ /api/v1/cloud/", config)
        self.assertIn("location = /genycaster-sw.js", config)
        self.assertIn("location = /deployment_config.mjs", config)
        self.assertIn("location = /releases/native.json", config)
        self.assertIn('return 200 "genycaster-main-v1";', config)
        self.assertIn('location ~* "\\.[a-f0-9]{20}\\.wasm$"', config)
        self.assertNotIn("server_name app.genycaster.xyz;", config)
        self.assertNotIn("default-src 'none'", config)

    def test_real_nginx_parser_accepts_primary_vhost(self):
        if not shutil.which("nginx"):
            self.skipTest("Nginx unavailable")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nginx.conf"
            path.write_text(
                "events {}\nhttp {\ninclude /etc/nginx/mime.types;\n"
                "include " + str(primary.SOURCE) + ";\n}\n",
                encoding="utf-8")
            result = subprocess.run(
                ["nginx", "-t", "-c", str(path)],
                text=True, capture_output=True, timeout=15)
            if not primary.CERT.is_file():
                self.skipTest("Canonical TLS certificate unavailable in test environment")
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_boot_shell_contains_accessible_platform_dialog(self):
        shell = (ROOT / "ui/wasm/genycaster-shell.html").read_text(encoding="utf-8")
        for name in ("native-downloads-dialog", "native-downloads-trigger",
                     "native-downloads-architecture", "native-downloads-backdrop",
                     "native-downloads-nudge", "native-downloads-status"):
            self.assertIn(f'id="{name}"', shell)
        self.assertIn('aria-modal="true"', shell)
        self.assertIn('href="native-downloads.css"', shell)
        self.assertIn('src="native-downloads.js"', shell)
        css = (ROOT / "ui/wasm/native-downloads.css").read_text()
        self.assertIn("max-width:640px", css)
        self.assertIn("prefers-reduced-motion:reduce", css)
        self.assertIn("safe-area-inset-bottom", css)
        js = (ROOT / "ui/wasm/native-downloads.js").read_text()
        self.assertIn('fetch("/releases/native.json"', js)
        self.assertIn('response.ok', js)
        self.assertIn("sha256", js)
        self.assertIn("Escape", js)
        self.assertIn("credentials: \"omit\"", js)
        self.assertIn("github.com/genyleap/genycaster/releases/download/", js)
        if shutil.which("node"):
            check = subprocess.run(
                ["node", "--check", str(ROOT / "ui/wasm/native-downloads.js")],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(check.returncode, 0, check.stderr)

    def test_unreleased_platforms_never_get_invented_links(self):
        catalog = json.loads(
            (ROOT / "ui/wasm/native-releases.json").read_text(encoding="utf-8"))
        self.assertEqual(catalog, {"schema": 1, "platforms": {}})

    def test_unified_catalog_from_exact_published_release_artifacts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sums = []
            for artifact, suffix in EXPECTED.items():
                name = f"GenyCaster-v1.0.0-{artifact.removeprefix('GenyCaster-')}{suffix}"
                content = ("fixture-" + artifact).encode("utf-8")
                (root / name).write_bytes(content)
                sums.append(f"{hashlib.sha256(content).hexdigest()}  {name}")
            (root / "SHA256SUMS").write_text("\n".join(sums) + "\n")
            browser, landing = native.generate(root, "v1.0.0")
            self.assertEqual(browser["schema"], 1)
            self.assertEqual(len(browser["platforms"]["macos"]), 2)
            self.assertEqual(len(browser["platforms"]["windows"]), 2)
            self.assertEqual(len(browser["platforms"]["linux"]), 2)
            self.assertEqual(len(browser["platforms"]["android"]), 1)
            self.assertEqual(len(landing["platforms"]["ios"]["assets"]), 0)
            link = browser["platforms"]["windows"]["arm64"]["url"]
            self.assertIn("/releases/download/v1.0.0/", link)
            self.assertEqual(len(browser["platforms"]["windows"]["arm64"]["sha256"]), 64)
            # A wrong checksum or an unavailable installer must never
            # enable a native download on either website.
            broken = sums.copy()
            broken[0] = "0" * 64 + "  " + broken[0].split(maxsplit=1)[1]
            (root / "SHA256SUMS").write_text("\n".join(broken) + "\n")
            with self.assertRaises(ValueError):
                native.generate(root, "v1.0.0")
            with self.assertRaises(ValueError):
                native.generate(root, "not-a-version")

    def test_deploy_preview_preserves_about_and_limits_write_scope(self):
        prepare = (ROOT / "ci/prepare_primary_wasm_preview.py").read_text()
        self.assertIn('ROOT = Path("/var/www/genycaster.xyz/public")', prepare)
        self.assertIn('about = ROOT / "about/index.html"', prepare)
        self.assertIn("about.parent.chmod(0o755)", prepare)
        self.assertIn("target.parent.chmod(0o755)", prepare)
        self.assertIn('VERSION = "1.0.0-rc1"',
                      (ROOT / "ci/stage_wasm_preview.py").read_text())

if __name__ == "__main__":
    unittest.main()
