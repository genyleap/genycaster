import QtQuick

// A real image request never looks like a finished generic Mini App icon.
// Preserve final tile geometry during fetch; reveal fallback only for a
// missing or failed source. The overlay is non-interactive.
Item {
    id: root
    property bool imageReady: false
    property bool loadFailed: false
    property bool hasSource: false
    opacity: root.imageReady ? 0 : 1
    visible: opacity > 0
    Behavior on opacity {
        enabled: !Theme.reduceMotion
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Superellipse {
        anchors.fill: parent
        fillColor: Theme.surfaceMuted
        curveDepth: Theme.curveControl
        opacity: root.hasSource && !root.loadFailed ? 0.9 : 0.45

        SequentialAnimation on opacity {
            running: root.visible && root.hasSource && !root.loadFailed && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation { to: 0.48; duration: 800; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0.90; duration: 800; easing.type: Easing.InOutSine }
        }
    }

    Icon {
        anchors.centerIn: parent
        visible: !root.hasSource || root.loadFailed
        text: Theme.iconApps
        color: Theme.inkSubtle
        preferredSize: Math.min(28, root.width * 0.42)
    }
}
