#!/usr/bin/env python3
"""Offline regression checks for dedicated GenyCaster HTTPS site activation."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from ci import activate_app_site as site


class Response:
    status = 200

    def __init__(self, body: str, url: str = site.PROBE):
        self.body = body
        self.url = url

    def geturl(self) -> str:
        return self.url

    def read(self, count: int) -> bytes:
        return self.body.encode("utf-8")[:count]

    def __enter__(self) -> "Response":
        return self

    def __exit__(self, *_: object) -> None:
        return None


class AppSiteActivationTest(unittest.TestCase):
    def test_reviewed_https_template_is_exactly_scoped(self):
        text = site.SOURCE.read_text(encoding="utf-8")
        site.validate_config(text)
        self.assertEqual(text.count(f"server_name {site.HOST};"), 2)
        self.assertIn(f"ssl_certificate {site.CERT};", text)

    def test_nginx_regex_quantifier_is_quoted(self):
        # An unquoted {20} in a location regexp is tokenized as a
        # directive delimiter by Nginx: unknown directive "20}\\.wasm$".
        source = site.SOURCE.read_text(encoding="utf-8")
        self.assertIn('location ~* "\\\\.[a-f0-9]{20}\\\\.wasm$" {', source)
        self.assertNotIn("location ~* \\\\.[a-f0-9]{20}", source)

    def test_nginx_binary_parses_site_regexes(self):
        if not shutil.which("nginx"):
            self.skipTest("nginx not available on this host")
        source = site.SOURCE.read_text(encoding="utf-8")
        locations = [line.strip() for line in source.splitlines()
                     if line.strip().startswith("location ~* ")]
        self.assertGreaterEqual(len(locations), 3)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "nginx.conf"
            lines = ["events {}", "http {", "  server {",
                     "    listen 127.0.0.1:18080;", "    server_name localhost;"]
            for location in locations:
                lines.extend((f"    {location}", "        return 204;", "    }"))
            lines.extend(("  }", "}"))
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            result = subprocess.run(["nginx", "-t", "-c", str(path)],
                                    text=True, capture_output=True, timeout=8)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_landing_page_capture_or_missing_marker_rejected(self):
        original = site.SOURCE.read_text(encoding="utf-8")
        for changed in (
            original.replace(f"server_name {site.HOST};",
                             "server_name genycaster.xyz;", 1),
            original.replace('return 200 "genycaster-app-v1";',
                             'return 200 "status-page";'),
            original.replace(str(site.ROOT), "/var/www/genycaster.xyz/public"),
        ):
            with self.subTest(changed=changed[:40]):
                with self.assertRaises(ValueError):
                    site.validate_config(changed)

    def test_acme_bootstrap_has_no_tls_dependency_or_other_domain(self):
        conf = site.http_certificate_bootstrap()
        self.assertIn(f"server_name {site.HOST};", conf)
        self.assertIn("/.well-known/acme-challenge/", conf)
        self.assertNotIn("listen 443", conf)
        self.assertNotIn("genycaster.xyz;", conf.replace(site.HOST, ""))
        self.assertTrue(conf.startswith(site.MANAGED))

    def test_https_marker_requires_exact_content_and_origin(self):
        with patch.object(site.urllib.request, "urlopen",
                          return_value=Response(site.MARKER)):
            site.check_public_marker()
        for response in (
            Response("Genyleap System Status"),
            Response("genycaster-app-v1\n"),
            Response(site.MARKER, "https://status.genyleap.com/"),
        ):
            with self.subTest(response=response):
                with patch.object(site.urllib.request, "urlopen",
                                  return_value=response):
                    with self.assertRaises(RuntimeError):
                        site.check_public_marker()

    def test_launch_defaults_to_nonmutating_dry_run(self):
        with patch("sys.argv", ["activate_app_site.py"]):
            self.assertEqual(site.main(), 0)
        self.assertTrue(Path(site.SOURCE).is_file())


if __name__ == "__main__":
    unittest.main()
