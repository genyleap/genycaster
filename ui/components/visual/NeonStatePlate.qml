import QtQuick
import Farcaster.Client

// Selection, hover and pressed layers follow the Neon UI Kit v0.4.5:
// stable geometry; independent opacity transitions; focus drawn on top.
Item {
    id: plate
    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property bool outlined: false
    property bool animatePaint: true
    property bool neonOnSelection: true
    property color selectedFill: Theme.primarySurface
    property color hoverFill: Theme.surfaceHover
    property color pressedFill: Theme.surfacePressed
    property color outlineColor: Theme.borderMuted
    property real cornerRadius: Theme.curveControl
    property real pointerX: width / 2
    property real pointerY: height / 2

    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.selectedFill
        opacity: plate.selected ? 1 : 0
        Behavior on opacity {
            enabled: plate.animatePaint && !Theme.reduceMotion
            NumberAnimation {
                duration: Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }
    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.hoverFill
        opacity: plate.hovered && !plate.pressed ? (plate.selected ? 0.23 : 1) : 0
        Behavior on opacity {
            enabled: plate.animatePaint && !Theme.reduceMotion
            NumberAnimation {
                duration: Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }
    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.pressedFill
        opacity: plate.pressed ? 1 : 0
    }
    // Keyboard focus remains legible even when decorative Neon is disabled.
    NeonSurface {
        anchors.fill: parent
        fillColor: "transparent"
        borderWidth: plate.focused ? 2 : plate.outlined ? 1 : 0
        borderColor: plate.focused ? Theme.focus : plate.outlineColor
        curveDepth: plate.cornerRadius
        ambientNeon: plate.neonOnSelection && plate.selected
        interactive: plate.hovered
        externalHovered: plate.hovered
        externalPointerX: plate.pointerX
        externalPointerY: plate.pointerY
        neonStrength: Theme.neonControlStrength
        neonColor: plate.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.34) : "transparent"
        animatePaint: false
    }
}
