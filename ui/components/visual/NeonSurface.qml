import QtQuick
import Farcaster.Client

// One material owner for the surface, its focus outline, and its Neon rim.
// Adapted from the user-provided Genyleap Neon UI Kit v0.4.5 NeonSurface.
// Keep GenyCaster's retained QSG Superellipse and theme tokens; never add
// Shape/Canvas objects to every feed row.
Superellipse {
    id: material

    property bool interactive: false
    property NeonReflectionSource reflectionSource: null
    ReflectionCoordinates.source: reflectionSource && reflectionSource.capturing
        ? reflectionSource.sourceItem : null
    property rect reflectionRect: ReflectionCoordinates.bounds
    property real reflectionStrength: Theme.reflectionStrength
    property real neonReflectionStrength: Theme.neonReflectionStrength
    property bool reflectedBorder: true
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
    // Shared, passive pointer for collections. Like the original Neon Kit
    // product grid, a single parent HoverHandler sees gutters as well as
    // card interiors, so adjacent cards react simultaneously.
    property bool proximityPointerActive: false
    property real proximityPointerX: width / 2
    property real proximityPointerY: height / 2
    property real proximityReach: 85
    readonly property real proximityDx: Math.max(0, -proximityPointerX,
                                                 proximityPointerX - width)
    readonly property real proximityDy: Math.max(0, -proximityPointerY,
                                                 proximityPointerY - height)
    readonly property real proximityStrength: proximityPointerActive && interactive
        ? Math.max(0, 1 - Math.hypot(proximityDx, proximityDy) / Math.max(1, proximityReach))
        : 0
    readonly property bool pointerActive: interactive && (externalHovered
        || pointer.hovered || proximityStrength > 0)
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
        enabled: material.animatePaint && !Theme.reduceMotion && !Theme.paletteSwitchInProgress
        ColorAnimation {
            duration: Theme.materialSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }
    Behavior on borderColor {
        enabled: material.animatePaint && !Theme.reduceMotion && !Theme.paletteSwitchInProgress
        ColorAnimation {
            duration: Theme.materialSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }
    readonly property real pointerPositionX: proximityPointerActive && proximityStrength > 0
        ? proximityPointerX : externalHovered ? externalPointerX
        : pointer.hovered ? pointer.point.position.x : width / 2
    readonly property real pointerPositionY: proximityPointerActive && proximityStrength > 0
        ? proximityPointerY : externalHovered ? externalPointerY
        : pointer.hovered ? pointer.point.position.y : height / 2

    HoverHandler {
        id: pointer
        enabled: material.interactive && Theme.neonEnabled
        blocking: false
    }

    // The chromatic body and stationary border are independent of pointer
    // lighting. Only allocate for explicit adjacent sources, not every cast.
    Loader {
        anchors.fill: parent
        active: material.visible && Theme.reflectionGain > 0
            && !!material.reflectionSource && material.reflectionSource.capturing
            && !material.focused
        sourceComponent: NeonReflection {
            anchors.fill: parent
            reflectionSource: material.reflectionSource
            sampleRect: material.reflectionRect
            strength: material.reflectionStrength
            cornerRadius: material.curveDepth
            cornerPower: material.exponent
            borderInset: material.borderWidth / 2
            edgeStrength: material.reflectedBorder && material.borderWidth > 0 ? 0.72 : 0
            edgeWidth: Math.max(1, material.borderWidth)
            edgeInset: material.borderWidth / 2
            reach: Theme.reflectionReach
        }
    }

    Loader {
        anchors.fill: parent
        active: material.visible && Theme.neonEnabled
                && (material.ambientNeon || material.neonBusy
                    || (material.interactive && (material.focused
                                                || !material.deferredNeon
                                                || material.pointerActive
                                                || material.hoverLifetime > 0.001)))
        sourceComponent: NeonHalo {
            anchors.fill: parent
            // Keyboard focus must activate the same glint as pointer hover.
            active: material.pointerActive || material.focused
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
            reflectionSource: material.reflectionSource
            reflectionRect: material.reflectionRect
            reflectionStrength: material.neonReflectionStrength
            reflectionReach: Theme.reflectionReach
            // Focus outline and Neon are compatible, not mutually exclusive.
            suppressed: false
            strength: material.neonStrength * (material.proximityPointerActive
                ? material.proximityStrength : 1)
            ambientStrength: material.ambientStrength
            curveTopLeft: material.curveTopLeft
            curveTopRight: material.curveTopRight
            curveBottomRight: material.curveBottomRight
            curveBottomLeft: material.curveBottomLeft
        }
    }
}
