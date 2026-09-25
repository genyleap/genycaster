import QtQuick
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
        visible: root.width > 0 && root.height > 0
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
}
