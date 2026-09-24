import QtQuick
import QtQuick.Controls
import Farcaster.Client

/**
 * Genycaster progress indicator with the same superellipse geometry and accent
 * semantics used by the rest of the product control family.
 */
ProgressBar {
    id: control

    property color progressColor: Theme.primaryFill
    property color trackColor: Theme.surfacePressed
    property bool compact: false

    implicitWidth: 220
    implicitHeight: compact ? 6 : 8
    padding: 0

    background: Superellipse {
        implicitWidth: 220
        implicitHeight: control.compact ? 6 : 8
        fillColor: control.enabled ? control.trackColor : Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: height / 2
    }

    contentItem: Item {
        implicitWidth: 220
        implicitHeight: control.compact ? 6 : 8

        Superellipse {
            width: control.position <= 0
                   ? 0
                   : Math.max(parent.height, parent.width * control.position)
            height: parent.height
            anchors.left: Theme.rtl ? undefined : parent.left
            anchors.right: Theme.rtl ? parent.right : undefined
            fillColor: control.enabled ? control.progressColor : Theme.inkDisabled
            curveDepth: height / 2

            Behavior on width {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
