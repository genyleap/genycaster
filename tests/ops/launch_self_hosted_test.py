#!/usr/bin/env python3
"""Contract tests for securely composing independent native read/write routes."""
import importlib.util
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("selfhost_launch",
    ROOT / "tools" / "launch_self_hosted.py")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class LauncherTests(unittest.TestCase):
    def test_exact_raw_reader_and_independent_writer(self):
        env = launcher.make_environment(
            "http://127.0.0.1:3381/", "https://validator.example",
            {"GENYCASTER_FARCASTER_READ_API_KEY": "secret",
             "GENYCASTER_READ_PROVIDER_API_KEYS_JSON": '{"managed":"secret"}',
             "GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE": "old-writer-secret",
             "GENYCASTER_SNAPCHAIN_AUTH_VALUE": "old-reader-secret",
             "PATH": "/usr/bin"})
        reader = json.loads(env["GENYCASTER_READ_PROVIDER_PROFILES_JSON"])[0]
        writer = json.loads(env["GENYCASTER_WRITE_PROVIDER_PROFILES_JSON"])[0]
        self.assertEqual(reader["contract"], "snapchain-v1")
        self.assertEqual(writer["contract"], "snapchain-v1")
        self.assertEqual(reader["endpoint"], "http://127.0.0.1:3381")
        self.assertEqual(writer["endpoint"], "https://validator.example")
        self.assertEqual(writer["readEndpoint"], reader["endpoint"])
        self.assertEqual(reader["id"], env["GENYCASTER_PREFERRED_READ_PROVIDER_ID"])
        self.assertEqual(writer["id"], env["GENYCASTER_PREFERRED_WRITE_PROVIDER_ID"])
        self.assertNotEqual(reader["id"], writer["id"])
        self.assertEqual(env["GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK"], "0")
        self.assertEqual(env["GENYCASTER_SNAPCHAIN_BASE_URL"], reader["endpoint"])
        self.assertEqual(env["GENYCASTER_SNAPCHAIN_WRITE_BASE_URL"], writer["endpoint"])
        self.assertNotIn("GENYCASTER_FARCASTER_READ_API_KEY", env)
        self.assertNotIn("GENYCASTER_READ_PROVIDER_API_KEYS_JSON", env)
        self.assertNotIn("GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE", env)
        self.assertNotIn("GENYCASTER_SNAPCHAIN_AUTH_VALUE", env)
        self.assertEqual(env["PATH"], "/usr/bin")

    def test_validator_must_be_independent_and_proven_compatible(self):
        with self.assertRaisesRegex(ValueError, "independent verified validator"):
            launcher.make_environment("https://node.example", "https://node.example", {})
        with self.assertRaises(ValueError):
            launcher.make_environment("https://alice:secret@node.example",
                                      "https://validator.example", {})
        with self.assertRaises(ValueError):
            launcher.make_environment("http://node.example:3381",
                                      "https://validator.example", {})

    def test_deployment_selfhosted_flag_is_explicit(self):
        default = launcher.make_environment(
            "https://read.example", "https://validator.example", {})
        self.assertFalse(json.loads(
            default["GENYCASTER_WRITE_PROVIDER_PROFILES_JSON"])[0]["selfHosted"])
        asserted = launcher.make_environment(
            "https://read.example", "https://validator.example", {},
            writer_self_hosted=True)
        self.assertTrue(json.loads(
            asserted["GENYCASTER_WRITE_PROVIDER_PROFILES_JSON"])[0]["selfHosted"])


if __name__ == "__main__":
    unittest.main()
