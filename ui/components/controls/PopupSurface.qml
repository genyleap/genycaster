import QtQuick
import Farcaster.Client

// Every transient panel shares one Kit-style material. The surface owns the
// border, pointer reflection and a stationary rim; no stacked ShaderEffects.
NeonSurface {
    id: plate
    property bool neonAmbient: true
    property bool neonInteractive: true

    ambientNeon: neonAmbient
    interactive: neonInteractive
    neonStrength: Theme.neonPanelStrength
    ambientStrength: Theme.neonAmbientStrength
    fillColor: Theme.surfaceElevated
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveModal
    elevation: Theme.elevationModal
    shadowOpacityScale: 0.92
}
