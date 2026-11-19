#!/usr/bin/env python3
"""Regression contracts for independent Genyleap/Farcaster browser login."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BrowserAuthFailureContractTests(unittest.TestCase):
    def test_only_explicit_rejection_maps_to_unauthorized(self):
        header = (ROOT / "ui/app/identity/browser_auth.hpp").read_text()
        source = (ROOT / "ui/app/identity/browser_auth.cpp").read_text()
        bridge = (ROOT / "ui/app/runtime/bindings.cpp").read_text()
        self.assertIn("enum class FailureKind", header)
        for kind in ("Rejected", "Transport", "InvalidResponse", "MissingLink",
                     "RateLimited", "Expired", "Conflict"):
            self.assertIn(kind, header)
            self.assertIn("Kind::" + kind, bridge)
        self.assertIn("status == 401 || status == 403", source)
        self.assertIn("status == 429", source)
        self.assertIn("status == 0 || status >= 500", source)
        self.assertIn("FailureKind::Transport", source)
        self.assertIn("FailureKind::MissingLink", source)
        self.assertIn("FailureKind::Conflict", source)
        self.assertIn("case Kind::Rejected: code = Code::Unauthorized", bridge)
        self.assertIn("case Kind::Transport: code = Code::Unavailable", bridge)
        self.assertIn("case Kind::MissingLink: code = Code::NotFound", bridge)
        self.assertNotIn(
            "ErrorCode::Unauthorized, event.error.toStdString()", bridge)

    def test_browser_farcaster_failure_can_retry_with_verified_genyleap(self):
        qml = (ROOT / "ui/components/setup/InitialSetup.qml").read_text()
        start = qml.index('objectName: "setupRetryWithGenyleapAfterFarcaster"')
        fragment = qml[start:start + 820]
        self.assertIn('root.authenticationRoute === "farcaster"', fragment)
        self.assertIn('root.authenticationPhase === "failed"', fragment)
        self.assertIn('!root.authenticated', fragment)
        self.assertIn('root.continueWithGenyleap()', fragment)
        self.assertIn('genycaster.shell.continue_with_genyleap', fragment)
        self.assertIn('onClicked: root.switchStep(4)', qml)
        # No false success, forced linking, or account/session bypass.
        self.assertNotIn('root.authenticated = true', fragment)
        self.assertNotIn('accountFid =', fragment)


if __name__ == "__main__":
    unittest.main()
