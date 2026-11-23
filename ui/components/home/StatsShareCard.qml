import QtQuick

// Preview the EXACT PNG that will be attached to a public Cast. Keeping one
// raster renderer avoids QML/WebGL grabToImage stalls and WYSIWYG drift.
Item {
    id: root
    objectName: "genycasterRichStatsCard"
    width: 960
    height: 600

    property url sourceUrl: ""
    readonly property bool ready: previewImage.status === Image.Ready
    readonly property bool failed: previewImage.status === Image.Error

    Image {
        id: previewImage
        anchors.fill: parent
        source: root.sourceUrl
        sourceSize: Qt.size(960, 600)
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: false
        smooth: true
        mipmap: false
    }
}
