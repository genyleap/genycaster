#!/usr/bin/env python3
"""Offline regression checks for dedicated GenyCaster HTTPS site activation."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import MagicMock, patch

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
                          return_value=Response(site.MARKER)) as opened:
            site.check_public_marker()
            request = opened.call_args.args[0]
            self.assertEqual(request.full_url, site.PROBE)
            self.assertEqual(request.get_header("User-agent"), "curl/8.5.0")
            self.assertEqual(request.get_header("Cache-control"), "no-cache")
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

    def test_origin_probe_uses_loopback_tls_with_real_hostname(self):
        raw = MagicMock()
        connection = MagicMock()
        raw.__enter__.return_value = raw
        connection.__enter__.return_value = connection
        context = MagicMock()
        context.wrap_socket.return_value = connection
        response = MagicMock()
        response.status = 200
        response.read.side_effect = [
            site.MARKER.encode("ascii"), b"<!doctype html><html>"
        ]
        with patch.object(site.socket, "create_connection", return_value=raw) as dial, \
             patch.object(site.ssl, "create_default_context", return_value=context), \
             patch.object(site.http.client, "HTTPResponse", return_value=response):
            site.check_origin_service()
        self.assertEqual(dial.call_count, 2)
        self.assertEqual(dial.call_args.args[0], ("127.0.0.1", 443))
        self.assertEqual(context.wrap_socket.call_args.kwargs["server_hostname"],
                         site.HOST)
        self.assertEqual(connection.sendall.call_count, 2)
        self.assertIn(b"Host: app.genycaster.xyz",
                      connection.sendall.call_args_list[0].args[0])

    def test_cdn_failure_preserves_verified_origin_vhost(self):
        # A public 404 is not grounds for removing a successfully verified
        # localhost HTTPS vhost. No Certbot ceremony is needed when a valid
        # certificate already exists.
        with tempfile.TemporaryDirectory() as temp:
            available = Path(temp) / "app.available"
            enabled = Path(temp) / "app.enabled"
            cert = Path(temp) / "cert.pem"
            key = Path(temp) / "key.pem"
            cert.write_text("fixture")
            key.write_text("fixture")
            with patch.object(site, "AVAILABLE", available), \
                 patch.object(site, "ENABLED", enabled), \
                 patch.object(site, "CERT", cert), \
                 patch.object(site, "KEY", key), \
                 patch.object(site, "verify_ownership"), \
                 patch.object(site, "verify_certificate"), \
                 patch.object(site, "check_origin_service") as local, \
                 patch.object(site, "report_public_delivery", return_value=False), \
                 patch.object(site.os, "geteuid", return_value=0), \
                 patch.object(site.shutil, "which", return_value="/bin/true"), \
                 patch.object(site, "run_checked") as command:
                self.assertFalse(site.activate(None))
                self.assertTrue(available.is_file())
                self.assertTrue(enabled.is_symlink())
                self.assertEqual(enabled.resolve(), available)
                self.assertTrue(local.called)
                self.assertNotIn("certbot", [c.args[0] for c in command.call_args_list])
                with patch.object(site, "report_public_delivery", return_value=True):
                    previous_calls = command.call_count
                    self.assertTrue(site.activate(None))
                    self.assertEqual(command.call_count, previous_calls)

    def test_failed_local_origin_rolls_back_new_vhost(self):
        with tempfile.TemporaryDirectory() as temp:
            available = Path(temp) / "app.available"
            enabled = Path(temp) / "app.enabled"
            cert = Path(temp) / "cert.pem"
            key = Path(temp) / "key.pem"
            cert.write_text("fixture")
            key.write_text("fixture")
            with patch.object(site, "AVAILABLE", available), \
                 patch.object(site, "ENABLED", enabled), \
                 patch.object(site, "CERT", cert), \
                 patch.object(site, "KEY", key), \
                 patch.object(site, "verify_ownership"), \
                 patch.object(site, "verify_certificate"), \
                 patch.object(site, "check_origin_service",
                              side_effect=RuntimeError("origin failed")), \
                 patch.object(site.os, "geteuid", return_value=0), \
                 patch.object(site.shutil, "which", return_value="/bin/true"), \
                 patch.object(site, "run_checked"):
                with self.assertRaisesRegex(RuntimeError, "origin failed"):
                    site.activate(None)
                self.assertFalse(available.exists())
                self.assertFalse(enabled.is_symlink())

    def test_launch_defaults_to_nonmutating_dry_run(self):
        with patch("sys.argv", ["activate_app_site.py"]):
            self.assertEqual(site.main(), 0)
        self.assertTrue(Path(site.SOURCE).is_file())


if __name__ == "__main__":
    unittest.main()
