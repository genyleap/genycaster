import QtQuick

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
    property string failureReason: ""
    property int retryCount: 0
    property bool explicitRetry: false
    property var spriteData: ({})
    property string requestedSequence: ""
    readonly property var controller: typeof emojiController !== "undefined" ? emojiController : null
    readonly property bool rendererUsable: root.controller !== null && root.controller.spriteAvailable
        && root.GraphicsInfo.api !== GraphicsInfo.Unknown
        && root.GraphicsInfo.api !== GraphicsInfo.Software
        && root.GraphicsInfo.api !== GraphicsInfo.Null
    // Download/cache admission must not depend on the scene graph already
    // being initialized. GraphicsInfo gates presentation, not asset preparation.
    readonly property bool artworkWanted: root.controller !== null && root.controller.spriteAvailable && root.visible && root.viewportVisible
        && root.width > 0 && root.height > 0 && root.unicodeSequence.length > 0
        && Qt.application.state === Qt.ApplicationActive
        && (root.active || root.preserveArtwork)
    readonly property bool animationReady: !root.requestFailed && atlas.status === Image.Ready
        && spriteLoader.item !== null
    readonly property bool loading: root.artworkWanted && !root.animationReady && !root.requestFailed
    readonly property int playbackLoops: root.reducedMotion || !root.active ? 0
        : root.controller !== null ? root.controller.playbackLoops(root.surface, false, true, root.artworkWanted) : 0

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
        root.requestFailed = false
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

    // Warm the exact URL at its natural size on Qt's image-loading thread.
    Image {
        id: atlas
        visible: false
        asynchronous: true
        cache: true
        source: root.artworkWanted || root.retainLoadedRenderer
            ? String(root.spriteData.source || "") : ""
        onStatusChanged: if (status === Image.Error) {
            // console.warn("Genycaster emoji", root.unicodeSequence, "image: prepared atlas could not be loaded")
            root.fail("image: prepared atlas could not be loaded")
        }
    }

    Loader {
        id: spriteLoader
        anchors.fill: parent
        active: root.rendererUsable && atlas.status === Image.Ready
                && (root.artworkWanted || root.retainLoadedRenderer)
        onStatusChanged: if (status === Loader.Error) {
            // console.warn("Genycaster emoji", root.unicodeSequence, "renderer: sprite component failed")
            root.fail("renderer: sprite component failed")
        }
        sourceComponent: AnimatedSprite {
            source: atlas.source
            frameWidth: Number(root.spriteData.frameSize || 96)
            frameHeight: frameWidth
            frameCount: Number(root.spriteData.frameCount || 1)
            frameDuration: Number(root.spriteData.frameDuration || 40)
            interpolate: false
            smooth: true
            loops: root.playbackLoops < 0 ? AnimatedSprite.Infinite : 1
            // AnimatedSprite internally calls setPaused on a NumberAnimation;
            // pausing before the animation has started warns on Qt 6.12.
            // Run only when playback is wanted instead of pausing an idle sprite.
            running: root.playbackLoops !== 0 && root.artworkWanted
            finishBehavior: AnimatedSprite.FinishAtInitialFrame
            // Pause/resume preserves completed loops; no restart on scrolling.
        }
    }
}
