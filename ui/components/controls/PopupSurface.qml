import QtQuick
import Farcaster.Client

Superellipse {
    id: plate
    property bool neonAmbient: true
    property bool neonInteractive: true
    property bool neonBusy: false
    fillColor: Theme.surfaceElevated
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveModal
    elevation: Theme.elevationModal
    shadowOpacityScale: 0.92

    HoverHandler {
        id: platePointer
        enabled: Theme.neonEnabled && plate.neonInteractive
        blocking: false
    }

    // The one shared transient surface owns its perimeter shader, not every
    // inner label or row. Animated content stays independent of the rim.
    NeonHalo {
        anchors.fill: parent
        ambient: plate.neonAmbient
        // Source pointer events are owned by the material, not by the halo.
        active: platePointer.hovered
        interactive: false
        pointerX: platePointer.point.position.x
        pointerY: platePointer.point.position.y
        activity: plate.neonBusy
        strength: Theme.dark ? 1.0 : 0.88
        ambientStrength: Theme.dark ? 0.88 : 0.72
        cornerRadius: plate.curveDepth
        borderInset: plate.borderWidth / 2
    }
}
