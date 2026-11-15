import QtQuick
import Farcaster.Client
import "NeonGeometry.js" as NeonGeometry

// Ported from the user-supplied Neon UI Kit v0.4.5 geometry/interaction
// contract. The effect and the retained GenyCaster QSG surface use the same
// Lamé (n=4) outline. Never capture the surface into its own shader.
Item {
    id: root

    property bool active: false
    property bool interactive: false
    property bool hoverCapable: false
    property bool ambient: false
    property bool activity: false
    property bool inverted: false
    property real pointerX: width / 2
    property real pointerY: height / 2
    property real cornerRadius: Theme.curveContainer
    property real cornerPower: Theme.superellipseExponent
    property real borderInset: Theme.strokeStandard / 2
    property real strength: Theme.neonCardStrength
    property real ambientStrength: Theme.neonAmbientStrength
    property real activityStrength: Theme.neonActivityStrength
    property real reach: 150
    property color tintColor: "transparent"

    readonly property bool useLight: Theme.dark !== inverted
    readonly property bool hovered: interactive && pointerHandler.hovered
    readonly property bool tracking: Theme.neonEnabled && (active || hovered) && enabled && width > 0 && height > 0
    readonly property bool activityActive: Theme.neonEnabled && activity && width > 0 && height > 0
    readonly property bool ambientActive: Theme.neonEnabled && ambient && width > 0 && height > 0
    readonly property bool responding: tracking || activityActive || ambientActive
    readonly property real inset: Math.min(Math.max(0, borderInset), width / 2, height / 2)
    readonly property real reflectionLength: Math.max(16, Math.min(135, reach * 0.43, Math.min(width, height) * 0.65))
    readonly property real edgeSigma: Math.max(12, Math.min(Theme.neonEdgeFalloff, Math.min(width, height) * 0.24))

    HoverHandler {
        id: pointerHandler
        enabled: root.interactive && Theme.neonEnabled
        blocking: false
    }

    // Geometry now comes from the Kit's exact shared arc-length library.
    // Keep it outside the transient QML object so no stale function closures
    // are retained when a feed delegate or drawer is destroyed.
    // Retain the last pointer sample when leaving hover so the light fades
    // at the point of exit instead of jumping to the middle of the control.
    property point sampledPointer: Qt.point(width / 2, height / 2)
    property real sampledStrength: 0
    property real sampledSteady: 0
    property real activityPhase: 0
    readonly property int orbitDuration: Math.round(Math.max(1800, Math.min(9000,
        1000 * NeonGeometry.borderLength(width - 2 * inset, height - 2 * inset,
                                         cornerRadius, cornerPower) / Theme.neonOrbitSpeed)))
    NumberAnimation on activityPhase {
        from: 0
        to: 1
        duration: root.orbitDuration
        loops: Animation.Infinite
        running: root.activityActive && root.visible && Theme.neonMotionEnabled
        easing.type: Easing.Linear
    }
    function samplePointer() {
        if (activityActive && Theme.neonMotionEnabled) {
            const point = NeonGeometry.borderPoint(activityPhase,
                width - 2 * inset, height - 2 * inset, cornerRadius, cornerPower)
            sampledPointer = Qt.point(point.x + inset, point.y + inset);
            sampledStrength = Math.max(0, Math.min(1, activityStrength));
            sampledSteady = 0;
        } else if (tracking && Number.isFinite(pointerX) && Number.isFinite(pointerY)) {
            sampledPointer = Qt.point(active ? pointerX : pointerHandler.point.position.x, active ? pointerY : pointerHandler.point.position.y);
            sampledStrength = Math.max(0, Math.min(1, strength));
            sampledSteady = 0;
        } else if (ambientActive || activityActive) {
            sampledStrength = Math.max(0, Math.min(1, activityActive ? activityStrength : ambientStrength));
            sampledSteady = 1;
        }
    }

    onActivityPhaseChanged: samplePointer()
    onActivityActiveChanged: samplePointer()
    onAmbientActiveChanged: samplePointer()
    onStrengthChanged: samplePointer()
    onActivityStrengthChanged: samplePointer()
    onAmbientStrengthChanged: samplePointer()
    onTrackingChanged: samplePointer()
    onPointerXChanged: samplePointer()
    onPointerYChanged: samplePointer()
    onWidthChanged: samplePointer()
    onHeightChanged: samplePointer()
    Component.onCompleted: samplePointer()

    readonly property var edgePoint: NeonGeometry.nearestBorderPoint(sampledPointer.x - inset, sampledPointer.y - inset, Math.max(0, width - 2 * inset), Math.max(0, height - 2 * inset), cornerRadius, cornerPower)

    property real envelope: responding ? 1 : 0
    Behavior on envelope {
        enabled: Theme.neonMotionEnabled
        NumberAnimation {
            duration: root.responding ? Theme.neonEnterDuration : Theme.neonExitDuration
            easing.type: Easing.OutCubic
        }
    }
    property real visibleStrength: sampledStrength
    Behavior on visibleStrength {
        enabled: Theme.neonMotionEnabled
        NumberAnimation {
            duration: Theme.neonSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }
    property real visualSteady: sampledSteady
    Behavior on visualSteady {
        enabled: Theme.neonMotionEnabled
        NumberAnimation {
            duration: Theme.neonSurfaceDuration
            easing.type: Easing.OutCubic
        }
    }

    visible: Theme.neonEnabled && width > 0 && height > 0 && (hoverCapable || ambient || activity || responding || envelope > 0.001)
    opacity: Theme.neonEnabled ? (Theme.neonMotionEnabled ? envelope : (responding ? 1 : 0)) : 0

    ShaderEffect {
        anchors.fill: parent
        blending: true
        property vector4d surfaceParameters: Qt.vector4d(Math.max(1, root.width), Math.max(1, root.height), root.cornerRadius, root.cornerPower)
        property vector4d focusParameters: Qt.vector4d(root.sampledPointer.x, root.sampledPointer.y, root.reflectionLength, Math.min(10000, root.edgePoint.distance))
        property vector4d lightParameters: Qt.vector4d(root.useLight ? Theme.neonLightLuminance : Theme.neonDarkLuminance, root.visibleStrength, root.edgeSigma, root.inset)
        property vector4d tintParameters: Qt.vector4d(root.tintColor.r, root.tintColor.g, root.tintColor.b, root.tintColor.a)
        property real steadyActivity: root.visualSteady
        fragmentShader: "qrc:/genycaster/effects/neon.frag.qsb"
    }
}
