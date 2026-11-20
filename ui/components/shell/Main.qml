import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Farcaster.Client

ApplicationWindow {
    id: rootWindow
    objectName: "rootWindow"

    // The same QML entrypoint is used on all platforms. Desktop should
    // launch as a desktop application, not with the phone-sized 430x860
    // canvas. Size only the initial window; normal user resize breaks the
    // bindings and is never overridden by a resize handler or timer.
    readonly property bool nativeDesktop: Qt.platform.os === "windows"
                                          || Qt.platform.os === "osx"
                                          || Qt.platform.os === "linux"
    // Default to a three-column desktop workspace when the monitor allows
    // it: navigation, cast feed and widgets are visible from first launch.
    // Keep smaller monitors within their available work area.
    width: nativeDesktop
           ? Math.min(1560, Screen.desktopAvailableWidth > 0
                      ? Math.max(360, Screen.desktopAvailableWidth - 48) : 1560)
           : 430
    height: nativeDesktop
            ? Math.min(940, Screen.desktopAvailableHeight > 0
                       ? Math.max(640, Screen.desktopAvailableHeight - 48) : 940)
            : 860
    // Only resizable desktop windows have a minimum size. A gesture bar or
    // software keyboard can reduce mobile and browser viewports below 640dp.
    minimumWidth: nativeDesktop ? 360 : 0
    minimumHeight: nativeDesktop ? 640 : 0
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
