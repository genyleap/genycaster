import QtQuick

// Preview the EXACT PNG that will be attached to a public Cast. Keeping one
// raster renderer avoids QML/WebGL grabToImage stalls and WYSIWYG drift.
Item {
    id: root
    objectName: "genycasterRichStatsCard"
    width: 960
    height: 960

    property url sourceUrl: ""
    readonly property bool ready: previewImage.status === Image.Ready
    readonly property bool failed: previewImage.status === Image.Error

    Image {
        id: previewImage
        anchors.fill: parent
        source: root.sourceUrl
        // Decode the entire 2x CPU-rendered PNG; never ask Qt to downsample
        // to the logical card size before the device composites the preview.
        sourceSize: Qt.size(1920, 1920)
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: false
        smooth: true
        mipmap: false
    }
}
