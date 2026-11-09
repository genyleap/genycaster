import QtQuick
import QtQuick.Shapes
import Farcaster.Client

Item {
    id: root

    property var samples: []
    property color strokeColor: Theme.accent
    property real strokeWidth: 1.9
    property real contentPadding: 2
    property bool fillArea: true
    property bool smoothCurve: false

    // Market sampling, smoothing and coordinate math are owned by C++.
    // QML retains reactive item geometry, palette and native Shape rendering.
    readonly property var sparklineGeometry: root.smoothCurve
        ? TokenSparklineGeometry.computeSmooth(root.samples, root.width, root.height,
                                               root.contentPadding, root.strokeWidth)
        : TokenSparklineGeometry.compute(root.samples, root.width, root.height,
                                         root.contentPadding, root.strokeWidth)
    readonly property var normalizedPoints: root.sparklineGeometry.linePoints
    readonly property var areaPoints: root.sparklineGeometry.areaPoints

    Shape {
        anchors.fill: parent
        // C++ has already sampled a monotone smooth path. Re-tessellating its
        // dense PathPolyline with CurveRenderer can create faceted edge bands.
        // A local 4x-MSAA layer covers the stroke and translucent fill
        // together, independent of the window's global MSAA setting.
        preferredRendererType: Shape.GeometryRenderer
        asynchronous: false
        antialiasing: true
        layer.enabled: root.smoothCurve
        layer.smooth: true
        layer.samples: 4

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: -1
            fillColor: root.fillArea && root.areaPoints.length >= 4
                       ? Qt.rgba(root.strokeColor.r,
                                 root.strokeColor.g,
                                 root.strokeColor.b,
                                 Theme.dark ? 0.13 : 0.085)
                       : "transparent"
            // An uninitialized/empty Qt QVariantList is not a valid PathPolyline
            // path under every Qt 6.12 renderer. Degenerate points are inert
            // while the fill is transparent and avoid scene-graph warnings.
            PathPolyline {
                path: root.areaPoints && root.areaPoints.length >= 4
                      ? root.areaPoints
                      : [Qt.point(0, 0), Qt.point(0, 0), Qt.point(0, 0), Qt.point(0, 0)]
            }
        }

        ShapePath {
            strokeColor: root.normalizedPoints.length >= 2 ? root.strokeColor : "transparent"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline {
                path: root.normalizedPoints && root.normalizedPoints.length >= 2
                      ? root.normalizedPoints
                      : [Qt.point(0, 0), Qt.point(0, 0)]
            }
        }
    }
}
