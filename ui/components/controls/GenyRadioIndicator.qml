import QtQuick
import Farcaster.Client

Item {
    id: root

    property bool checked: false

    implicitWidth: 22
    implicitHeight: 22

    Superellipse {
        anchors.fill: parent
        exponent: 2.01
        curveDepth: width / 2
        fillColor: root.checked ? Theme.primarySurface : "transparent"
        borderWidth: root.checked ? Theme.strokeEmphasis : Theme.strokeHairline
        borderColor: root.checked ? Theme.accent : Theme.borderStrong

        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
        Behavior on borderColor { ColorAnimation { duration: Theme.motionFast } }

        Superellipse {
            anchors.centerIn: parent
            width: root.checked ? 9 : 5
            height: width
            exponent: 2.01
            curveDepth: width / 2
            fillColor: Theme.accent
            opacity: root.checked ? 1 : 0

            Behavior on width { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        }
    }
}
