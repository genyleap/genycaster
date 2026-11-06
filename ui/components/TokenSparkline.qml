import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property var samples: []
    property color strokeColor: Theme.accent
    property real strokeWidth: 1.9
    property real contentPadding: 2
    property bool fillArea: true

    // Market sampling, smoothing and coordinate math are owned by C++.
    // QML retains reactive item geometry, palette and native Shape rendering.
    readonly property var sparklineGeometry: TokenSparklineGeometry.compute(
        root.samples, root.width, root.height,
        root.contentPadding, root.strokeWidth)
    readonly property var normalizedPoints: root.sparklineGeometry.linePoints
    readonly property var areaPoints: root.sparklineGeometry.areaPoints

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.GeometryRenderer
        asynchronous: false
        antialiasing: true

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: -1
            fillColor: root.fillArea && root.areaPoints.length >= 4
                       ? Qt.rgba(root.strokeColor.r,
                                 root.strokeColor.g,
                                 root.strokeColor.b,
                                 Theme.dark ? 0.13 : 0.085)
                       : "transparent"
            PathPolyline { path: root.areaPoints }
        }

        ShapePath {
            strokeColor: root.normalizedPoints.length >= 2 ? root.strokeColor : "transparent"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.normalizedPoints }
        }
    }
}
