import QtQuick
import Farcaster.Client

// Worker-prepared PNG sheets share Qt's image cache across picker delegates.
Item {
    id: root
    property string unicodeSequence: ""
    property bool active: false
    property bool reducedMotion: false
    property bool preserveArtwork: false
    property bool retainLoadedRenderer: false
    property int surface: 3
    property bool viewportVisible: true
    property bool requestFailed: false
    // An authored one-shot may terminate on a fully transparent frame.
    // Return the inline Unicode/Qt color emoji when it finishes rather than
    // continuing to mask the text with an empty sprite.
    property bool playbackFinished: false
    property string failureReason: ""
    property int retryCount: 0
    property bool explicitRetry: false
    property var spriteData: ({})
    property string requestedSequence: ""
    property real framePosition: 0
    readonly property var controller: typeof emojiController !== "undefined" ? emojiController : null
    readonly property bool rendererUsable: root.controller !== null && root.controller.spriteAvailable
        && root.GraphicsInfo.api !== GraphicsInfo.Unknown
        && root.GraphicsInfo.api !== GraphicsInfo.Software
        && root.GraphicsInfo.api !== GraphicsInfo.Null
    // Download/cache admission must not depend on the scene graph already
    // being initialized. GraphicsInfo gates presentation, not asset preparation.
    readonly property bool artworkWanted: root.controller !== null && root.controller.spriteAvailable && root.visible && root.viewportVisible
        && root.width > 0 && root.height > 0 && root.unicodeSequence.length > 0
        // Deactivation pauses playback, but must not release the painted frame.
        && (root.active || root.preserveArtwork)
    readonly property bool animationReady: !root.requestFailed && !root.playbackFinished
        && root.rendererUsable && atlas.status === Image.Ready
    readonly property bool loading: root.artworkWanted && !root.animationReady
        && !root.requestFailed && !root.playbackFinished
    // Configure the lifetime of a playback independently from pause/resume.
    readonly property int configuredLoops: root.controller !== null
        ? root.controller.playbackLoops(root.surface, false, true, true) : 0
    readonly property bool motionActive: root.active && !root.reducedMotion
        && root.visible && root.viewportVisible
        && Qt.application.state === Qt.ApplicationActive
    readonly property int frameSize: Math.max(1, Number(root.spriteData.frameSize || 96))
    readonly property int frameCount: Math.max(1, Number(root.spriteData.frameCount || 1))
    readonly property int frameDuration: Math.max(1, Number(root.spriteData.frameDuration || 40))
    readonly property int frameIndex: Math.min(root.frameCount - 1, Math.floor(root.framePosition))

    function syncInterest() {
        const wantedSequence = root.artworkWanted ? root.unicodeSequence : ""
        if (!root.artworkWanted) {
            admission.stop()
            retryDelay.stop()
        }
        if (wantedSequence === root.requestedSequence)
            return
        if (root.requestedSequence.length > 0 && root.controller !== null)
            root.controller.releaseSprite(root.requestedSequence)
        root.requestedSequence = ""
        admission.stop()
        if (wantedSequence.length > 0) {
            root.requestFailed = false
            root.failureReason = ""
            const cached = root.controller.spriteFor(wantedSequence)
            if (cached.source)
                root.spriteData = cached
            admission.start()
        }
    }

    function retry() {
        root.retryCount = 0
        root.restartRequest(true)
    }

    function restartRequest(explicit) {
        if (!root.artworkWanted)
            return
        retryDelay.stop()
        if (root.requestedSequence.length > 0 && root.controller !== null)
            root.controller.releaseSprite(root.requestedSequence)
        root.requestedSequence = ""
        root.spriteData = ({})
        root.requestFailed = false
        root.playbackFinished = false
        root.failureReason = ""
        root.explicitRetry = explicit
        admission.restart()
    }

    function fail(reason) {
        root.requestFailed = true
        root.failureReason = String(reason || "atlas: preparation failed")
        if (root.artworkWanted && root.retryCount < 2 && !retryDelay.running)
            retryDelay.start()
    }

    // Two automatic retries per sequence, after the native failure cooldown.
    // No hot failure loop and no timer per successfully loaded cell.
    Timer {
        id: retryDelay
        interval: 31000
        onTriggered: {
            if (root.artworkWanted && root.requestFailed) {
                root.retryCount += 1
                root.restartRequest(false)
            }
        }
    }

    // Coalesce fast category/viewport changes before acquiring work. Admission
    // backpressure retries here; actual failures use the slow bounded timer.
    Timer {
        id: admission
        interval: 150
        repeat: false
        onTriggered: {
            if (!root.artworkWanted || root.requestedSequence.length > 0)
                return
            // A retained/Qt-cached sheet needs neither a disk digest probe nor
            // another native request when a short scroll reveals it again.
            if (atlas.status === Image.Ready && !root.requestFailed)
                return
            root.requestedSequence = root.unicodeSequence
            if (!root.controller.requestSprite(root.requestedSequence, root.explicitRetry)) {
                root.requestedSequence = ""
                admission.restart()
            } else {
                root.explicitRetry = false
            }
        }
    }

    onArtworkWantedChanged: Qt.callLater(root.syncInterest)
    onUnicodeSequenceChanged: {
        root.spriteData = ({})
        root.framePosition = 0
        root.requestFailed = false
        root.playbackFinished = false
        root.failureReason = ""
        root.retryCount = 0
        root.explicitRetry = false
        retryDelay.stop()
        Qt.callLater(root.syncInterest)
    }
    Component.onCompleted: root.syncInterest()
    Component.onDestruction: {
        if (root.requestedSequence.length > 0 && root.controller !== null)
            root.controller.releaseSprite(root.requestedSequence)
    }

    Connections {
        target: root.controller
        function onSpriteReady(sequence, metadata) {
            if (sequence !== root.unicodeSequence || !root.artworkWanted)
                return
            // A memory entry can outlive disk-cache eviction. Clear an errored
            // URL before restoring a newly prepared file at the same path.
            if (atlas.status === Image.Error)
                root.spriteData = ({})
            Qt.callLater(() => {
                if (sequence === root.unicodeSequence && root.artworkWanted) {
                    root.requestFailed = false
                    root.failureReason = ""
                    retryDelay.stop()
                    root.spriteData = metadata
                }
            })
        }
        function onSpriteFailed(sequence, reason) {
            if (sequence === root.requestedSequence && !root.animationReady) {
                root.spriteData = ({})
                root.fail(reason)
            }
        }
    }

    // Paint the same Image whose readiness we observe. A hidden preload Image
    // cannot establish that AnimatedSprite has assembled/uploaded its texture.
    // Move the cached sheet under a fixed clip: no per-frame decode, source
    // changes, delegate recreation or stopped multi-row sprite UV calculation.
    Item {
        id: frameViewport
        anchors.fill: parent
        clip: true
        opacity: root.animationReady ? 1 : 0
        Image {
            id: atlas
            readonly property int columns: Math.max(1, Math.floor(sourceSize.width / root.frameSize))
            width: frameViewport.width * sourceSize.width / root.frameSize
            height: frameViewport.height * sourceSize.height / root.frameSize
            x: -(root.frameIndex % columns) * frameViewport.width
            y: -Math.floor(root.frameIndex / columns) * frameViewport.height
            asynchronous: true
            cache: true
            smooth: true
            source: root.artworkWanted || root.retainLoadedRenderer
                ? String(root.spriteData.source || "") : ""
            onStatusChanged: {
                if (status === Image.Error)
                    root.fail("image: prepared atlas could not be loaded")
            }
        }
    }

    NumberAnimation {
        id: spritePlayback
        target: root
        property: "framePosition"
        from: 0
        to: root.frameCount
        duration: root.frameCount * root.frameDuration
        loops: root.configuredLoops < 0 ? Animation.Infinite : 1
        running: root.rendererUsable && atlas.status === Image.Ready && !root.playbackFinished
        paused: running && !root.motionActive
        onFinished: root.playbackFinished = true
    }
}
