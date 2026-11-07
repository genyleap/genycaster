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
        cls.interface = (ROOT / "ui/app/runtime/composition.cppm").read_text(
            encoding="utf-8"
        )
        cls.implementation = (ROOT / "ui/app/runtime/composition.cpp").read_text(
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

    def test_provider_routes_resolved_before_ownership_transfer(self) -> None:
        """Regression: moved-from std::optional<string> remains engaged but empty.

        Routing from that value previously produced:
        'Selected protocol has no compatible canonical message writer.
         Read provider ID:  (not registered)'
        and exited with code 2 before QML started on macOS.
        """
        main = self.entry
        parser = main.index("auto configuredProfiles = ")
        routes = main.index("auto routingPolicy = ")
        route_call = main.index("bootstrap::loadProviderRoutes(", routes)
        route_error_gate = main.index("if (!routingPolicy) return 2;", route_call)
        transfers = (
            "std::move(configuredProfiles->readProviderApiKeys)",
            "std::move(profile.id)",
            "std::move(profile.adapterContract)",
        )
        self.assertLess(parser, routes)
        self.assertLess(route_call, route_error_gate)
        for transfer in transfers:
            self.assertIn(transfer, main)
            self.assertLess(
                route_error_gate,
                main.index(transfer),
                msg=f"Route resolution must precede ownership transfer: {transfer}",
            )
        # Small selected IDs are copied; the original config stays valid even
        # when future startup stages also need the route/user selection.
        for selected in ("preferredReadProviderId", "preferredWriteProviderId"):
            self.assertNotIn(f"std::move(configuredProfiles->{selected})", main)
            self.assertIn(
                f"const auto {selected} = configuredProfiles->{selected};", main
            )
        self.assertIn("*configuredProfiles", main[route_call:route_error_gate])

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

    def test_gui_attachment_is_published_after_all_bindings(self) -> None:
        """All controller connections must be established before ready=true."""
        bridge = (ROOT / "ui/app/runtime/bindings.cpp").read_text(
            encoding="utf-8"
        )
        header = (ROOT / "ui/app/runtime/bindings.hpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("struct RuntimeAttachment final", header)
        self.assertIn("void attachRuntime(RuntimeAttachment attachment,", header)
        self.assertEqual(
            self.entry.count("genycaster::ui::bindings::attachRuntime({"), 1
        )
        self.assertNotIn("connectRuntime(", self.entry)
        self.assertNotIn("connectMediaPlayback(", self.entry)
        self.assertNotIn("connectMediaUpload(", self.entry)
        self.assertNotIn("connectBrowserMediaUpload(", self.entry)

        start = bridge.index("void attachRuntime(RuntimeAttachment attachment,")
        body = bridge[start:].split(
            "} // namespace genycaster::ui::bindings", 1
        )[0]
        source_order = (
            "attachment.runtime = std::move(readyRuntime);",
            "    connectRuntime(",
            "    connectMediaPlayback(",
            "    connectBrowserMediaUpload(",
            "    attachment.presentationBridge.applyRuntimeState(true);",
        )
        # The browser upload branch is independent from native media upload.
        offsets = [body.index(item) for item in source_order]
        self.assertEqual(offsets, sorted(offsets))
        self.assertIn("    connectMediaUpload(", body)
        self.assertLess(
            body.index("    connectMediaUpload("),
            body.index("applyRuntimeState(true)"),
        )
        self.assertGreater(
            body.index("openIdentity(attachment.startupIdentity)"),
            body.index("applyRuntimeState(true)"),
        )
        self.assertIn(
            "QThread::currentThread() == attachment.presentationBridge.thread()",
            body,
        )

    def test_legacy_browser_wheel_header_is_not_imported(self) -> None:
        bindings = (ROOT / "ui/app/runtime/bindings.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("browser_wheel_fallback.hpp", bindings)


if __name__ == "__main__":
    unittest.main()
