#!/usr/bin/env python3
"""Qt Quick tests for the shell's production utility Loader and filter menu.

Extract the shell's exact utility block, and use real Qt Controls with
lightweight stand-ins for the optional application-specific dialog content.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SHELL = ROOT / "ui/components/shell/AppShell.qml"
FILTER = ROOT / "ui/components/controls/FilterDropdown.qml"
THEME = """pragma Singleton
import QtQuick
QtObject {
    property int space1: 4
    property int space2: 8
    property int space3: 12
    property int typeCaptionSize: 12
    property int typeMetaSize: 13
    property int weightMedium: Font.Medium
    property int weightDemiBold: Font.DemiBold
    property real strokeHairline: 1
    property int motionFast: 0
    property bool rtl: false
    property bool reduceMotion: true
    property int uiDirection: Qt.LeftToRight
    property color textPrimary: "black"
    property color textTertiary: "gray"
    property color accent: "blue"
    property color surfaceElevated: "white"
    property color surfaceHover: "white"
    property color surfaceRaised: "white"
    property color borderDefault: "gray"
    property color primarySurface: "white"
    property color primarySurfaceHover: "white"
    property color surfacePressed: "gray"
    property string iconChevronDown: "v"
    property string iconCheck: "x"
    property int elevationNone: 0
    property int elevationPopover: 0
    property real curveCompact: 4
    function keyboardFocusVisible(control) { return false }
}
"""
POPUP = """import QtQuick
import QtQuick.Controls
Item {
    id: control
    property string reportRoute: ""
    property string draft: ""
    property int opens: 0
    readonly property bool opened: popup.opened
    width: parent ? parent.width - 40 : 0
    height: parent ? parent.height - 60 : 0
    function open() { ++opens; popup.open() }
    function close() { popup.close() }
    Popup {
        id: popup
        parent: control.parent
        modal: true
        width: control.width
        height: control.height
        contentItem: Item {}
    }
}
"""
TEST = """import QtQuick
import QtQuick.Controls
import QtTest
Item {
    id: host
    width: 800; height: 600
    readonly property var window: host.Window.window
    Item {
        id: root
        anchors.fill: parent
        property string shellRoute: ""
        property string primaryDestination: "home"
        readonly property bool overlayOpen: taskManagerPopup.opened ||
            renderingSettingsPopup.opened || feedbackDialog.opened
        // UTILITY_BLOCK
        FilterDropdown {
            id: filter
            width: 120
            model: ["Recent", "Popular", "Following"]
        }
        TestCase {
            name: "LazyShellUtilityLifecycle"
            when: windowShown
            function test_utilityLifecycle() {
                const loaders = [taskManagerPopup, renderingSettingsPopup, feedbackDialog]
                for (const loader of loaders) {
                    compare(loader.active,false)
                    compare(loader.item,null)
                    compare(loader.dialog,null)
                    compare(loader.opened,false)
                    loader.close()
                }
                compare(root.overlayOpen,false)
                for (const loader of loaders) {
                    loader.open()
                    tryCompare(loader,"opened",true)
                    verify(loader.dialog)
                    compare(loader.dialog.parent,root.Overlay.overlay)
                    tryCompare(loader.dialog,"width",window.width-40)
                    tryCompare(loader.dialog,"height",window.height-60)
                    compare(root.overlayOpen,true)
                    const dialog = loader.dialog
                    dialog.draft = "retain me"
                    loader.close()
                    tryCompare(loader,"opened",false)
                    compare(root.overlayOpen,false)
                    compare(loader.dialog,dialog)
                    compare(dialog.draft,"retain me")
                    loader.open()
                    tryCompare(loader,"opened",true)
                    compare(loader.dialog,dialog)
                    compare(dialog.opens,2)
                    window.width = 700
                    tryCompare(dialog,"width",660)
                    loader.close()
                    tryCompare(loader,"opened",false)
                    window.width = 800
                    tryCompare(dialog,"width",760)
                }
                root.shellRoute = "profile"
                compare(feedbackDialog.dialog.reportRoute,"profile")
                root.shellRoute = ""
                root.primaryDestination = "explorer"
                compare(feedbackDialog.dialog.reportRoute,"explorer")
            }
            function test_filterModelLifecycle() {
                compare(filter.popup.visible,false)
                compare(filter.popup.contentItem.model,null)
                filter.popup.open()
                tryCompare(filter.popup,"opened",true)
                tryCompare(filter.popup.contentItem,"count",3)
                verify(filter.popup.height > 0)
                tryVerify(() => filter.popup.contentItem.itemAtIndex(2) !== null)
                const option = filter.popup.contentItem.itemAtIndex(2)
                mouseClick(option, option.width/2, option.height/2)
                tryCompare(filter.popup,"visible",false)
                compare(filter.popup.contentItem.model,null)
                compare(filter.currentIndex,2)
                compare(filter.currentText,"Following")
                filter.popup.open()
                tryCompare(filter.popup.contentItem,"count",3)
                verify(filter.popup.height > 0)
                filter.popup.close()
                tryCompare(filter.popup,"visible",false)
            }
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER")
    shell = SHELL.read_text()
    start = shell.index("    component UtilitySurface: Loader {")
    stop = shell.index("\n    Component.onCompleted:", start)
    block = shell[start:stop]
    with tempfile.TemporaryDirectory(prefix="genycaster-lazy-tools-") as tmp:
        path = Path(tmp)
        module = path / "Farcaster/Client"
        module.mkdir(parents=True)
        (module / "qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\n")
        (module / "Theme.qml").write_text(THEME)
        for name in ("TaskManagerPopup", "RenderingSettingsPopup", "FeedbackDialog"):
            (path / (name+".qml")).write_text(POPUP)
        stubs = {
            "DirectionAwareLabel": "Label {}",
            "Icon": 'Label { property real preferredSize: 12; width: preferredSize; height: preferredSize }',
            "Superellipse": 'Rectangle { property color fillColor: "transparent"; property real borderWidth: 0; property color borderColor: "gray"; property real curveDepth: 0; property real elevation: 0; color: fillColor }',
            "NeonStatePlate": 'Item { property bool selected: false; property bool hovered: false; property bool pressed: false; property bool focused: false; property color selectedFill: "white"; property color hoverFill: "white"; property color pressedFill: "white"; property bool neonOnHover: false; property real cornerRadius: 0 }',
            "PopupSurface": 'Rectangle { property color fillColor: "white"; property color borderColor: "gray"; property real curveDepth: 0; property real elevation: 0; color: fillColor }',
            "GenyScrollBar": "ScrollBar {}",
        }
        for name, body in stubs.items():
            (path / (name+".qml")).write_text("import QtQuick\nimport QtQuick.Controls\n"+body)
        shutil.copyfile(FILTER,path/"FilterDropdown.qml")
        (path/"tst_lazy.qml").write_text(TEST.replace("        // UTILITY_BLOCK",block))
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                   QT_QUICK_CONTROLS_STYLE="Basic")
        result = subprocess.run([runner,"-import",tmp,"-input",tmp],env=env,
                                capture_output=True,text=True,timeout=30)
        output=result.stdout+result.stderr
        print(output)
        if result.returncode or any(x in output for x in ("Binding loop detected", "ReferenceError:", "TypeError:", "during incubation", "items in the process", "Unable to assign")):
            raise SystemExit(1)
if __name__ == "__main__":
    main()
