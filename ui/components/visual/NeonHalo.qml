import QtQuick
import Farcaster.Client

// Product-owned adaptation of Genyleap Neon UI Kit v0.4.5.
// The owning control supplies hover state and pointer coordinates; a handler
// inside a Control.background alone cannot reliably see pointer hover.
// Ambient is static, while genuine busy state may orbit the perimeter.
Item {
    id: root

    property bool active: false
    property bool interactive: false
    property bool ambient: false
    property bool activity: false
    property real pointerX: width / 2
    property real pointerY: height / 2
    property real cornerRadius: Theme.curveContainer
    property real borderInset: Theme.strokeStandard / 2
    property real strength: Theme.dark ? 0.95 : 0.75
    property real ambientStrength: Theme.dark ? 0.72 : 0.62
    property real reach: 170
    property color tintColor: "transparent"

    readonly property bool hovered: interactive && hoverHandler.hovered
    readonly property bool tracking: Theme.neonEnabled && (active || hovered)
    readonly property bool activityActive: Theme.neonEnabled && activity
    readonly property bool ambientActive: Theme.neonEnabled && ambient
    readonly property bool responding: tracking || activityActive || ambientActive
    readonly property bool motionAllowed: Theme.neonMotionEnabled
    readonly property int orbitDuration: Math.round(Math.max(
        1800, Math.min(9000, 2 * (Math.max(1, width) + Math.max(1, height)) * 1000 / 260)))

    property real activityPhase: 0
    NumberAnimation on activityPhase {
        from: 0
        to: 1
        duration: root.orbitDuration
        loops: Animation.Infinite
        running: root.activityActive && root.visible && root.motionAllowed
        easing.type: Easing.Linear
    }

    function orbitPoint(progress) {
        const w = Math.max(1, width)
        const h = Math.max(1, height)
        const r = Math.max(0, Math.min(cornerRadius, w / 2, h / 2))
        const straightH = w - 2 * r
        const straightV = h - 2 * r
        const quarter = 1.76 * r
        const perimeter = Math.max(1, 2 * (straightH + straightV + 2 * quarter))
        let distance = ((progress % 1) + 1) % 1 * perimeter
        const exponent = 2 / Math.max(2, Theme.superellipseExponent)
        const power = value => Math.pow(Math.max(0, value), exponent)

        if (distance <= straightH) return Qt.point(r + distance, 0)
        distance -= straightH
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2
            return Qt.point(w - r + r * power(Math.sin(t)), r - r * power(Math.cos(t)))
        }
        distance -= quarter
        if (distance <= straightV) return Qt.point(w, r + distance)
        distance -= straightV
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2
            return Qt.point(w - r + r * power(Math.cos(t)), h - r + r * power(Math.sin(t)))
        }
        distance -= quarter
        if (distance <= straightH) return Qt.point(w - r - distance, h)
        distance -= straightH
        if (quarter > 0 && distance <= quarter) {
            const t = distance / quarter * Math.PI / 2
            return Qt.point(r - r * power(Math.sin(t)), h - r + r * power(Math.cos(t)))
        }
        distance -= quarter
        if (distance <= straightV) return Qt.point(0, h - r - distance)
        distance -= straightV
        if (quarter > 0) {
            const t = Math.min(1, distance / quarter) * Math.PI / 2
            return Qt.point(r - r * power(Math.cos(t)), r - r * power(Math.sin(t)))
        }
        return Qt.point(0, 0)
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.interactive && Theme.neonEnabled
        blocking: false
    }

    readonly property point pointer: activityActive && motionAllowed
        ? orbitPoint(activityPhase)
        : tracking ? (active ? Qt.point(pointerX, pointerY)
                             : hoverHandler.point.position)
                   : Qt.point(width / 2, height / 2)
    readonly property real edgeDistance: Math.max(0, Math.min(
        pointer.x, width - pointer.x, pointer.y, height - pointer.y))

    // Kept writable: Behavior must never target a readonly QML property.
    property real envelope: responding ? 1 : 0
    Behavior on envelope {
        NumberAnimation {
            duration: Theme.neonMotionEnabled
                      ? (root.responding ? Theme.motionFast : Theme.motionStandard) : 0
            easing.type: Easing.OutCubic
        }
    }

    visible: Theme.neonEnabled && width > 0 && height > 0
             && (ambient || interactive || active || activity || envelope > 0.001)

    ShaderEffect {
        anchors.fill: parent
        blending: true
        property vector4d surfaceParameters: Qt.vector4d(
            Math.max(1, root.width), Math.max(1, root.height),
            root.cornerRadius, Theme.superellipseExponent)
        property vector4d focusParameters: Qt.vector4d(
            root.pointer.x, root.pointer.y, Math.max(18, root.reach * 0.43),
            Math.min(10000, root.edgeDistance))
        property vector4d lightParameters: Qt.vector4d(
            Theme.dark ? 0.96 : 0.10,
            root.envelope * (root.activityActive ? root.strength
                           : root.tracking ? root.strength : root.ambientStrength),
            42, root.borderInset)
        property vector4d tintParameters: Qt.vector4d(
            root.tintColor.r, root.tintColor.g, root.tintColor.b, root.tintColor.a)
        property real steadyActivity: root.activityActive && root.motionAllowed
            ? 0 : root.tracking ? 0 : 1
        fragmentShader: "qrc:/genycaster/effects/neon.frag.qsb"
    }
}
