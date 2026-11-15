import QtQuick
import Farcaster.Client

// Faithful Genyleap Neon UI Kit state plate: selected, hover, and pressed
// are independent opacity layers. Neon is NOT painted here: tabs and menu
// rows should have crisp material transitions, not permanent luminous borders.
Item {
    id: plate
    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property bool outlined: false
    property bool animatePaint: true
    property color selectedFill: Theme.primarySurface
    property color hoverFill: Theme.surfaceHover
    property color pressedFill: Theme.surfacePressed
    property color outlineColor: Theme.borderMuted
    property real cornerRadius: Theme.curveControl

    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.selectedFill
        borderWidth: 0
        opacity: plate.selected ? 1 : 0
        Behavior on opacity {
            enabled: plate.animatePaint && !Theme.reduceMotion
            NumberAnimation {
                duration: Theme.materialSelectionDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.hoverFill
        borderWidth: 0
        opacity: plate.hovered && !plate.pressed ? (plate.selected ? 0.23 : 1) : 0
        Behavior on opacity {
            enabled: plate.animatePaint && !Theme.reduceMotion
            NumberAnimation {
                duration: plate.hovered ? Theme.materialHoverInDuration
                                        : Theme.materialHoverOutDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    Superellipse {
        anchors.fill: parent
        curveDepth: plate.cornerRadius
        fillColor: plate.pressedFill
        borderWidth: 0
        opacity: plate.pressed ? 1 : 0
    }
    // Keyboard focus remains visible even with the Neon preference disabled.
    Superellipse {
        anchors.fill: parent
        fillColor: "transparent"
        borderWidth: plate.focused ? 2 : plate.outlined ? 1 : 0
        borderColor: plate.focused ? Theme.focus : plate.outlineColor
        curveDepth: plate.cornerRadius
    }
}
