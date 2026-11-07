import QtQuick

// Busy state shared by Feed, Profile, Detail and Apps. An existing result stays
// readable while refresh runs; only this non-interactive line moves.
Item {
    id: root
    property bool active: false
    visible: active
    height: active ? 3 : 0
    clip: true
    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTrId("genycaster.feed_surface.loading_more")
    Accessible.description: active ? Accessible.name : ""

    Superellipse {
        id: sweep
        x: root.active && Theme.reduceMotion ? 0 : -width
        width: Theme.reduceMotion ? root.width : Math.max(64, root.width * 0.32)
        height: parent.height
        fillColor: Theme.accent
        curveDepth: 2

        SequentialAnimation on x {
            running: root.visible && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation {
                from: -sweep.width
                to: root.width
                duration: 1100
                easing.type: Easing.InOutSine
            }
        }
    }
}
