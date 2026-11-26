pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Feed-oriented video presentation. Playback sessions, transport controls and
// fullscreen live in GenyVideoPlayer (the single canonical media stack). This
// wrapper owns stable feed geometry and a lightweight poster until Play.
Item {
    id: root

    property var embed: ({})
    property bool compact: false
    // Exposed for deterministic silent offscreen playback tests. Audio is
    // enabled for every production feed delegate by default.
    property bool audioOutputEnabled: true
    property bool playbackRequested: false
    property bool browserPlayback: false
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
    property real reservedMediaAspect: 16 / 9
    readonly property bool playerReady: playerLoader.status === Loader.Ready
    readonly property bool hasSource: root.targetUrl.length > 0
                                      || root.adaptiveSource.length > 0
                                      || root.videoRenditions.length > 0
    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    readonly property string adaptiveSource: String(root.embed.adaptiveUrl
                                                     || root.embed.hlsUrl
                                                     || root.embed.manifestUrl || "")
    readonly property var videoRenditions: root.embed.renditions
                                            || root.embed.videoRenditions
                                            || root.embed.variants || []
    readonly property string contentType: String(root.embed.contentType
                                                 || root.embed.mediaType || "")
    // The read-model already derives Cloudflare Stream thumbnails. Keep
    // alternate embed poster field names for feeds from other providers.
    readonly property string thumbnailSource: String(root.embed.imageSource
                                                      || root.embed.thumbnail
                                                      || root.embed.previewImage
                                                      || root.embed.thumbnailUrl
                                                      || root.embed.posterUrl
                                                      || root.embed.poster
                                                      || root.embed.coverImage || "")
    readonly property string mediaLabel: String(root.embed.fileName
                                                || root.embed.host
                                                || root.embed.domain || "")
    readonly property real mediaAspect: {
        const ratio = Number(root.embed.aspectRatio || 0)
        if (ratio > 0)
            return ratio
        const width = Number(root.embed.width || 0)
        const height = Number(root.embed.height || 0)
        return width > 0 && height > 0 ? width / height : 0
    }

    signal openRequested(string url)

    function releasePlayback() {
        if (root.browserPlayback && resourceScope) {
            root.browserPlayback = false
            const session = resourceScope.coordinator()
            if (session) session.closeBrowserHls()
        }
        if (resourceScope) resourceScope.releaseReservation()
        root.playbackRequested = false
    }
    Component.onDestruction: root.releasePlayback()

    function requestPlayback() {
        if (!root.hasSource || root.playbackRequested || !resourceScope.eligible) return
        releaseTimer.stop()
        if (!resourceScope.reservePlayback()) return
        const candidate = root.adaptiveSource.length > 0 ? root.adaptiveSource : root.targetUrl
        // Browser Qt Multimedia has inconsistent adaptive HLS support.
        // Use the browser's native HLS support or local hls.js/MSE instead.
        if (Qt.platform.os === 'wasm' && /\.m3u8(?:[?#]|$)/i.test(candidate)) {
            if (typeof mediaPlayback !== 'undefined'
                    && mediaPlayback !== null
                    && mediaPlayback.openBrowserHls(candidate)) {
                root.browserPlayback = true
                return
            }
            resourceScope.releaseReservation()
            root.openRequested(candidate)
            return
        }
        root.playbackRequested = true
    }

    onVisibleChanged: {
        if (!root.visible)
            root.releasePlayback()
    }

    // A reused delegate must neither finish activating nor retain the previous
    // Cast's media session. Loader owns and releases the complete player tree.
    onTargetUrlChanged: {
        root.playbackRequested = false
        root.releasePlayback()
        aspectReservationTimer.restart()
    }
    onAdaptiveSourceChanged: {
        root.playbackRequested = false
        root.releasePlayback()
        aspectReservationTimer.restart()
    }

    function reserveMediaAspect() {
        root.reservedMediaAspect = root.mediaAspect > 0 ? root.mediaAspect : 16 / 9
    }
    Component.onCompleted: root.reserveMediaAspect()
    Timer {
        id: aspectReservationTimer
        interval: 0
        onTriggered: root.reserveMediaAspect()
    }

    function boundedHeight(rawHeight, minimumHeight, maximumHeight) {
        return Math.max(minimumHeight, Math.min(maximumHeight, rawHeight))
    }

    // Layout stays stable when metadata arrives later: declared media
    // dimensions, then a 16:9 fallback. Decoded video pixels never reflow the
    // feed delegate because geometry is owned here, not by the video sink.
    readonly property real preferredMediaHeight: {
        const available = Math.max(1, root.width)
        const maximum = root.compact ? 420 : 520
        const minimum = root.compact ? 180 : 240
        return root.boundedHeight(available / root.reservedMediaAspect, minimum, maximum)
    }
    implicitHeight: root.preferredMediaHeight

    AmbientMediaBackdrop {
        anchors.fill: parent
        sourceItem: poster.renderItem
        active: poster.ready
        spread: 13
        strength: Theme.dark ? 0.82 : 0.70
        blurStrength: 1.0
        saturation: 0.0
        sampleSize: 104
    }

    // Browsing a video row creates only a poster and its two affordances.
    // Even a source-less MediaPlayer initializes multimedia resources, so the
    // canonical player must not be constructed by a scrolling feed delegate.
    Button {
        id: previewControl
        objectName: "feedVideoPreviewControl"
        anchors.fill: parent
        visible: !root.playerReady
        enabled: root.hasSource && !root.playbackRequested
        padding: 0
        hoverEnabled: !poster.scrollMoving
        text: qsTrId("genycaster.player.play")
        Accessible.name: text
        onClicked: root.requestPlayback()
        Keys.onReturnPressed: event => { root.requestPlayback(); event.accepted = true }
        Keys.onEnterPressed: event => { root.requestPlayback(); event.accepted = true }

        background: SuperellipseImage {
            id: poster
            source: root.thumbnailSource
            mediaCategory: "video-previews"
            fillMode: Image.PreserveAspectFit
            fillColor: Theme.mediaPlayerBackground
            curveDepth: Theme.shapeMedia
            borderWidth: 0
        }

        contentItem: Item {
            Superellipse {
                id: playBadge
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height,
                                parent.width < 400 || parent.height < 260
                                ? Theme.mediaPlayerActionHeight : 64)
                height: width
                fillColor: previewControl.hovered || previewControl.down
                           ? Theme.mediaPlayerActionHover : Theme.mediaPlayerAction
                borderWidth: Theme.keyboardFocusVisible(previewControl) ? Theme.strokeEmphasis : 0
                borderColor: Theme.accent
                curveDepth: Theme.curveControl

                Icon {
                    anchors.centerIn: parent
                    visible: !root.playbackRequested
                    text: Theme.iconPlay
                    color: Theme.mediaPlayerInk
                    preferredSize: Math.min(32, playBadge.width * 0.5)
                }

                GenySpinner {
                    anchors.centerIn: parent
                    visible: root.playbackRequested
                    running: visible
                    Accessible.ignored: true
                }
            }

            DirectionAwareLabel {
                anchors.top: playBadge.bottom
                anchors.topMargin: Theme.space2
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(260, Math.max(1, parent.width - Theme.space3 * 2))
                text: qsTrId("genycaster.player.play")
                color: Theme.mediaPlayerInk
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                style: Text.Outline
                styleColor: Theme.mediaPlayerBackground
                Accessible.ignored: true
            }
        }
    }

    Button {
        id: openOriginalControl
        anchors.top: parent.top
        anchors.topMargin: Theme.space3
        anchors.right: Theme.rtl ? undefined : parent.right
        anchors.left: Theme.rtl ? parent.left : undefined
        anchors.rightMargin: Theme.space3
        anchors.leftMargin: Theme.space3
        width: Theme.mediaPlayerControlSize
        height: width
        padding: 0
        visible: !root.playerReady
        enabled: root.targetUrl.length > 0
        hoverEnabled: !poster.scrollMoving
        text: qsTrId("genycaster.player.open_original")
        Accessible.name: text
        onClicked: root.openRequested(root.targetUrl)
        Keys.onReturnPressed: event => { root.openRequested(root.targetUrl); event.accepted = true }
        Keys.onEnterPressed: event => { root.openRequested(root.targetUrl); event.accepted = true }

        background: Superellipse {
            fillColor: openOriginalControl.hovered
                       ? Theme.mediaPlayerChromeHover : Theme.mediaPlayerChrome
            borderWidth: Theme.keyboardFocusVisible(openOriginalControl) ? Theme.strokeEmphasis : 0
            borderColor: Theme.accent
            curveDepth: width / 2
        }
        contentItem: Icon {
            text: Theme.iconArrowUpRight
            color: Theme.mediaPlayerInk
            preferredSize: Theme.mediaPlayerControlIconSize
        }
    }

    Loader {
        id: playerLoader
        objectName: "feedVideoPlayerLoader"
        anchors.fill: parent
        active: root.playbackRequested && resourceScope.eligible
        asynchronous: true
        visible: root.playerReady
        onLoaded: {
            const loadedPlayer = playerLoader.item as GenyVideoPlayer
            if (loadedPlayer && root.playbackRequested && resourceScope.eligible && resourceScope.handoffPlayback())
                loadedPlayer.resumePlayback()
            else
                releaseTimer.restart()
        }

        sourceComponent: Component {
            GenyVideoPlayer {
                onResourceRevoked: releaseTimer.restart()
                source: root.targetUrl
                adaptiveSource: root.adaptiveSource
                renditions: root.videoRenditions
                contentType: root.contentType
                posterSource: root.thumbnailSource
                primeFrame: false
                eager: false
                forceMuted: false
                audioOutputEnabled: root.audioOutputEnabled
                mediaLabel: root.mediaLabel
                compact: root.compact
                fullscreenEnabled: true
                openOriginalEnabled: true
                openOriginalUrl: root.targetUrl
                curveDepth: Theme.shapeMedia
                cornerMaskColor: Theme.background
                onOpenOriginalRequested: url => root.openRequested(url)
            }
        }
    }
}
