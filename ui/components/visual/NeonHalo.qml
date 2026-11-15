import QtQuick
import Farcaster.Client

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

    // The Kit's analytic closest-point projection: four straight segments
    // and four n=power Lamé corner arcs. Minimization happens on pointer
    // updates, never in the fragment shader.
    function unitCorner(t, power) {
        const diagonal = Math.pow(0.5, 1 / power);
        if (t <= 0.5) {
            const x = t * 2 * diagonal;
            return {
                x: x,
                y: -Math.pow(Math.max(0, 1 - Math.pow(x, power)), 1 / power)
            };
        }
        const y = (1 - t) * 2 * diagonal;
        return {
            x: Math.pow(Math.max(0, 1 - Math.pow(y, power)), 1 / power),
            y: -y
        };
    }

    function nearestBorderPoint(px, py, w, h, radius, power) {
        const width = Math.max(0, w);
        const height = Math.max(0, h);
        if (width <= 0 || height <= 0 || !Number.isFinite(px) || !Number.isFinite(py))
            return {
                x: width / 2,
                y: height / 2,
                distance: Infinity
            };
        const r = Math.min(Math.max(0, radius), width / 2, height / 2);
        const exponent = Math.max(2, power);
        const clamp = (v, low, high) => Math.max(low, Math.min(high, v));
        let nearestX = 0;
        let nearestY = 0;
        let bestSq = Infinity;
        function consider(x, y) {
            const dx = px - x;
            const dy = py - y;
            const squared = dx * dx + dy * dy;
            if (squared < bestSq) {
                bestSq = squared;
                nearestX = x;
                nearestY = y;
            }
        }
        consider(clamp(px, r, width - r), 0);
        consider(width, clamp(py, r, height - r));
        consider(clamp(px, r, width - r), height);
        consider(0, clamp(py, r, height - r));
        if (r > 0) {
            const corners = [[r, r, -1, -1], [width - r, r, 1, -1], [width - r, height - r, 1, 1], [r, height - r, -1, 1]];
            const fraction = (Math.sqrt(5) - 1) * 0.5;
            for (const corner of corners) {
                const cx = corner[0];
                const cy = corner[1];
                const sx = corner[2];
                const sy = corner[3];
                const dx = px - clamp(px, cx + Math.min(0, sx * r), cx + Math.max(0, sx * r));
                const dy = py - clamp(py, cy + Math.min(0, sy * r), cy + Math.max(0, sy * r));
                if (dx * dx + dy * dy >= bestSq)
                    continue;
                function distSq(parameter) {
                    const p = unitCorner(parameter, exponent);
                    const xx = px - (cx + sx * r * p.x);
                    const yy = py - (cy - sy * r * p.y);
                    return xx * xx + yy * yy;
                }
                let lower = 0;
                let upper = 1;
                let first = upper - fraction * (upper - lower);
                let second = lower + fraction * (upper - lower);
                let firstDistance = distSq(first);
                let secondDistance = distSq(second);
                for (let step = 0; step < 18; ++step) {
                    if (firstDistance > secondDistance) {
                        lower = first;
                        first = second;
                        firstDistance = secondDistance;
                        second = lower + fraction * (upper - lower);
                        secondDistance = distSq(second);
                    } else {
                        upper = second;
                        second = first;
                        secondDistance = firstDistance;
                        first = upper - fraction * (upper - lower);
                        firstDistance = distSq(first);
                    }
                }
                const p = unitCorner((lower + upper) * 0.5, exponent);
                consider(cx + sx * r * p.x, cy - sy * r * p.y);
            }
        }
        return {
            x: nearestX,
            y: nearestY,
            distance: Math.sqrt(bestSq)
        };
    }

    // Retain the last pointer sample when leaving hover so the light fades
    // at the point of exit instead of jumping to the middle of the control.
    property point sampledPointer: Qt.point(width / 2, height / 2)
    property real sampledStrength: 0
    property real sampledSteady: 0
    property real activityPhase: 0
    readonly property int orbitDuration: Math.round(Math.max(1800, Math.min(9000, 1000 * (2 * (Math.max(0, width - 2 * cornerRadius) + Math.max(0, height - 2 * cornerRadius)) + 7.04 * cornerRadius) / Theme.neonOrbitSpeed)))
    NumberAnimation on activityPhase {
        from: 0
        to: 1
        duration: root.orbitDuration
        loops: Animation.Infinite
        running: root.activityActive && root.visible && Theme.neonMotionEnabled
        easing.type: Easing.Linear
    }
    function orbitPoint(progress) {
        const w = Math.max(1, width);
        const h = Math.max(1, height);
        const r = Math.max(0, Math.min(cornerRadius, w / 2, h / 2));
        const straightH = w - 2 * r;
        const straightV = h - 2 * r;
        const quarter = 1.76 * r;
        const perimeter = Math.max(1, 2 * (straightH + straightV + 2 * quarter));
        let d = ((progress % 1) + 1) % 1 * perimeter;
        const e = 2 / Math.max(2, cornerPower);
        const power = x => Math.pow(Math.max(0, x), e);
        if (d <= straightH)
            return Qt.point(r + d, 0);
        d -= straightH;
        if (quarter > 0 && d <= quarter) {
            const t = d / quarter * Math.PI / 2;
            return Qt.point(w - r + r * power(Math.sin(t)), r - r * power(Math.cos(t)));
        }
        d -= quarter;
        if (d <= straightV)
            return Qt.point(w, r + d);
        d -= straightV;
        if (quarter > 0 && d <= quarter) {
            const t = d / quarter * Math.PI / 2;
            return Qt.point(w - r + r * power(Math.cos(t)), h - r + r * power(Math.sin(t)));
        }
        d -= quarter;
        if (d <= straightH)
            return Qt.point(w - r - d, h);
        d -= straightH;
        if (quarter > 0 && d <= quarter) {
            const t = d / quarter * Math.PI / 2;
            return Qt.point(r - r * power(Math.sin(t)), h - r + r * power(Math.cos(t)));
        }
        d -= quarter;
        if (d <= straightV)
            return Qt.point(0, h - r - d);
        d -= straightV;
        if (quarter > 0) {
            const t = Math.min(1, d / quarter) * Math.PI / 2;
            return Qt.point(r - r * power(Math.cos(t)), r - r * power(Math.sin(t)));
        }
        return Qt.point(0, 0);
    }
    function samplePointer() {
        if (activityActive && Theme.neonMotionEnabled) {
            sampledPointer = orbitPoint(activityPhase);
            sampledStrength = Math.max(0, Math.min(1, activityStrength));
            sampledSteady = 0;
        } else if (tracking) {
            sampledPointer = Qt.point(active ? pointerX : pointerHandler.point.position.x, active ? pointerY : pointerHandler.point.position.y);
            sampledStrength = Math.max(0, Math.min(1, strength));
            sampledSteady = 0;
        } else if (ambientActive || activityActive) {
            sampledStrength = Math.max(0, Math.min(1, activityActive ? activityStrength : ambientStrength));
            sampledSteady = 1;
        }
    }

    onActivityPhaseChanged: Qt.callLater(samplePointer)
    onActivityActiveChanged: Qt.callLater(samplePointer)
    onAmbientActiveChanged: Qt.callLater(samplePointer)
    onStrengthChanged: Qt.callLater(samplePointer)
    onActivityStrengthChanged: Qt.callLater(samplePointer)
    onAmbientStrengthChanged: Qt.callLater(samplePointer)
    onTrackingChanged: Qt.callLater(samplePointer)
    onPointerXChanged: Qt.callLater(samplePointer)
    onPointerYChanged: Qt.callLater(samplePointer)
    onWidthChanged: Qt.callLater(samplePointer)
    onHeightChanged: Qt.callLater(samplePointer)
    Component.onCompleted: Qt.callLater(samplePointer)

    readonly property var edgePoint: nearestBorderPoint(sampledPointer.x - inset, sampledPointer.y - inset, Math.max(0, width - 2 * inset), Math.max(0, height - 2 * inset), cornerRadius, cornerPower)

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
