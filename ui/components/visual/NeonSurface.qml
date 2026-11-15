import QtQuick
import Farcaster.Client

// One material owner for the surface, its focus outline, and its Neon rim.
// Adapted from the user-provided Genyleap Neon UI Kit v0.4.5 NeonSurface.
// Keep GenyCaster's retained QSG Superellipse and theme tokens; never add
// Shape/Canvas objects to every feed row.
Superellipse {
    id: material

    property bool interactive: false
    property bool ambientNeon: false
    // Virtualized Cast cards defer the GPU shader until actual pointer entry,
    // then keep it alive through the exit fade, unlike dashboard cards.
    property bool deferredNeon: false
    // Activity belongs to the material; never shadow the consumer's public
    // `busy` property (e.g. FeedPulseCard's data-loading state).
    property bool neonBusy: false
    property bool inverted: false
    property bool focused: false
    property bool animatePaint: true
    // A parent Control may own pointer delivery. Expose its hover as an
    // explicit input; otherwise the surface owns a passive HoverHandler.
    property bool externalHovered: false
    property real externalPointerX: width * 0.5
    property real externalPointerY: height * 0.5
    property real neonStrength: Theme.neonControlStrength
    property real ambientStrength: Theme.neonAmbientStrength
    property color neonColor: "transparent"

    readonly property bool pointerActive: interactive && (externalHovered || pointer.hovered)
    property real hoverLifetime: pointerActive ? 1 : 0
    Behavior on hoverLifetime {
        enabled: material.deferredNeon && Theme.neonMotionEnabled
        NumberAnimation {
            duration: material.pointerActive ? Theme.neonEnterDuration : Theme.neonExitDuration
            easing.type: Easing.OutCubic
        }
    }
    // Unlike the first Neon integration, the material is the sole owner of
    // both paint transitions and rim instantiation. Dense plain rows never
    // allocate ShaderEffects unless they explicitly opt into Neon.
    Behavior on fillColor {
        enabled: material.animatePaint && !Theme.reduceMotion
        ColorAnimation {
            duration: Theme.materialSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }
    Behavior on borderColor {
        enabled: material.animatePaint && !Theme.reduceMotion
        ColorAnimation {
            duration: Theme.materialSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }
    readonly property real pointerPositionX: externalHovered ? externalPointerX : pointer.hovered ? pointer.point.position.x : width / 2
    readonly property real pointerPositionY: externalHovered ? externalPointerY : pointer.hovered ? pointer.point.position.y : height / 2

    HoverHandler {
        id: pointer
        enabled: material.interactive && Theme.neonEnabled
        blocking: false
    }

    Loader {
        anchors.fill: parent
        active: material.visible && Theme.neonEnabled
                && (material.ambientNeon || material.neonBusy
                    || (material.interactive && (!material.deferredNeon
                                                || material.pointerActive
                                                || material.hoverLifetime > 0.001)))
        sourceComponent: NeonHalo {
            anchors.fill: parent
            active: material.pointerActive
            ambient: material.ambientNeon
            activity: material.neonBusy
            hoverCapable: material.interactive
            pointerX: material.pointerPositionX
            pointerY: material.pointerPositionY
            cornerRadius: material.curveDepth
            cornerPower: material.exponent
            borderInset: material.borderWidth / 2
            inverted: material.inverted
            tintColor: material.neonColor
            strength: material.neonStrength
            ambientStrength: material.ambientStrength
        }
    }
}
