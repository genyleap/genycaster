import QtQuick

// Preview the EXACT PNG that will be attached to a public Cast. Keeping one
// raster renderer avoids QML/WebGL grabToImage stalls and WYSIWYG drift.
Item {
    id: root
    objectName: "genycasterRichStatsCard"
    width: 960
    height: variantStyle === "minimal" ? 480 : 960

    property string variantStyle: "neon"
    property url sourceUrl: ""
    readonly property bool ready: previewImage.status === Image.Ready
    readonly property bool failed: previewImage.status === Image.Error

    Image {
        id: previewImage
        anchors.fill: parent
        source: root.sourceUrl
        // Decode the source at its native export resolution; the style owns
        // its aspect ratio and neither QML nor the operating system crops it.
        sourceSize: Qt.size(1920, root.variantStyle === "minimal" ? 960 : 1920)
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: false
        smooth: true
        mipmap: false
    }
}
