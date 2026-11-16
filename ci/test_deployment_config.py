"""Deployment configuration is data, never executable C++/JS/CMake source."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("generate_deployment_config.py")
spec = importlib.util.spec_from_file_location("genycaster_deployment", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

DEFAULT = {
    "GENYCASTER_PUBLIC_ORIGIN": "https://genycaster.xyz",
    "GENYCASTER_SIWF_ORIGIN": "https://genycaster.xyz",
    "GENYCASTER_OPENPROOF_ISSUER": "https://openproof.genyleap.com",
    "GENYCASTER_OPENPROOF_BROWSER_BASE": "https://genyleap.com/openproof",
    "GENYCASTER_BROWSER_GATEWAY_ORIGIN": "https://storage-farcaster.genyleap.com",
    "GENYCASTER_MEDIA_PUBLIC_ORIGIN": "https://media-farcaster.genyleap.com",
    "GENYCASTER_SNAPCHAIN_ORIGIN": "https://snapchain.genyleap.com",
    "GENYCASTER_HYPERSNAP_ORIGIN": "https://haatz.quilibrium.com",
    "GENYCASTER_COMPANY_ORIGIN": "https://genyleap.com",
}


class DeploymentConfigurationTests(unittest.TestCase):
    def test_shipping_application_and_sign_in_authority_are_dedicated_domain(self):
        root = SCRIPT.parent.parent
        cmake = (root / "CMakeLists.txt").read_text()
        browser = (root / "ui/wasm/deployment_config.mjs").read_text()
        server = (root / "services/deployment_config.mjs").read_text()
        self.assertIn('set(GENYCASTER_PUBLIC_ORIGIN "https://genycaster.xyz"', cmake)
        self.assertIn('set(GENYCASTER_SIWF_ORIGIN "https://genycaster.xyz"', cmake)
        self.assertIn('if(GENYCASTER_PUBLIC_ORIGIN STREQUAL "https://caster.genyleap.com")', cmake)
        self.assertIn('if(FARCASTER_DEFAULT_SIWF_DOMAIN STREQUAL "genycaster.genyleap.com")', cmake)
        self.assertIn("publicOrigin: 'https://genycaster.xyz'", browser)
        self.assertIn("siwfOrigin: 'https://genycaster.xyz'", browser)
        self.assertIn("env.GENYCASTER_PUBLIC_ORIGIN || 'https://genycaster.xyz'", server)

    def test_generate_public_files_and_type_safe_cpp(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            values = DEFAULT | {
                "GENYCASTER_PUBLIC_ORIGIN": "https://caster.example.org",
                "GENYCASTER_SIWF_ORIGIN": "https://signin.example.org",
                "GENYCASTER_OPENPROOF_ISSUER": "https://login.example.org",
                "GENYCASTER_OPENPROOF_BROWSER_BASE": "https://example.org/openproof",
                "GENYCASTER_BROWSER_GATEWAY_ORIGIN": "https://cdn.example.org",
                "GENYCASTER_MEDIA_PUBLIC_ORIGIN": "https://media.example.org",
                "GENYCASTER_SNAPCHAIN_ORIGIN": "https://snap.example.org",
                "GENYCASTER_HYPERSNAP_ORIGIN": "https://hyper.example.org",
                "GENYCASTER_COMPANY_ORIGIN": "https://example.org",
            }
            generated = module.generate(values, root)
            self.assertEqual(generated["openProofIssuer"], "https://login.example.org")
            header = (root / "genycaster_deployment.hpp").read_text()
            self.assertIn('openProofIssuer{"https://login.example.org"}', header)
            self.assertNotIn("genyleap.com", header)
            browser = (root / "deployment_config.mjs").read_text()
            self.assertIn('"browserGateway":"https://cdn.example.org"', browser)
            self.assertNotIn("genyleap.com", browser)
            public_env = (root / "deployment.env").read_text().splitlines()
            self.assertIn("GENYCASTER_PUBLIC_ORIGIN=https://caster.example.org", public_env)
            self.assertIn("GENYCASTER_DEPLOYMENT_STRICT=1", public_env)
            self.assertFalse(any("SECRET" in line or "TOKEN" in line for line in public_env))
            data = json.loads((root / "public_endpoints.json").read_text())
            self.assertEqual(data["genycasterCloudApiUrl"],
                             "https://caster.example.org/api/v1/cloud/profile_extras")
            self.assertEqual(data["openproofConnectionsGatewayUrl"],
                             "https://cdn.example.org/v1/browser/openproof/connections")

    def test_fail_closed_on_untrusted_deployment_value(self):
        malicious = (
            'http://unsafe.example.org', 'javascript:alert(1)', 'file:///etc/passwd',
            'https://user:pass@host.example.org', 'https://host.example.org:8443',
            'https://host.example.org/?redirect=https://evil.example.net',
            'https://host.example.org/#fragment', 'https://host.example.org/private',
            'https://host.example.org\n#include "injected.hpp"',
            'https://host.example.org";message(FATAL_ERROR "owned")',
            'https://host.example.org/%0d%0aHeader:yes',
            'https://host.example.org.evil.org\\invalid',
            'https://hos%74.example.org', 'https://127.0.0.1',
        )
        for value in malicious:
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    module.normalize(value, "publicOrigin")

    def test_shipping_links_generated_browser_module_without_offline_caching(self):
        root = SCRIPT.parent.parent
        ui_cmake = (root / "ui/CMakeLists.txt").read_text()
        package = (root / "ci/package_wasm_assets.py").read_text()
        service_worker = (root / "ui/wasm/genycaster-sw.js").read_text()
        self.assertGreaterEqual(ui_cmake.count("generated/deployment_config.mjs"), 2)
        self.assertIn('"deployment_config.mjs"', package)
        public_set = service_worker.split("const PUBLIC_FILES=new Set([", 1)[1].split("]);", 1)[0]
        self.assertNotIn('"deployment_config.mjs"', public_set)

    def test_unrelated_or_missing_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                module.generate({**DEFAULT, "EXTRA": "https://extra.example.org"},
                                Path(directory))
            with self.assertRaises(ValueError):
                module.generate({k: v for k, v in DEFAULT.items()
                                 if k != "GENYCASTER_OPENPROOF_ISSUER"}, Path(directory))
            with self.assertRaises(ValueError):
                module.generate(DEFAULT | {
                    "GENYCASTER_OPENPROOF_ISSUER": DEFAULT["GENYCASTER_PUBLIC_ORIGIN"]
                }, Path(directory))


if __name__ == "__main__":
    unittest.main()
