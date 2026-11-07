import QtQuick
import QtQuick.Controls
import Farcaster.Client

ScrollBar {
    id: control

    property bool alwaysVisible: false

    policy: ScrollBar.AsNeeded
    minimumSize: 0.08
    padding: 2
    implicitWidth: orientation === Qt.Vertical ? 8 : 40
    implicitHeight: orientation === Qt.Horizontal ? 8 : 40

    contentItem: Item {
        implicitWidth: control.orientation === Qt.Vertical ? 4 : 28
        implicitHeight: control.orientation === Qt.Horizontal ? 4 : 28

        Superellipse {
            anchors.fill: parent
            fillColor: control.pressed ? Theme.inkMuted : Theme.inkSubtle
            opacity: control.alwaysVisible || control.active || control.hovered || control.pressed ? 0.58 : 0.0
            curveDepth: Math.min(width, height) / 2
            exponent: 2.01

            Behavior on opacity {
                NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic }
            }
        }
    }

    background: Item {}
}
