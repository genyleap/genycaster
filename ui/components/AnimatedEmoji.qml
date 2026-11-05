import QtQuick

// The caller paints Unicode until the local renderer is ready. Keep the optional
// Lottie type behind a URL Loader so renderer-free installations remain usable.
Item {
    id: root

    property string unicodeSequence: ""
    property bool active: false
    property bool reducedMotion: false
    property bool cachedOnly: false
    // Keep the Noto Animated Emoji artwork resident even when motion policy
    // says "static". Picker cells use this so their resting appearance matches
    // the animated asset instead of falling back to the platform emoji font.
    property bool preserveArtwork: false
    // Picker delegates may retain an already-parsed VectorImage while pooled.
    // This never keeps animation running offscreen; it only avoids rebuilding
    // the renderer when a short reverse scroll brings the delegate back.
    property bool retainLoadedRenderer: false
    // Native Surface: Composer=0, Feed=1, CastDetail=2, PickerCell=3, Preview=4.
    property int surface: 1
    property int assetRevision: 0
    property bool requestFailed: false
    property bool rendererResident: false
    // Some lightweight/test QML contexts intentionally omit the optional emoji runtime.
    // Resolve it defensively so delayed bindings never throw ReferenceError.
    readonly property var controller: typeof emojiController !== "undefined" ? emojiController : null
    // Resolve the nearest scrolling surface once instead of mapping geometry
    // through the complete parent chain on every Flickable contentY update.
    // Existing artwork remains resident during a fling, while new Lottie loads
    // and animation playback wait until scrolling settles.
    property var scrollHost: null
    property var clipHost: null
    property bool insideViewport: true
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    readonly property bool motionActive: root.active && root.visible
                                         && root.insideViewport
                                         && !root.scrollMoving
                                         && Qt.application.state === Qt.ApplicationActive

    function resolveHosts() {
        let candidate = root.parent
        root.scrollHost = null
        root.clipHost = null
        while (candidate !== null) {
            if (root.clipHost === null && Boolean(candidate.clip))
                root.clipHost = candidate
            if (root.scrollHost === null
                    && candidate.moving !== undefined && candidate.contentY !== undefined)
                root.scrollHost = candidate
            if (root.clipHost !== null && root.scrollHost !== null)
                break
            candidate = candidate.parent
        }
    }

    function evaluateViewport() {
        if (!root.visible || root.width <= 0 || root.height <= 0) {
            root.insideViewport = false
            return
        }
        if (root.clipHost === null) {
            root.insideViewport = true
            return
        }
        const bounds = root.mapToItem(root.clipHost, Qt.rect(0, 0, root.width, root.height))
        root.insideViewport = bounds.x + bounds.width > 0
                           && bounds.y + bounds.height > 0
                           && bounds.x < root.clipHost.width
                           && bounds.y < root.clipHost.height
    }

    // Track-matte composition needs a shader-capable scene graph. Keep Unicode
    // intact on the software backend rather than showing incomplete silhouettes.
    readonly property bool rendererUsable: root.controller !== null && root.controller.lottieAvailable
        && root.GraphicsInfo.api !== GraphicsInfo.Unknown
        && root.GraphicsInfo.api !== GraphicsInfo.Software
        && root.GraphicsInfo.api !== GraphicsInfo.Null
    // Keep the configured loop count independent of transient focus, scroll
    // and viewport changes. Swapping an already-running infinite loop to a
    // one-shot during a fling makes VectorImage end on a transparent frame.
    readonly property int configuredLoops: root.controller !== null ? root.controller.playbackLoops(
        root.surface, root.reducedMotion, root.active, true) : 0
    readonly property int playbackLoops: root.motionActive ? root.configuredLoops : 0
    readonly property bool artworkWanted: root.rendererUsable
        && root.unicodeSequence.length > 0
        && root.visible && root.insideViewport
        // Keep the vector artwork visible even after the window loses focus.
        // motionActive alone decides whether its animation may run.
        && (!root.scrollMoving || root.rendererResident)
        && (root.playbackLoops !== 0 || root.preserveArtwork)
    readonly property string assetSource: {
        const revision = root.assetRevision
        return root.controller !== null && root.unicodeSequence.length > 0
            ? root.controller.assetSourceFor(root.unicodeSequence) : ""
    }
    readonly property bool animationReady: lottie.status === Loader.Ready
        && lottie.item !== null && lottie.item.renderReady
    readonly property bool rendererRetained: root.retainLoadedRenderer
        && root.rendererResident && root.assetSource.length > 0

    function requestIfNeeded() {
        if (root.artworkWanted && root.assetSource.length === 0) {
            if (root.controller === null)
                return
            if (root.cachedOnly)
                root.controller.requestCachedAsset(root.unicodeSequence)
            else
                root.controller.requestAsset(root.unicodeSequence)
        }
    }

    function syncRendererState() {
        if (lottie.item === null)
            return
        // A paused renderer still needs one animation iteration configured so
        // VectorImage materializes the authored first frame.
        lottie.item.playCount = root.configuredLoops === 0 ? 1 : root.configuredLoops
        lottie.item.animationActive = root.playbackLoops !== 0
    }

    Timer {
        id: requestTimer
        interval: 90
        repeat: false
        onTriggered: root.requestIfNeeded()
    }

    Timer {
        id: viewportTimer
        interval: 0
        repeat: false
        onTriggered: root.evaluateViewport()
    }

    onConfiguredLoopsChanged: root.syncRendererState()
    onPlaybackLoopsChanged: {
        requestTimer.restart()
        root.syncRendererState()
    }
    onAssetSourceChanged: {
        const nextSource = root.assetSource
        if (nextSource.length === 0)
            root.rendererResident = false
        if (lottie.item !== null)
            lottie.item.source = nextSource
    }
    onAnimationReadyChanged: {
        if (root.animationReady)
            root.rendererResident = true
    }
    onRendererUsableChanged: requestTimer.restart()
    onUnicodeSequenceChanged: {
        root.requestFailed = false
        root.rendererResident = false
        requestTimer.restart()
    }
    onPreserveArtworkChanged: requestTimer.restart()
    onVisibleChanged: {
        viewportTimer.restart()
        requestTimer.restart()
    }
    onXChanged: viewportTimer.restart()
    onYChanged: viewportTimer.restart()
    onWidthChanged: viewportTimer.restart()
    onHeightChanged: viewportTimer.restart()
    onInsideViewportChanged: requestTimer.restart()
    onScrollMovingChanged: {
        root.syncRendererState()
        if (!root.scrollMoving) {
            viewportTimer.restart()
            requestTimer.restart()
        }
    }
    Component.onCompleted: {
        root.resolveHosts()
        root.evaluateViewport()
        requestTimer.restart()
    }

    Connections {
        target: root.controller
        // Asset completion is global, but only the matching overlay may
        // invalidate its renderer. The previous fan-out made every completion
        // re-evaluate every visible emoji (quadratic churn in dense pickers).
        function onAssetReady(sequence, source) {
            if (String(sequence) === root.unicodeSequence) {
                root.requestFailed = false
                root.assetRevision += 1
            }
        }
        function onAssetFailed(sequence) {
            if (String(sequence) === root.unicodeSequence) {
                root.requestFailed = true
                root.assetRevision += 1
            }
        }
    }

    Loader {
        id: lottie
        anchors.fill: parent
        // Renderer creation is already gated to visible/settled delegates. Creating
        // this tiny wrapper synchronously avoids Qt Quick cancelling an asynchronous
        // incubation when a recycled feed delegate leaves the viewport.
        asynchronous: false
        active: (root.artworkWanted || root.rendererRetained)
                && root.assetSource.length > 0
        source: active ? Qt.resolvedUrl("AnimatedEmojiLottie.qml") : ""
        onLoaded: {
            lottie.item.source = root.assetSource
            root.syncRendererState()
        }
        onStatusChanged: {
            if (status === Loader.Null || status === Loader.Error)
                root.rendererResident = false
        }
    }
}
