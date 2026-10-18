import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: rootWindow
    objectName: "rootWindow"

    width: 430
    height: 860
    // Browsers can expose a visual viewport shorter than the native app's
    // 640px minimum (especially with navigation bars or a software keyboard).
    // Never let the Qt canvas grow below the visible viewport on WebAssembly.
    minimumWidth: Qt.platform.os === "wasm" ? 0 : 360
    minimumHeight: Qt.platform.os === "wasm" ? 0 : 640
    visible: true
    color: Theme.background
    title: qsTrId("genycaster.app.genycaster")
    font.family: Theme.fontFamily
    font.pixelSize: Theme.typeBodySize

    // Geometry is directed explicitly by the reusable UI components.
    // Do not globally mirror the tree: several content surfaces intentionally
    // keep their own bidi direction, and global mirroring would double-flip
    // RowLayout.layoutDirection and explicit text alignment.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    palette.window: Theme.background
    palette.windowText: Theme.ink
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceMuted
    palette.text: Theme.ink
    palette.button: Theme.surface
    palette.buttonText: Theme.ink
    palette.highlight: Theme.primaryFill
    palette.highlightedText: Theme.accentInk
    palette.placeholderText: Theme.inkMuted

    readonly property bool isReady: appShell.isReady

    AppShell {
        id: appShell
        anchors.fill: parent
    }
}
