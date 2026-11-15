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

    // The one shared transient surface owns its perimeter shader, not every
    // inner label or row. Animated content stays independent of the rim.
    NeonHalo {
        anchors.fill: parent
        ambient: plate.neonAmbient
        interactive: plate.neonInteractive
        activity: plate.neonBusy
        cornerRadius: plate.curveDepth
        borderInset: plate.borderWidth / 2
    }
}
