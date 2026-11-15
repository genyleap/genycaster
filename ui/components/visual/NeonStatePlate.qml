import QtQuick
import Farcaster.Client

// Kit material state plate: selected, hover, and pressed are independent
// opacity layers. The glint is transient (never a permanent selected halo).
// This is shared by Tabs, ComboBox menus and navigation to avoid one-off
// implementations that never respond to pointer proximity.
Item {
    id: plate
    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property bool outlined: false
    property bool animatePaint: true
    property bool neonOnHover: true
    property real neonStrength: Theme.neonControlStrength
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
    HoverHandler {
        id: glintPointer
        enabled: plate.neonOnHover && Theme.neonEnabled
        blocking: false
    }
    // Focus is a distinct, accessible outline. The optional hover reflection
    // is demand-loaded and fades when the pointer leaves.
    NeonSurface {
        anchors.fill: parent
        interactive: plate.neonOnHover
        deferredNeon: true
        ambientNeon: false
        focused: plate.focused
        externalHovered: plate.hovered && plate.neonOnHover
        externalPointerX: glintPointer.hovered ? glintPointer.point.position.x : width / 2
        externalPointerY: glintPointer.hovered ? glintPointer.point.position.y : height / 2
        fillColor: "transparent"
        borderWidth: plate.focused ? 2 : plate.outlined ? 1 : 0
        borderColor: plate.focused ? Theme.focus : plate.outlineColor
        curveDepth: plate.cornerRadius
        neonStrength: plate.neonStrength
    }
}
