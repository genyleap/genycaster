import QtQuick
import QtQuick.Controls
import QtQuick.Window
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
    // Composer-only: show the uploaded canonical image at intrinsic quality,
    // not the 96px JPEG emitted for the in-flight selection indicator.
    property bool fullQualityImage: false
    property bool minimalChrome: false
    property color fillColor: Theme.surfaceMuted
    property bool primeVideoFrame: false
    property bool showFailureText: width >= 180
    property bool userPlayback: false
    MediaResourceScope {
        id: resourceScope
        owner: root
        onEligibleChanged: if (!eligible) root.releasePlayback()
        onRevoked: root.releasePlayback()
    }
    Timer {
        id: releaseTimer
        interval: 0
        onTriggered: root.releasePlayback()
    }
    property bool primedVideoFrame: false

    // Intrinsic raster ratio for Composer layouts. The preview must reserve
    // the image's actual height instead of masking a square into a short tile.
    readonly property real imageAspectRatio: imagePreview.sourceImplicitHeight > 1
        && imagePreview.sourceImplicitWidth > 1
        ? imagePreview.sourceImplicitWidth / imagePreview.sourceImplicitHeight
        : (fullImage.implicitHeight > 1 && fullImage.implicitWidth > 1
            ? fullImage.implicitWidth / fullImage.implicitHeight : 1)
    readonly property bool isImage: String(root.mediaType || "").startsWith("image/")
    readonly property bool isVideo: String(root.mediaType || "").startsWith("video/")
    readonly property bool hasSource: root.source.toString().length > 0
    readonly property bool videoPlaying: videoLoader.item !== null && videoLoader.item.playing
    readonly property bool failed: root.hasSource
                                   && (root.isImage
                                       ? (root.fullQualityImage
                                           ? fullImage.status === Image.Error && imagePreview.loadFailed
                                           : imagePreview.loadFailed)
                                       : root.isVideo ? (videoLoader.status === Loader.Error
                                           || (videoLoader.item !== null && videoLoader.item.failed)) : true)

    clip: false

    function releasePlayback() {
        if (resourceScope) resourceScope.releaseReservation()
        root.userPlayback = false
    }
    Component.onDestruction: root.releasePlayback()

    onSourceChanged: {
        root.userPlayback = false
        root.releasePlayback()
        root.primedVideoFrame = false
    }

    onIsVideoChanged: {
        if (!isVideo) {
            root.userPlayback = false
            root.releasePlayback()
            root.primedVideoFrame = false
        }
    }

    AmbientMediaBackdrop {
        anchors.fill: parent
        sourceItem: imagePreview.renderItem
        active: !root.minimalChrome && !root.fullQualityImage
                && root.isImage && imagePreview.ready && !root.failed
        spread: 11
        strength: Theme.dark ? 0.82 : 0.70
        blurStrength: 0.86
        saturation: 0.10
        sampleSize: 88
    }

    Superellipse {
        anchors.fill: parent
        fillColor: root.minimalChrome ? "transparent" : root.fillColor
        borderWidth: root.minimalChrome ? 0 : root.showBorder ? 1 : 0
        borderColor: Theme.borderMuted
        curveDepth: root.curveDepth
    }

    SuperellipseImage {
        id: imagePreview
        anchors.fill: parent
        // If a Qt/platform backend cannot fetch the full image directly,
        // retain the shared image-provider route as a safe fallback.
        visible: root.isImage && root.hasSource
                 && (!root.fullQualityImage || fullImage.status === Image.Error)
        source: visible ? root.source : ""
        mediaCategory: "photos"
        requestedSourceWidth: root.imageDecodeEdge
        requestedSourceHeight: root.imageDecodeEdge
        fillMode: root.fitImage ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        curveDepth: root.curveDepth
        borderWidth: 0
    }

    // Full-fidelity image for authored attachments: decode the canonical source, but bound the
    // display raster to the physical viewport. The uploaded bytes are unchanged.
    Image {
        id: fullImage
        anchors.fill: parent
        visible: root.fullQualityImage && root.isImage && root.hasSource
        source: visible ? root.source : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        readonly property int decodeEdge: Math.min(Qt.platform.os === "android" || Qt.platform.os === "ios" || Qt.platform.os === "wasm" ? 2048 : 4096,
            Math.max(256, Math.ceil(Math.max(width, height) * Screen.devicePixelRatio / 256) * 256))
        sourceSize: Qt.size(decodeEdge, decodeEdge)
        smooth: true
        mipmap: false
    }

    // A hidden/source-less Qt MediaPlayer still initializes native media
    // resources. In particular an image upload must never construct a video
    // sink or a QRhi on an Android multimedia worker thread.
    Loader {
        id: videoLoader
        objectName: "mediaPreviewVideoLoader"
        anchors.fill: parent
        anchors.margins: root.showBorder ? 1 : 0
        active: root.isVideo && root.hasSource && root.userPlayback && resourceScope.eligible
        visible: active && !root.failed
        onLoaded: {
            if (resourceScope.eligible && root.userPlayback && root.isVideo && root.hasSource && item
                    && resourceScope.handoffPlayback())
                item.resumePlayback()
            else
                releaseTimer.restart()
        }
        sourceComponent: GenyVideoPlayer {
            onResourceRevoked: releaseTimer.restart()
            source: root.source
            interactive: false
            showChrome: false
            fullscreenEnabled: false
            forceMuted: true
            eager: false
            primeFrame: false
            curveDepth: root.curveDepth
            cornerMaskColor: root.fillColor
        }
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
            if (!resourceScope.eligible) return
            if (!videoLoader.item) {
                releaseTimer.stop()
                if (!resourceScope.reservePlayback()) return
                // First tap constructs the player; onLoaded owns activation.
                root.primedVideoFrame = true
                root.userPlayback = true
                return
            }
            if (videoLoader.item.playing)
                videoLoader.item.pausePlayback()
            else
                videoLoader.item.resumePlayback()
        }
    }

    GenySpinner {
        anchors.centerIn: parent
        visible: root.isVideo && videoLoader.item !== null
                 && videoLoader.item.userActivated && videoLoader.item.loading
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
        visible: hover.hovered && root.interactive && !root.minimalChrome
        fillColor: "transparent"
        borderWidth: 1
        borderColor: Theme.accent
        curveDepth: root.curveDepth
        opacity: 0.32
    }
}
