import QtQuick
import QtQuick.Controls
import Farcaster.Client

/**
 * Genycaster slider with a product-owned track, thumb and interaction states.
 * The control keeps Qt's keyboard/pointer semantics while replacing the native
 * style completely so settings surfaces remain visually consistent.
 */
Slider {
    id: control

    property bool compact: false
    property color activeColor: Theme.primaryFill

    implicitWidth: 220
    implicitHeight: compact ? Theme.compactControlHeight : Theme.controlHeight
    leftPadding: Theme.space1
    rightPadding: Theme.space1
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true

    readonly property real trackHeight: compact ? 5 : 6
    readonly property real thumbSize: compact ? 20 : 24
    readonly property real thumbPosition: Theme.rtl ? 1 - control.position : control.position

    background: Item {
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        width: control.availableWidth
        height: Math.max(control.trackHeight, control.thumbSize)

        Superellipse {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: control.trackHeight
            fillColor: control.enabled ? Theme.surfacePressed : Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderMuted
            curveDepth: height / 2
        }

        Superellipse {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: Theme.rtl ? undefined : parent.left
            anchors.right: Theme.rtl ? parent.right : undefined
            width: control.position <= 0
                   ? 0
                   : Math.max(control.trackHeight,
                              Math.min(parent.width, parent.width * control.position))
            height: control.trackHeight
            fillColor: control.enabled ? control.activeColor : Theme.inkDisabled
            curveDepth: height / 2
            Behavior on width {
                NumberAnimation {
                    duration: control.pressed || Theme.reduceMotion ? 0 : Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    handle: Item {
        x: control.leftPadding
           + control.thumbPosition * Math.max(0, control.availableWidth - width)
        y: Math.round((control.height - height) / 2)
        width: control.thumbSize
        height: control.thumbSize
        scale: control.pressed ? 0.94 : control.hovered ? 1.04 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: Theme.reduceMotion ? 0 : Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }

        Superellipse {
            anchors.fill: parent
            fillColor: control.enabled ? Theme.surfaceElevated : Theme.surfaceMuted
            borderWidth: Theme.keyboardFocusVisible(control) ? 2 : Theme.strokeStandard
            borderColor: Theme.keyboardFocusVisible(control) ? Theme.focus : control.activeColor
            curveDepth: width / 2
            elevation: control.enabled ? Theme.elevationControl : Theme.elevationNone
            shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
            shadowOpacityScale: control.pressed ? 0.5 : 0.9

            Superellipse {
                anchors.centerIn: parent
                width: control.compact ? 7 : 8
                height: width
                fillColor: control.enabled ? control.activeColor : Theme.inkDisabled
                curveDepth: width / 2
            }
        }
    }
}
