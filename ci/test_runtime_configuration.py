"""Runtime startup configuration mapping and protocol isolation regressions.

Tests keep a named C++ runtime policy snapshot aligned with its existing
provider/signing factory. They do not require a production signer vault.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Treat field order as part of the documented cross-platform startup contract.
# Read, Write, signer attestation and approval cannot be silently interchanged.
EXPECTED_FIELDS = (
    "databasePath",
    "networkProxyPolicy",
    "providerBaseUrl",
    "farcasterReadBaseUrl",
    "farcasterReadApiKey",
    "additionalReadProviders",
    "readProviderApiKeys",
    "preferredReadProviderId",
    "allowCompatibleReadFallback",
    "additionalWriteProviders",
    "preferredWriteProviderId",
    "authRelayUrl",
    "optimismRpcUrl",
    "snapchainBaseUrl",
    "snapchainWriteBaseUrl",
    "mediaGatewayBaseUrl",
    "snapchainAuthorizationHeader",
    "snapchainAuthorizationValue",
    "snapchainWriteAuthorizationHeader",
    "snapchainWriteAuthorizationValue",
    "appFid",
    "signerAttestationUrl",
    "approvalLauncher",
)


def _fields(source: str) -> tuple[str, ...]:
    marker = "struct RuntimeConfiguration final {"
    assert source.count(marker) == 1, "missing or duplicate runtime policy declaration"
    body = source.split(marker, 1)[1].split("\n};", 1)[0]
    return tuple(re.findall(r"\b([A-Za-z]\w*)\s*\{(?:false)?\}\s*;", body))


class RuntimeStartupPolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.interface = (ROOT / "ui/app/runtime_composition.cppm").read_text(
            encoding="utf-8"
        )
        cls.implementation = (ROOT / "ui/app/runtime_composition.cpp").read_text(
            encoding="utf-8"
        )
        cls.entry = (ROOT / "ui/app/main.cpp").read_text(encoding="utf-8")

    def test_named_policy_fields_match_existing_factory_contract(self) -> None:
        self.assertEqual(_fields(self.interface), EXPECTED_FIELDS)
        self.assertIn(
            "makeRuntime(\n    RuntimeConfiguration configuration);",
            self.interface,
        )

    def test_named_policy_wrapper_preserves_argument_order(self) -> None:
        marker = "makeRuntime(\n    RuntimeConfiguration configuration)\n{"
        self.assertEqual(self.implementation.count(marker), 1)
        body = self.implementation.split(marker, 1)[1].split("\n}\n", 1)[0]
        forwarded = tuple(re.findall(r"configuration\.([A-Za-z]\w*)", body))
        self.assertEqual(forwarded, EXPECTED_FIELDS)
        self.assertIn("std::move(configuration.approvalLauncher)", body)
        self.assertIn("return makeRuntime(", body)

    def test_application_owns_one_named_snapshot(self) -> None:
        marker = "auto startupOptions = genycaster::ui::runtime::RuntimeConfiguration{"
        self.assertEqual(self.entry.count(marker), 1)
        body = self.entry.split(marker, 1)[1].split("\n        };", 1)[0]
        named = tuple(re.findall(r"\.([A-Za-z]\w*)\s*=", body))
        self.assertEqual(named, EXPECTED_FIELDS)
        self.assertIn(".approvalLauncher = makeWalletApprovalLauncher(", body)
        self.assertIn("makeRuntime(std::move(startupOptions))", self.entry)
        self.assertIn("startupOptions = std::move(startupOptions)", self.entry)

    def test_read_write_and_attestor_are_independent(self) -> None:
        block = self.interface.split("struct RuntimeConfiguration final {", 1)[1]
        block = block.split("\n};", 1)[0]
        for label in (
            "preferredReadProviderId",
            "preferredWriteProviderId",
            "signerAttestationUrl",
            "snapchainWriteBaseUrl",
            "readProviderApiKeys",
        ):
            self.assertIn(label, block)
        self.assertNotIn("QSettings", block)
        self.assertNotIn("setContextProperty", block)
        self.assertIn(
            "authenticationAvailable, openExploreForTest]() mutable",
            self.entry,
        )

    def test_legacy_browser_wheel_header_is_not_imported(self) -> None:
        bindings = (ROOT / "ui/app/runtime_ui_bindings.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("browser_wheel_fallback.hpp", bindings)


if __name__ == "__main__":
    unittest.main()
