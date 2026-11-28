#!/usr/bin/env python3
"""Offscreen Qt 6.12 smoke test for ProfileSurface's real identity resolver.

No application build, network request, or CI runner is required. Uses Qt's
own QML engine to exercise the extracted functions under rapid navigation.
"""
from pathlib import Path
import os
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT / "ui/components/profile/ProfileSurface.qml"
QML = SOURCE.read_text()

def extract(before: str, after: str) -> str:
    return QML.split(before, 1)[1].split(after, 1)[0]

resolver = "    function resolveActiveDetail() {" + extract(
    "    function resolveActiveDetail() {", "    function synchronizeActiveDetail() {")
synchronizer = "    function synchronizeActiveDetail() {" + extract(
    "    function synchronizeActiveDetail() {", "    // Coalesce duplicate session")
fid_resolver = "    function resolveProfileFidText() {" + extract(
    "    function resolveProfileFidText() {", "    // The selected read provider")

fixture = """import QtQuick
Item {
    id: root
    property var detail: ({fid: 42, fidString: "42", displayName: "Original"})
    property bool ownProfileNavigation: false
    property string accountFidText: "42"
    property double accountFid: 42
    property string accountNetwork: "farcaster"
    property string activeReadNetwork: "farcaster"
    property var activeDetail: ({})
    property string activeDetailSignature: ""
    property string activeDetailNetwork: ""
    property string profileFidText: ""
    readonly property string profileNetwork: ownProfileNavigation ? accountNetwork : activeReadNetwork
    property int transitions: 0
    onActiveDetailChanged: transitions += 1
    function ensure(condition, message) {
        if (condition) return
        console.error("PROFILE_QML_RUNTIME_FAILURE:", message)
        Qt.exit(2)
    }
    QtObject {
        id: presentationBridge
        property string activeProfileFidText: "42"
        property var accountProfile: ({
            fid: 42, fidString: "42", displayName: "Authenticated",
            followerCount: 33, socialCountsKnown: true
        })
        property bool ownProfileReadReady: false
        property string accountName: "Signed-in owner"
        property string accountUsername: "owner"
        property string accountAvatarUrl: ""
        signal stateChanged()
        signal authenticationStateChanged()
    }
""" + resolver + synchronizer + fid_resolver + """
    Connections {
        target: presentationBridge
        function onStateChanged() { Qt.callLater(root.synchronizeActiveDetail) }
        function onAuthenticationStateChanged() { Qt.callLater(root.synchronizeActiveDetail) }
    }
    onDetailChanged: root.synchronizeActiveDetail()
    onOwnProfileNavigationChanged: Qt.callLater(root.synchronizeActiveDetail)
    onAccountFidTextChanged: Qt.callLater(root.synchronizeActiveDetail)
    onActiveReadNetworkChanged: Qt.callLater(root.synchronizeActiveDetail)
    Component.onCompleted: root.synchronizeActiveDetail()
    Timer {
        interval: 35
        running: true
        repeat: true
        property int stage: 0
        onTriggered: {
            if (stage === 0) {
                root.ensure(root.activeDetail.displayName === "Original", "initial")
                presentationBridge.activeProfileFidText = "73"
                presentationBridge.stateChanged()
            } else if (stage === 1) {
                root.ensure(!root.activeDetail.fid, "stale original after Back")
                root.ensure(root.profileFidText === "73", "request identity updated independently of detail")
                root.detail = {fid: 73, fidString: "73", displayName: "New person"}
            } else if (stage === 2) {
                root.ensure(root.activeDetail.displayName === "New person", "new identity")
                root.ownProfileNavigation = true
                root.activeReadNetwork = "hypersnap"
                presentationBridge.ownProfileReadReady = false
                root.synchronizeActiveDetail()
            } else if (stage === 3) {
                root.ensure(root.activeDetail.displayName === "Signed-in owner",
                            "foreign-profile protection")
                presentationBridge.ownProfileReadReady = true
                presentationBridge.stateChanged()
            } else if (stage === 4) {
                root.ensure(root.activeDetail.displayName === "Authenticated",
                            "verified owner restored")
                root.ensure(root.activeDetail.followerCount === 33,
                            "authentic profile counters")
                root.ensure(root.profileFidText === "42", "own request identity")
                root.ensure(root.transitions <= 10,
                            "unexpected binding feedback storm")
                console.log("PROFILE_QML_RUNTIME_PASS", root.transitions)
                Qt.exit(0)
            }
            stage += 1
        }
    }
}
"""

def main() -> None:
    runner = os.environ.get("QML_RUNNER", "/opt/Qt/6.12.0/gcc_64/bin/qml")
    if not Path(runner).is_file():
        raise SystemExit(f"Qt runtime not installed: {runner}")
    environment = dict(os.environ)
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["QT_QUICK_BACKEND"] = "software"
    with tempfile.TemporaryDirectory(prefix="genycaster-profile-qml-") as tmp:
        path = Path(tmp) / "ProfileIdentitySmoke.qml"
        path.write_text(fixture)
        result = subprocess.run(
            [runner, "-f", str(path)],
            capture_output=True,
            text=True,
            timeout=12,
            env=environment,
            cwd=PROJECT,
            check=False,
        )
    output = result.stdout + result.stderr
    print(output[-4500:])
    if result.returncode != 0 or "PROFILE_QML_RUNTIME_PASS" not in output:
        raise SystemExit(f"Qt profile runtime smoke failed: {result.returncode}")
    if "Binding loop detected" in output:
        raise SystemExit("Qt profile runtime reported a QML binding cycle")

if __name__ == "__main__":
    main()
