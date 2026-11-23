import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Composer/chat media tile. Images render through the shared image path;
// videos render through the canonical GenyVideoPlayer media stack with the
// player chrome disabled - the tile owns only its own compact composer
// surface (border, play affordance, failure hint), never a second
// MediaPlayer/video pipeline.
Item {
    id: root

    property url source: ""
    property string mediaType: ""
    property real curveDepth: Theme.shapeMedia
    property bool interactive: true
    property bool showBorder: true
    // Composing a post must show the entire submitted image, not a cropped
    // square that conceals the actual content.
    property bool fitImage: false
    // Explicit full-fidelity decode budget for authored media inspection.
    // Feed tiles keep their adaptive (often smaller) decode policy.
    property int imageDecodeEdge: 0
    property color fillColor: Theme.surfaceMuted
    property bool primeVideoFrame: false
    property bool showFailureText: width >= 180
    property bool userPlayback: false
    property bool primedVideoFrame: false

    readonly property bool isImage: String(root.mediaType || "").startsWith("image/")
    readonly property bool isVideo: String(root.mediaType || "").startsWith("video/")
    readonly property bool hasSource: root.source.toString().length > 0
    readonly property bool videoPlaying: videoCore.playing
    readonly property bool failed: root.hasSource
                                   && (root.isImage ? imagePreview.loadFailed
                                       : root.isVideo ? videoCore.failed : true)

    clip: false

    onSourceChanged: {
        root.userPlayback = false
        root.primedVideoFrame = false
    }

    AmbientMediaBackdrop {
        anchors.fill: parent
        sourceItem: imagePreview.renderItem
        active: root.isImage && imagePreview.ready && !root.failed
        spread: 11
        strength: Theme.dark ? 0.82 : 0.70
        blurStrength: 0.86
        saturation: 0.10
        sampleSize: 88
    }

    Superellipse {
        anchors.fill: parent
        fillColor: root.fillColor
        borderWidth: root.showBorder ? 1 : 0
        borderColor: Theme.borderMuted
        curveDepth: root.curveDepth
    }

    SuperellipseImage {
        id: imagePreview
        anchors.fill: parent
        visible: root.isImage && root.hasSource
        source: root.isImage ? root.source : ""
        mediaCategory: "photos"
        requestedSourceWidth: root.imageDecodeEdge
        requestedSourceHeight: root.imageDecodeEdge
        fillMode: root.fitImage ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        curveDepth: root.curveDepth
        borderWidth: 0
    }

    GenyVideoPlayer {
        id: videoCore
        anchors.fill: parent
        anchors.margins: root.showBorder ? 1 : 0
        visible: root.isVideo && root.hasSource && !root.failed
        source: root.isVideo ? root.source : ""
        interactive: false
        showChrome: false
        fullscreenEnabled: false
        forceMuted: true
        // Never initialize the decoder just to manufacture a thumbnail.
        // Playback remains user-initiated through the shared player path.
        eager: false
        primeFrame: false
        curveDepth: root.curveDepth
        cornerMaskColor: root.fillColor

    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.max(0, parent.width - Theme.space3 * 2)
        spacing: Theme.space1
        visible: !root.hasSource || root.failed || (!root.isImage && !root.isVideo)

        Icon {
            Layout.alignment: Qt.AlignHCenter
            text: root.isVideo ? Theme.iconPlay : Theme.iconFile
            color: Theme.inkSubtle
            preferredSize: Theme.typeBodySize
        }

        DirectionAwareLabel {
            visible: root.failed && root.showFailureText
            Layout.fillWidth: true
            text: qsTrId("genycaster.embed.media_unavailable")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    // Composer tile chrome: a single play/pause affordance over the shared
    // video stack. It does not trigger hidden first-frame decoding.
    Superellipse {
        visible: root.isVideo && root.hasSource && !root.failed
        anchors.centerIn: parent
        width: Math.min(46, Math.max(30, parent.width * 0.28))
        height: width
        fillColor: Theme.mediaOverlay
        curveDepth: width / 2
        exponent: 2.01
        opacity: root.videoPlaying ? 0.70 : 0.88

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.videoPlaying ? 0 : 1
            text: root.videoPlaying ? Theme.iconPause : Theme.iconPlay
            color: Theme.accentInk
            preferredSize: Theme.typeLabelSize
        }
    }

    TapHandler {
        enabled: root.interactive && root.isVideo && root.hasSource && !root.failed
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            root.userPlayback = true
            root.primedVideoFrame = true
            if (videoCore.playing)
                videoCore.pausePlayback()
            else
                videoCore.resumePlayback()
        }
    }

    GenySpinner {
        anchors.centerIn: parent
        visible: root.isVideo && videoCore.userActivated && videoCore.loading
                 && !root.videoPlaying
        running: visible
        Accessible.ignored: true
    }

    HoverHandler {
        id: hover
        enabled: root.interactive
    }

    Superellipse {
        anchors.fill: parent
        visible: hover.hovered && root.interactive
        fillColor: "transparent"
        borderWidth: 1
        borderColor: Theme.accent
        curveDepth: root.curveDepth
        opacity: 0.32
    }
}
