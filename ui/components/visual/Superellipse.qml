import QtQuick
import QtQuick.Window
import QtQuick.Shapes
import Farcaster.Client

Item {
    id: root

    implicitWidth: 1
    implicitHeight: 1

    property color fillColor: "transparent"
    property color borderColor: "transparent"
    property real borderWidth: 0
    property real curveDepth: Theme.curveControl
    property real exponent: Theme.superellipseExponent
    // Retained for API compatibility with the previous scene-graph renderer.
    // Visual rendering no longer depends on a line-segment sampling budget.
    property int cornerSamples: 96
    property int elevation: Theme.elevationNone
    property bool shadowEnabled: false
    property color shadowColor: Theme.shadow
    property real shadowOpacityScale: 1.0
    property bool curveTopLeft: true
    property bool curveTopRight: true
    property bool curveBottomRight: true
    property bool curveBottomLeft: true
    // Small focal controls can opt into Qt's CurveRenderer for native analytic
    // antialiasing. Timeline/card surfaces stay on the retained QSG geometry
    // path so scrolling does not recreate Shape/ShapePath object trees.
    property bool highQualityEdges: false

    // A shared QML Superellipse is instantiated hundreds of times in timelines.
    // Keep the QML object tree to one retained scene-graph item instead of a Shape,
    // ShapePath, and eight path-element QObjects per surface. Geometry is rebuilt
    // only when the item's size/style changes and is rendered directly by QSG.
    //
    // Hit testing intentionally remains on this lightweight wrapper's rectangular
    // bounds; the child renderer's mathematical contains() is not installed as a
    // containment mask for ordinary controls.
    SceneGraphSuperellipse {
        id: renderedShape
        anchors.fill: parent
        visible: !root.highQualityEdges && root.width > 0 && root.height > 0
        fillColor: root.fillColor
        borderColor: root.borderColor
        borderWidth: root.borderWidth
        curveDepth: root.curveDepth
        exponent: root.exponent
        qualityScale: typeof renderingController !== "undefined" && renderingController !== null
                      ? renderingController.geometryQualityScale : 1.0
        curveTopLeft: root.curveTopLeft
        curveTopRight: root.curveTopRight
        curveBottomRight: root.curveBottomRight
        curveBottomLeft: root.curveBottomLeft
    }

    Loader {
        anchors.fill: parent
        active: root.highQualityEdges
        asynchronous: false

        sourceComponent: Component {
            Shape {
                id: precisionShape
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                asynchronous: false

                readonly property real pathInset: root.borderWidth > 0 ? root.borderWidth / 2 : 0
                readonly property real pathLeft: pathInset
                readonly property real pathTop: pathInset
                readonly property real pathRight: Math.max(pathLeft, width - pathInset)
                readonly property real pathBottom: Math.max(pathTop, height - pathInset)
                readonly property real pathWidth: Math.max(0, pathRight - pathLeft)
                readonly property real pathHeight: Math.max(0, pathBottom - pathTop)
                // Qt 6.11 CurveRenderer can produce a tiny axial spike when a
                // continuous corner consumes an edge exactly (pill/circle case).
                // Leave half a physical pixel of total straight-edge length so
                // no PathLine join degenerates to zero length.
                readonly property real edgeGuard: 0.25 / Math.max(1, Screen.devicePixelRatio)
                readonly property real requestedCornerRadius: Math.max(
                    0,
                    Math.min(root.curveDepth - pathInset, pathWidth / 2, pathHeight / 2))
                readonly property bool saturatesWidth: requestedCornerRadius > 0
                                                       && requestedCornerRadius >= pathWidth / 2 - 0.0001
                readonly property bool saturatesHeight: requestedCornerRadius > 0
                                                        && requestedCornerRadius >= pathHeight / 2 - 0.0001
                readonly property real cornerRadius: Math.max(
                    0,
                    Math.min(root.curveDepth - pathInset,
                             pathWidth / 2 - (saturatesWidth ? edgeGuard : 0),
                             pathHeight / 2 - (saturatesHeight ? edgeGuard : 0)))
                readonly property real topLeftRadius: root.curveTopLeft ? cornerRadius : 0
                readonly property real topRightRadius: root.curveTopRight ? cornerRadius : 0
                readonly property real bottomRightRadius: root.curveBottomRight ? cornerRadius : 0
                readonly property real bottomLeftRadius: root.curveBottomLeft ? cornerRadius : 0
                readonly property real cubicFactor: {
                    const safeExponent = Math.max(2.0, root.exponent)
                    const diagonal = Math.pow(0.5, 1 / safeExponent)
                    return Math.max(0.5522847498,
                                    Math.min(0.985, (8 * diagonal - 4) / 3))
                }

                ShapePath {
                    fillColor: root.fillColor
                    fillRule: ShapePath.WindingFill
                    strokeColor: root.borderWidth > 0 ? root.borderColor : "transparent"
                    strokeWidth: root.borderWidth > 0 ? root.borderWidth : -1
                    joinStyle: ShapePath.MiterJoin
                    capStyle: ShapePath.FlatCap

                    startX: precisionShape.pathLeft + precisionShape.topLeftRadius
                    startY: precisionShape.pathTop

                    PathLine {
                        x: precisionShape.pathRight - precisionShape.topRightRadius
                        y: precisionShape.pathTop
                    }
                    PathCubic {
                        x: precisionShape.pathRight
                        y: precisionShape.pathTop + precisionShape.topRightRadius
                        control1X: precisionShape.pathRight - precisionShape.topRightRadius
                                   + precisionShape.cubicFactor * precisionShape.topRightRadius
                        control1Y: precisionShape.pathTop
                        control2X: precisionShape.pathRight
                        control2Y: precisionShape.pathTop + precisionShape.topRightRadius
                                   - precisionShape.cubicFactor * precisionShape.topRightRadius
                    }
                    PathLine {
                        x: precisionShape.pathRight
                        y: precisionShape.pathBottom - precisionShape.bottomRightRadius
                    }
                    PathCubic {
                        x: precisionShape.pathRight - precisionShape.bottomRightRadius
                        y: precisionShape.pathBottom
                        control1X: precisionShape.pathRight
                        control1Y: precisionShape.pathBottom - precisionShape.bottomRightRadius
                                   + precisionShape.cubicFactor * precisionShape.bottomRightRadius
                        control2X: precisionShape.pathRight - precisionShape.bottomRightRadius
                                   + precisionShape.cubicFactor * precisionShape.bottomRightRadius
                        control2Y: precisionShape.pathBottom
                    }
                    PathLine {
                        x: precisionShape.pathLeft + precisionShape.bottomLeftRadius
                        y: precisionShape.pathBottom
                    }
                    PathCubic {
                        x: precisionShape.pathLeft
                        y: precisionShape.pathBottom - precisionShape.bottomLeftRadius
                        control1X: precisionShape.pathLeft + precisionShape.bottomLeftRadius
                                   - precisionShape.cubicFactor * precisionShape.bottomLeftRadius
                        control1Y: precisionShape.pathBottom
                        control2X: precisionShape.pathLeft
                        control2Y: precisionShape.pathBottom - precisionShape.bottomLeftRadius
                                   + precisionShape.cubicFactor * precisionShape.bottomLeftRadius
                    }
                    PathLine {
                        x: precisionShape.pathLeft
                        y: precisionShape.pathTop + precisionShape.topLeftRadius
                    }
                    PathCubic {
                        x: precisionShape.pathLeft + precisionShape.topLeftRadius
                        y: precisionShape.pathTop
                        control1X: precisionShape.pathLeft
                        control1Y: precisionShape.pathTop + precisionShape.topLeftRadius
                                   - precisionShape.cubicFactor * precisionShape.topLeftRadius
                        control2X: precisionShape.pathLeft + precisionShape.topLeftRadius
                                   - precisionShape.cubicFactor * precisionShape.topLeftRadius
                        control2Y: precisionShape.pathTop
                    }
                }
            }
        }
    }
}
