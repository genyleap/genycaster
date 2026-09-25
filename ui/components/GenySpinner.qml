import QtQuick
import QtQuick.Shapes
import Farcaster.Client

/**
 * Smooth indeterminate progress indicator for compact media and feed surfaces.
 * CurveRenderer avoids the faceted/jagged arc produced by geometry tessellation
 * at small sizes and fractional device-pixel ratios.
 */
Item {
    id: root

    property color color: Theme.accent
    property real strokeWidth: 2
    property bool running: visible
    property var scrollHost: null
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    readonly property real arcRadius: Math.max(0, Math.min(width, height) / 2 - root.strokeWidth / 2)

    function resolveScrollHost() {
        let candidate = root.parent
        while (candidate !== null) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined) {
                root.scrollHost = candidate
                return
            }
            candidate = candidate.parent
        }
        root.scrollHost = null
    }

    Component.onCompleted: root.resolveScrollHost()
    onParentChanged: root.resolveScrollHost()

    implicitWidth: 18
    implicitHeight: 18
    Accessible.role: Accessible.ProgressBar

    // One Shape owns both static paths. Rotating the full-circle track together
    // with the active arc is visually identical, and avoids a second Shape node
    // for every pending row in a timeline.
    Shape {
        id: spinnerShape
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        asynchronous: false
        transformOrigin: Item.Center

        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.14)
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: -90
                sweepAngle: 359.999
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: -90
                sweepAngle: 248
            }
        }

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 760
            loops: Animation.Infinite
            running: root.running && root.visible && !root.scrollMoving && !Theme.reduceMotion
        }
    }
}
