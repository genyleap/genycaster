import QtQuick
import Farcaster.Client

// GPU edge reflection adapted from the user-supplied Neon UI Kit v0.4.5.
// At rest the rim is static; hover glints are localized; only genuine busy
// surfaces may animate a perimeter orbit. No source textures or input grabs.
Item {
    id: root
    property bool ambient: false
    property bool interactive: false
    property bool activity: false
    property real cornerRadius: Theme.curveContainer
    property real borderInset: Theme.strokeStandard / 2
    property real strength: Theme.dark ? 0.70 : 0.42
    property real ambientStrength: Theme.dark ? 0.38 : 0.24
    property real reach: 150

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool activityActive: Theme.neonEnabled && activity
    readonly property bool responding: Theme.neonEnabled && (ambient || hovered || activity)
    property real activityPhase: 0
    readonly property int orbitDuration: Math.round(Math.max(1800, Math.min(9000, (2 * (Math.max(0, width) + Math.max(0, height))) * 1000 / 260)))
    NumberAnimation on activityPhase {
        from: 0
        to: 1
        duration: root.orbitDuration
        loops: Animation.Infinite
        running: root.activityActive && root.visible && !Theme.reduceMotion
        easing.type: Easing.Linear
    }

    // Approximate arc-length parameterization of the n=4 Superellipse
    // silhouette. Inactive instances never execute this calculation.
    function orbitPoint(progress) {
        const w = Math.max(1, width);
        const h = Math.max(1, height);
        const r = Math.max(0, Math.min(cornerRadius, w / 2, h / 2));
        const straightH = w - 2 * r;
        const straightV = h - 2 * r;
        const quarter = 1.76 * r;
        const perimeter = 2 * (straightH + straightV + 2 * quarter);
        let distance = ((progress % 1) + 1) % 1 * perimeter;
        const exponent = 2 / Math.max(2, Theme.superellipseExponent);
        const power = value => Math.pow(Math.max(0, value), exponent);
        if (distance <= straightH)
            return Qt.point(r + distance, 0);
        distance -= straightH;
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2;
            return Qt.point(w - r + r * power(Math.sin(t)), r - r * power(Math.cos(t)));
        }
        distance -= quarter;
        if (distance <= straightV)
            return Qt.point(w, r + distance);
        distance -= straightV;
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2;
            return Qt.point(w - r + r * power(Math.cos(t)), h - r + r * power(Math.sin(t)));
        }
        distance -= quarter;
        if (distance <= straightH)
            return Qt.point(w - r - distance, h);
        distance -= straightH;
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2;
            return Qt.point(r - r * power(Math.sin(t)), h - r + r * power(Math.cos(t)));
        }
        distance -= quarter;
        if (distance <= straightV)
            return Qt.point(0, h - r - distance);
        distance -= straightV;
        if (quarter > 0) {
            const t = Math.min(1, distance / quarter) * Math.PI / 2;
            return Qt.point(r - r * power(Math.cos(t)), r - r * power(Math.sin(t)));
        }
        return Qt.point(0, 0);
    }

    readonly property point pointer: activityActive && !Theme.reduceMotion ? orbitPoint(activityPhase) : hovered ? hoverHandler.point.position : Qt.point(width / 2, height / 2)
    readonly property real edgeDistance: Math.max(0, Math.min(pointer.x, width - pointer.x, pointer.y, height - pointer.y))
    property real response: responding ? 1 : 0
    Behavior on response {
        NumberAnimation {
            duration: Theme.reduceMotion ? 0 : (root.responding ? Theme.motionFast : Theme.motionStandard)
            easing.type: Easing.OutCubic
        }
    }
    visible: Theme.neonEnabled && (ambient || interactive || activity) && width > 0 && height > 0 && (response > 0.001 || responding)

    HoverHandler {
        id: hoverHandler
        enabled: root.interactive && Theme.neonEnabled
        blocking: false
    }
    ShaderEffect {
        anchors.fill: parent
        blending: true
        property vector4d surfaceParameters: Qt.vector4d(Math.max(1, root.width), Math.max(1, root.height), root.cornerRadius, Theme.superellipseExponent)
        property vector4d focusParameters: Qt.vector4d(root.pointer.x, root.pointer.y, Math.max(16, root.reach * 0.43), Math.min(10000, root.edgeDistance))
        property vector4d lightParameters: Qt.vector4d(Theme.dark ? 0.96 : 0.10, root.response * (root.activityActive ? root.strength : root.hovered ? root.strength : root.ambientStrength), 32, root.borderInset)
        // Reference rim is monochrome at rest; only hovered controls receive
        // a restrained accent glint.
        property vector4d tintParameters: Qt.vector4d(Theme.accent.r, Theme.accent.g, Theme.accent.b, root.hovered && !root.activityActive ? 0.22 : 0)
        property real steadyActivity: root.activityActive && !Theme.reduceMotion ? 0 : root.hovered ? 0 : 1
        fragmentShader: "qrc:/genycaster/effects/neon.frag.qsb"
    }
}
