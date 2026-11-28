import QtQuick
import QtQuick.Window
import QtQuick.Effects
import Farcaster.Client

/**
 * Shared remote/local image surface with superellipse clipping, bounded decode
 * sizing and stable loading/error states.
 */
Item {
    id: root

    property url source: ""
    property int fillMode: Image.PreserveAspectCrop
    property real curveDepth: Theme.curveContainer
    property real exponent: Theme.superellipseExponent
    property color fillColor: Theme.surfaceMuted
    property color borderColor: Theme.borderMuted
    property real borderWidth: 0
    property bool asynchronous: true
    property bool cache: true
    property int requestedSourceWidth: 0
    property int requestedSourceHeight: 0
    property int decodeQuantum: 128
    property bool showLoadingIndicator: true
    // Large feed artwork can opt into a cheap image-node opacity reveal.
    // Avatars/thumbnails retain their immediate path by default.
    property bool smoothReveal: false
    // Keep decoded pixels behind the placeholder until the parent has committed
    // the final layout. Image.Ready alone does not guarantee stable geometry.
    property bool presentationEnabled: true
    property bool showFailureIndicator: true
    property bool showManualLoadIndicator: true
    property bool retainPreviousWhileLoading: false
    property string mediaCategory: "other"
    property bool manualLoadRequested: false
    // Presentation-critical artwork (notably Mini App discovery images/icons)
    // can opt into an unconditional fetch without changing the global data policy.
    property bool forceAutomaticDownload: false
    // Large cover surfaces can opt out of the scene-graph mask pass. Qt Quick's
    // MultiEffect mask is reliable for ordinary thumbnails/avatars, but very
    // wide textures can be reported Ready while the effect layer itself paints
    // no pixels on some GPU/backend combinations. Keeping this explicit lets
    // cover media prefer a guaranteed plain-image path over a decorative clip.
    property bool clipToSuperellipse: true
    // Visible media must make progress during continuous scrolling. Network/
    // decode remain asynchronous and texture admission remains frame-paced.
    // Specialized surfaces may defer NEW requests, never detach in-flight ones.
    property bool deferNewLoadsWhileScrolling: false
    // Gallery siblings use a small per-index delay so cache hits and texture
    // creation are admitted over separate frames instead of one render burst.
    property int loadDelayMs: 0
    property var scrollHost: null
    property bool allowAnimation: true
    property url activeSource: ""
    property bool componentReady: false
    property bool presentationReady: false
    // Large decoded surfaces wait for the shared window-frame coordinator before
    // becoming visible. Small avatars/icons remain immediate.
    property bool framePacedPresentation: true
    property bool presentationAdmissionPending: false
    property int presentationGeneration: 0
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    readonly property bool remoteSource: /^image:\/\/remote\//i.test(root.source.toString())
    readonly property bool remoteRaster: root.remoteSource
        || /^https?:\/\//i.test(root.source.toString())
    readonly property bool browserRemoteRaster: Qt.platform.os === "wasm" && root.remoteRaster
    // Keep the decode/texture working set tied to the visible page, including
    // pooled ListView delegates and retained shell routes. Local assets and
    // authored attachment previews keep their original behavior.
    MediaResourceScope {
        id: rasterScope
        owner: root
        observeViewport: root.remoteRaster
        viewportMargin: Qt.platform.os === "wasm" ? 320 : 480
        // One shared viewport sample, two hysteresis boundaries.
        retentionMargin: Qt.platform.os === "wasm" ? 640 : 960
        requireOwnerVisible: false
    }
    readonly property bool rasterEligible: !root.remoteRaster || rasterScope.eligible
    readonly property bool rasterRetained: !root.remoteRaster || rasterScope.retained
    readonly property bool rasterPageVisible: root.parent !== null && root.parent.visible
        && rasterScope.foreground
    onRasterEligibleChanged: {
        if (!componentReady)
            return
        if (rasterEligible) {
            rasterReleaseTimer.stop()
            // A retained request/texture already has the correct identity.
            if (root.activeSource.toString().length === 0)
                sourceStartTimer.restart()
            root.synchronizePresentation()
        } else {
            if (root.activeSource.toString().length === 0)
                sourceStartTimer.stop()
            // An unpresented remote surface outside admission no longer
            // competes for a frame ticket. Keep decoded pixels resident.
            if (root.presentationAdmissionPending)
                root.invalidatePresentationAdmission()
        }
    }
    onRasterRetainedChanged: {
        if (!componentReady) return
        if (rasterRetained)
            rasterReleaseTimer.stop()
        else if (root.activeSource.toString().length > 0)
            rasterReleaseTimer.restart()
    }
    onRasterPageVisibleChanged: {
        if (componentReady && root.remoteRaster && !rasterPageVisible
                && root.activeSource.toString().length > 0)
            rasterReleaseTimer.restart()
    }
    Timer {
        id: rasterReleaseTimer
        interval: root.rasterPageVisible ? 1100 : 250
        onTriggered: {
            if (!root.rasterRetained) {
                root.activeSource = ""
                root.invalidatePresentationAdmission()
            }
        }
    }
    // Keep intrinsic dimensions across viewport eviction, but never across
    // a source identity change. Layout must not jump as pixels are released.
    // Publish both intrinsic dimensions in a single transaction. A decode
    // upgrade can round pixels differently; it is not new source metadata.
    property size rememberedIntrinsicSize: Qt.size(0, 0)
    readonly property real rememberedIntrinsicWidth: rememberedIntrinsicSize.width
    readonly property real rememberedIntrinsicHeight: rememberedIntrinsicSize.height
    // AnimatedImage cannot consume a QQuickImageProvider URL. Decode the
    // original canonical URL and use the bounded HTTPS image gateway on WASM.
    // Unsupported sources still keep the ordinary one-frame image fallback.
    // The legacy string base64 decoder is deprecated in Qt 6.11 and ran on every
    // gallery delegate (including non-GIF avatars). Decode base64url bytes
    // in plain QML JavaScript only when we actually need the URL.
    function decodeBase64Url(input) {
        const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
        if (input.length === 0 || input.length > 4096) return ""
        let accumulator = 0
        let bits = 0
        let bytes = ""
        for (let index = 0; index < input.length; ++index) {
            if (input[index] === "=") break
            const value = alphabet.indexOf(input[index])
            if (value < 0) return ""
            accumulator = ((accumulator << 6) | value) & 0xffffff
            bits += 6
            if (bits >= 8) {
                bits -= 8
                bytes += "%" + ((accumulator >> bits) & 255).toString(16).padStart(2, "0")
            }
        }
        try { return decodeURIComponent(bytes) }
        catch (error) { return "" }
    }

    readonly property string animatedGifUrl: {
        const input = root.source.toString()
        let mediaUrl = input
        if (/^image:\/\/remote\//i.test(input)) {
            let encoded = input.substring('image://remote/'.length)
            if (encoded.indexOf('v22/') === 0) {
                const parts = encoded.split('/')
                encoded = parts.slice(3).join('/')
            }
            mediaUrl = root.decodeBase64Url(encoded)
            if (!mediaUrl.length) return ''
        }
        if (!/^https:\/\/[^\s?#]+\.gif(?:\?[^#]*)?$/i.test(mediaUrl))
            return ''
        // Tenor's media1 CDN answers GET with valid GIF bytes but sometimes
        // rejects the request style used by Qt's QMovie/AnimatedImage stack.
        // The bounded, same-origin-capable first-party image proxy pins DNS,
        // verifies GIF magic, and never relies on a HEAD request. Route the
        // known problematic CDN through it on desktop as well as WASM.
        const useGateway = Qt.platform.os === 'wasm'
            || /^https:\/\/media1\.tenor\.com\/m\//i.test(mediaUrl)
        return useGateway
            ? (deploymentEndpoints.browserGateway + "/v1/browser/images?url=")
                + encodeURIComponent(mediaUrl)
            : mediaUrl
    }
    readonly property bool automaticDownloadAllowed: !root.remoteSource
                                                    || root.manualLoadRequested
                                                    || root.forceAutomaticDownload
                                                    || dataStorage.autoDownloadAllowed(root.mediaCategory)
    // Automatic policy blocks only the network leg. The provider is still
    // allowed to satisfy the request from RAM/disk cache first. Keep that cache
    // lookup visually indistinguishable from an ordinary image load and show
    // Tap-to-load only after the automatic request has confirmed a cache miss.
    readonly property bool automaticPolicyBlocked: root.remoteSource
                                                   && !root.manualLoadRequested
                                                   && !root.forceAutomaticDownload
                                                   && !dataStorage.autoDownloadAllowed(root.mediaCategory)
    readonly property bool manualLoadRequired: root.automaticPolicyBlocked && root.loadFailed
    readonly property bool policyBlocked: root.automaticPolicyBlocked && !root.ready
    readonly property string effectiveSource: root.remoteSource
                                              ? dataStorage.remoteImageSource(
                                                    root.source.toString(), root.mediaCategory,
                                                    root.manualLoadRequested || root.forceAutomaticDownload)
                                              : root.source.toString()
    // An empty activeSource can last beyond the moving signal while the
    // post-fling admission timer is running. Treat that whole interval as one
    // loading state so the placeholder cannot disappear for a frame.
    readonly property bool waitingForAdmission: root.source.toString().length > 0
                                                && root.effectiveSource.length > 0
                                                && root.activeSource.toString().length === 0

    readonly property int status: imageSource.status
    readonly property real progress: imageSource.progress
    readonly property real sourceImplicitWidth: rememberedIntrinsicWidth
    readonly property real sourceImplicitHeight: rememberedIntrinsicHeight
    readonly property real intrinsicAspect: rememberedIntrinsicSize.width > 0
        && rememberedIntrinsicSize.height > 0
        ? rememberedIntrinsicSize.width / rememberedIntrinsicSize.height : 0
    readonly property Item renderItem: directRaster.item !== null && root.directMask ? directRaster.item : imageSource
    // Keep the fitted/cropped rectangle stable while Image swaps textures.
    // Do not use intermediate paintedWidth/Height from the loading decoder.
    readonly property real paintedWidth: intrinsicAspect > 0
        ? (fillMode === Image.PreserveAspectFit ? Math.min(width, height * intrinsicAspect)
           : fillMode === Image.PreserveAspectCrop ? Math.max(width, height * intrinsicAspect) : width)
        : imageSource.paintedWidth
    readonly property real paintedHeight: intrinsicAspect > 0
        ? (fillMode === Image.PreserveAspectFit ? Math.min(height, width / intrinsicAspect)
           : fillMode === Image.PreserveAspectCrop ? Math.max(height, width / intrinsicAspect) : height)
        : imageSource.paintedHeight
    readonly property real paintedX: Math.max(0, (root.width - root.paintedWidth) / 2)
    readonly property real paintedY: Math.max(0, (root.height - root.paintedHeight) / 2)
    readonly property real paintedAspectRatio: root.paintedHeight > 1
        ? root.paintedWidth / root.paintedHeight : 0
    readonly property rect visibleContentRect: root.fillMode === Image.PreserveAspectFit
                                                 ? Qt.rect(root.paintedX, root.paintedY,
                                                           Math.max(1, root.paintedWidth),
                                                           Math.max(1, root.paintedHeight))
                                                 : Qt.rect(0, 0, Math.max(1, root.width), Math.max(1, root.height))
    readonly property bool providerFallback: imageSource.status === Image.Ready
                                             && imageSource.implicitWidth <= 1
                                             && imageSource.implicitHeight <= 1
    readonly property bool loadFailed: imageSource.status === Image.Error || providerFallback
    readonly property bool loading: root.source.toString().length > 0
                                    && (imageSource.status === Image.Loading || root.waitingForAdmission
                                        || (root.decodedReady && !root.ready))
    readonly property bool decodedReady: imageSource.status === Image.Ready && !root.providerFallback
    // Keep the already presented texture for the SAME image during a
    // resolution upgrade. Identity changes always invalidate it immediately.
    property bool sizeReloadInFlight: false
    readonly property bool retainingSizedTexture: root.sizeReloadInFlight
        && root.presentationReady && imageSource.status !== Image.Error
        && imageSource.status !== Image.Ready
    readonly property bool ready: (root.decodedReady || root.retainingSizedTexture)
                                  && root.presentationReady && root.presentationEnabled
    readonly property bool hasRenderableGeometry: root.width >= 1 && root.height >= 1
    readonly property bool requiresFramePacedPresentation: root.framePacedPresentation
                                                            && root.width * root.height >= 160 * 160
    readonly property bool gpuRasterAvailable: GraphicsInfo.api !== GraphicsInfo.Unknown
        && GraphicsInfo.api !== GraphicsInfo.Software && GraphicsInfo.api !== GraphicsInfo.Null
    // The Qt Image remains the sole texture provider; sampling the ready
    // texture directly avoids two full-size render targets per clipped photo.
    property bool directRasterFailed: false
    readonly property bool directMask: root.clipToSuperellipse
        && root.gpuRasterAvailable && !root.directRasterFailed
    readonly property bool stillVisible: root.ready
        || (root.retainPreviousWhileLoading && imageSource.status === Image.Loading)
    readonly property bool maskActive: root.visible && root.clipToSuperellipse
        && root.stillVisible && root.hasRenderableGeometry
    readonly property bool animatedMaskActive: gifLoader.item !== null
        && gifLoader.item.visible && root.maskActive
    readonly property bool legacyMaskActive: (root.maskActive && !root.directMask)
        || root.animatedMaskActive
    readonly property size desiredDecodeSize: Qt.size(
        root.quantizedDecodeDimension(root.requestedSourceWidth, root.width),
        root.quantizedDecodeDimension(root.requestedSourceHeight, root.height))
    // Decode size is NOT viewport geometry. During resize the GPU scales the
    // existing texture. Commit both dimensions atomically after it settles,
    // and only if there is a meaningful resolution change.
    property size stableDecodeSize: Qt.size(0, 0)
    readonly property int stableSourceWidth: stableDecodeSize.width
    readonly property int stableSourceHeight: stableDecodeSize.height
    function commitDecodeSize(initial) {
        if (typeof renderingController !== "undefined" && renderingController
                && renderingController.shuttingDown === true)
            return
        if (!root.hasRenderableGeometry)
            return
        if (!initial && (imageSource.status === Image.Loading
                || (root.remoteRaster && (root.scrollMoving || !root.rasterRetained))))
            return
        const next = root.desiredDecodeSize
        const previous = root.stableDecodeSize
        if (!initial && previous.width > 0 && previous.height > 0) {
            const grows = next.width > previous.width * 1.20
                       || next.height > previous.height * 1.20
            // A resident texture is not downgraded on shrink. Eviction
            // or a new media identity resets the allocation to its new size.
            if (!grows)
                return
        }
        if (next.width === previous.width && next.height === previous.height)
            return
        root.sizeReloadInFlight = root.decodedReady && root.presentationReady
        root.stableDecodeSize = next
    }
    onDesiredDecodeSizeChanged: {
        if (root.componentReady)
            decodeSizeSettler.restart()
    }
    onWidthChanged: {
        if (root.componentReady && root.desiredDecodeSize.width !== root.stableSourceWidth)
            decodeSizeSettler.restart()
    }
    onHeightChanged: {
        if (root.componentReady && root.desiredDecodeSize.height !== root.stableSourceHeight)
            decodeSizeSettler.restart()
    }
    onHasRenderableGeometryChanged: {
        if (root.componentReady && root.hasRenderableGeometry
                && root.activeSource.toString().length === 0) {
            root.commitDecodeSize(true)
            sourceStartTimer.restart()
        }
    }
    Timer {
        id: decodeSizeSettler
        interval: 200
        repeat: false
        onTriggered: root.commitDecodeSize(false)
    }

    function resolveScrollHost() {
        let candidate = root.parent
        while (candidate !== null) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined) {
                root.scrollHost = candidate
                return
            }
            candidate = candidate.parent
        }
        root.scrollHost = null
    }

    function synchronizeActiveSource() {
        if (typeof renderingController !== "undefined" && renderingController
                && renderingController.shuttingDown === true)
            return
        if (!root.componentReady || !root.rasterEligible)
            return
        if (!root.hasRenderableGeometry && root.activeSource.toString().length === 0)
            return
        const requested = root.effectiveSource
        const current = root.activeSource.toString()
        if (requested.length === 0) {
            root.activeSource = ""
            return
        }

        // Never let a recycled image item paint the previous identity/media for
        // even one frame. Clear the old texture first, then start the new source
        // on the next event-loop turn so the stable placeholder owns the gap.
        if (!root.deferNewLoadsWhileScrolling || !root.scrollMoving) {
            if (current !== requested) {
                if (current.length > 0) {
                    root.activeSource = ""
                    sourceStartTimer.restart()
                } else {
                    root.commitDecodeSize(true)
                    root.activeSource = requested
                }
            }
            return
        }

        // An explicit deferral may postpone a new identity. It must not cancel
        // an already-running request for the same identity on every gesture.
        if (current !== requested)
            root.activeSource = ""
    }

    function synchronizePresentation() {
        if (typeof renderingController !== "undefined" && renderingController
                && renderingController.shuttingDown === true)
            return
        if (!root.decodedReady) {
            if (!root.retainingSizedTexture)
                root.presentationReady = false
            return
        }
        // Optional presentation deferral never discards decoded bytes. Normal
        // feeds admit through the shared frame budget while scrolling too.
        if (!root.componentReady || (root.deferNewLoadsWhileScrolling && root.scrollMoving))
            return
        if (root.presentationReady)
            return
        if (root.requiresFramePacedPresentation) {
            if (root.remoteRaster && !root.rasterEligible)
                return
            if (!root.presentationAdmissionPending) {
                root.presentationAdmissionPending = true
                root.presentationGeneration += 1
                renderingController.requestMediaPresentation(root, root.presentationGeneration)
            }
            return
        }
        root.presentationReady = true
    }

    function invalidatePresentationAdmission() {
        if (root.presentationAdmissionPending
                && typeof renderingController !== "undefined" && renderingController
                && typeof renderingController.cancelMediaPresentation === "function")
            renderingController.cancelMediaPresentation(root)
        root.presentationGeneration += 1
        root.presentationAdmissionPending = false
        root.presentationReady = false
    }

    function quantizedDecodeDimension(requested, logicalSize) {
        const dpr = Math.max(1, Screen.devicePixelRatio)
        const physical = Math.max(Number(requested || 0),
                                  Math.ceil(Math.max(1, logicalSize) * dpr * renderingController.mediaDecodeScale))
        const quantum = Math.max(32, Number(root.decodeQuantum || 128))
        const cap = Qt.platform.os === "wasm"
            ? (root.mediaCategory === "miniapps" ? 768 : 1536)
            : (Qt.platform.os === "android" || Qt.platform.os === "ios" ? 2048 : 4096)
        return Math.min(cap, Math.max(quantum, Math.ceil(physical / quantum) * quantum))
    }

    function syncRetainWhileLoading() {
        // Image.retainWhileLoading was added in Qt 6.8. Use dynamic property
        // discovery so the QML source remains compatible with Genycaster's older
        // supported Qt baseline while Qt 6.8+ keeps the previous frame until the
        // replacement decode is ready.
        const propertyName = "retainWhileLoading"
        if (imageSource[propertyName] !== undefined)
            imageSource[propertyName] = root.retainPreviousWhileLoading || root.sizeReloadInFlight
    }

    Timer {
        id: presentationTimer
        interval: 0
        repeat: false
        // The signal handler belongs to this component's context. Unlike a
        // retained JS callback, it cannot run after that context is invalidated
        // while a discarded delegate is waiting for QObject destruction.
        onTriggered: {
            if (!root.componentReady)
                root.initializePresentation()
            else
                root.synchronizePresentation()
        }
    }

    Timer {
        id: sourceStartTimer
        interval: 48 + Math.max(0, root.loadDelayMs)
        repeat: false
        onTriggered: {
            root.synchronizeActiveSource()
            root.synchronizePresentation()
        }
    }

    function initializePresentation() {
        // Loader/ListView can attach the delegate's ancestors after this
        // child's Component.onCompleted. Wait until that attachment is done
        // before deciding whether a new request is allowed during scrolling.
        root.resolveScrollHost()
        root.syncRetainWhileLoading()
        root.commitDecodeSize(true)
        root.componentReady = true
        if (root.loadDelayMs > 0)
            sourceStartTimer.restart()
        else
            root.synchronizeActiveSource()
        root.synchronizePresentation()
    }
    Component.onCompleted: presentationTimer.start()
    onParentChanged: {
        if (root.componentReady)
            root.resolveScrollHost()
    }
    onActiveSourceChanged: {
        root.sizeReloadInFlight = false
        root.invalidatePresentationAdmission()
        presentationTimer.restart()
    }
    onDecodedReadyChanged: {
        if (!root.decodedReady && !root.retainingSizedTexture)
            root.invalidatePresentationAdmission()
        root.synchronizePresentation()
    }
    function rememberIntrinsicGeometry() {
        if (imageSource.status !== Image.Ready || root.providerFallback
                || root.rememberedIntrinsicSize.width > 0
                || root.activeSource.toString().length === 0)
            return
        const width = imageSource.implicitWidth
        const height = imageSource.implicitHeight
        if (width > 0 && height > 0 && isFinite(width) && isFinite(height))
            root.rememberedIntrinsicSize = Qt.size(width, height)
    }
    onStatusChanged: {
        if (status === Image.Ready || status === Image.Error) {
            root.sizeReloadInFlight = false
            if (status === Image.Ready) {
                root.rememberIntrinsicGeometry()
                if (!root.providerFallback && typeof renderingController !== "undefined"
                        && renderingController && renderingController.mediaDiagnosticsEnabled === true)
                    renderingController.recordRasterGeometry(root.mediaCategory,
                        root.stableSourceWidth, root.stableSourceHeight,
                        imageSource.implicitWidth, imageSource.implicitHeight,
                        root.width, root.height)
                if (root.desiredDecodeSize.width > root.stableSourceWidth * 1.20
                        || root.desiredDecodeSize.height > root.stableSourceHeight * 1.20)
                    decodeSizeSettler.restart()
            }
        }
    }
    Timer {
        id: intrinsicGeometryTimer
        interval: 0
        repeat: false
        onTriggered: root.rememberIntrinsicGeometry()
    }
    onSizeReloadInFlightChanged: root.syncRetainWhileLoading()
    onRetainPreviousWhileLoadingChanged: root.syncRetainWhileLoading()
    onSourceChanged: {
        intrinsicGeometryTimer.stop()
        root.rememberedIntrinsicSize = Qt.size(0, 0)
        root.sizeReloadInFlight = false
        root.manualLoadRequested = false
        // Invalidate synchronously: derived effectiveSource bindings may still
        // describe the old URL while this notification is being delivered.
        root.invalidatePresentationAdmission()
        root.activeSource = ""
        sourceStartTimer.restart()
    }
    onMediaCategoryChanged: {
        root.manualLoadRequested = false
        root.activeSource = ""
        sourceStartTimer.restart()
    }
    onManualLoadRequestedChanged: {
        root.activeSource = ""
        sourceStartTimer.restart()
    }
    onScrollMovingChanged: {
        if (root.scrollMoving && root.deferNewLoadsWhileScrolling) {
            // Pause only an unstarted source. A request already in flight must
            // survive touchpad/wheel bursts and reach a terminal state.
            if (root.activeSource.toString().length === 0)
                sourceStartTimer.stop()
        } else {
            if (root.activeSource.toString().length === 0)
                sourceStartTimer.restart()
            if (!root.scrollMoving && root.remoteRaster
                    && root.desiredDecodeSize !== root.stableDecodeSize)
                decodeSizeSettler.restart()
            root.synchronizePresentation()
        }
    }
    onDeferNewLoadsWhileScrollingChanged: {
        root.synchronizeActiveSource()
        root.synchronizePresentation()
    }
    onRequiresFramePacedPresentationChanged: {
        if (!root.presentationReady) {
            root.invalidatePresentationAdmission()
            root.synchronizePresentation()
        }
    }
    onLoadDelayMsChanged: {
        // Stagger only new loads. A recycled slot's changing delay is not a
        // reason to discard an in-flight request for the same URL.
        if (!root.componentReady || root.activeSource.toString().length > 0)
            return
        sourceStartTimer.restart()
    }
    // The provider owns bounded network retries and URL failure backoff. Once
    // it returns a terminal failure, keep that state until the source or policy
    // changes; reloading here only replays the cached failure and churns the UI.

    Connections {
        target: dataStorage
        function onPolicyChanged() {
            if (!root.remoteSource || root.ready)
                return
            root.activeSource = ""
            sourceStartTimer.restart()
        }
    }

    // Invoked directly by the GUI-thread scheduler. No per-image
    // subscription to a global grant signal, including for small avatars.
    function acceptMediaPresentation(generation: int) {
        if (!root.presentationAdmissionPending || generation !== root.presentationGeneration)
            return
        root.presentationAdmissionPending = false
        if (!root.decodedReady || (root.remoteRaster && !root.rasterEligible)
                || (root.deferNewLoadsWhileScrolling && root.scrollMoving))
            return
        root.presentationReady = true
    }

    // Stable placeholder remains present through the complete request lifecycle.
    // This avoids flashing the page/background while an async response is pending.
    Superellipse {
        anchors.fill: parent
        fillColor: root.fillColor
        curveDepth: root.curveDepth
        exponent: root.exponent
        borderWidth: 0
    }

    // Image owns decode, atlas and upload. Its direct texture is consumed by
    // the analytic contour material, not a ShaderEffectSource capture.
    Image {
        id: imageSource
        z: 1
        anchors.fill: parent
        // A momentary zero-size layout must not detach an existing texture.
        source: root.activeSource
        sourceSize: root.stableDecodeSize
        onImplicitWidthChanged: if (status === Image.Ready) intrinsicGeometryTimer.restart()
        onImplicitHeightChanged: if (status === Image.Ready) intrinsicGeometryTimer.restart()
        fillMode: root.fillMode
        asynchronous: root.asynchronous
        // RemoteImageProvider already has bounded memory + disk caches. Avoid
        // a second QML cache layer so transient one-pixel failure sentinels are
        // never sticky for the lifetime of the scene graph image key.
        cache: root.remoteSource || root.browserRemoteRaster ? false : root.cache
        smooth: true
        mipmap: false
        visible: !root.directMask && root.stillVisible
        // Retain-previous mode must keep the old pixels visible while an
        // updated source loads; a reveal must not override that contract.
        opacity: root.ready || (root.retainPreviousWhileLoading
                   && imageSource.status === Image.Loading) ? 1 : 0
        Behavior on opacity {
            enabled: root.smoothReveal && !Theme.reduceMotion && !root.scrollMoving
            NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic }
        }
        layer.enabled: root.maskActive && !root.directMask
        layer.smooth: true
        layer.effect: layer.enabled ? maskEffectComponent : null
    }

    Loader {
        id: directRaster
        objectName: "directRasterMaterial"
        z: 1
        x: root.visibleContentRect.x
        y: root.visibleContentRect.y
        width: root.visibleContentRect.width
        height: root.visibleContentRect.height
        // Keep material topology stable across readiness/resize transitions.
        active: root.directMask && root.activeSource.toString().length > 0
        visible: root.stillVisible
        opacity: root.stillVisible ? 1 : 0
        sourceComponent: ShaderEffect {
            property variant source: imageSource
            property vector4d contour: Qt.vector4d(width, height,
                Math.min(root.curveDepth, width / 2, height / 2), root.exponent)
            property vector2d uvScale: root.fillMode === Image.PreserveAspectCrop
                ? Qt.vector2d(Math.min(1, width / Math.max(1, root.paintedWidth)),
                              Math.min(1, height / Math.max(1, root.paintedHeight)))
                : Qt.vector2d(1, 1)
            property real pixelWidth: 1 / Math.max(1, Screen.devicePixelRatio)
            fragmentShader: "qrc:/genycaster/effects/raster-contour.frag.qsb"
            onStatusChanged: if (status === ShaderEffect.Error) root.directRasterFailed = true
        }
    }

    // GIF animation must be decoded by QMovie/AnimatedImage, not QImageReader
    // (which supplies only the first frame). Keep the still image beneath as
    // a fallback, and never unload the running animation when scrolling.
    Loader {
        id: gifLoader
        z: 1.5
        anchors.fill: parent
        active: root.allowAnimation && root.visible && root.componentReady
            && root.rasterRetained && root.activeSource.toString().length > 0
            && root.animatedGifUrl.length > 0 && !Theme.powerSave
            && (root.mediaCategory !== "avatars" || Theme.animateAvatars)
        sourceComponent: AnimatedImage {
        id: animatedGif
        MediaResourceScope { id: animationScope; owner: root }
        anchors.fill: parent
        // ImagePolicy applies to GIFs too. The still-image provider remains
        // the single source of admission and is allowed to fail back cleanly
        // when the animation codec or network is unavailable.
        source: root.componentReady && root.hasRenderableGeometry
                && root.rasterRetained && !Theme.powerSave
                && (root.mediaCategory !== "avatars" || Theme.animateAvatars)
                && root.automaticDownloadAllowed
                && root.activeSource.toString().length > 0
            ? root.animatedGifUrl : ""
        sourceSize: Qt.size(Math.min(1024, root.stableSourceWidth), Math.min(1024, root.stableSourceHeight))
        fillMode: root.fillMode
        asynchronous: true
        cache: false
        visible: root.animatedGifUrl.length > 0 && status === AnimatedImage.Ready
        // Keep the still poster while motion or viewport admission suspends GIFs.
        playing: visible && root.visible && animationScope.eligible && !root.scrollMoving && !Theme.powerSave
                 && (root.mediaCategory !== "avatars" || Theme.animateAvatars)
        smooth: true
        // The shared mask texture exists only after the still-image layer is
        // ready. Never apply a missing mask to an otherwise valid GIF.
        layer.enabled: visible && root.maskActive
        layer.effect: layer.enabled ? maskEffectComponent : null
        }
    }

    Component {
        id: maskEffectComponent

        MultiEffect {
            objectName: "mediaMaskEffect"
            maskEnabled: true
            maskSource: maskShape
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
            autoPaddingEnabled: false
            antialiasing: true
        }
    }

    // PreserveAspectFit may paint the real image inside the Item bounds. Build
    // a full-size transparent mask texture and place the superellipse exactly
    // on the painted pixel rectangle; masking the outer delegate bounds leaves
    // interior fitted-image corners square.
    Loader {
        id: maskShape
        objectName: "mediaMaskLoader"
        // Hidden geometry still costs CPU to instantiate. Build it only when
        // the image is admitted, not when its placeholder enters view.
        anchors.fill: parent
        visible: false
        active: root.legacyMaskActive
        layer.enabled: root.legacyMaskActive
        layer.smooth: true
        layer.samples: renderingController.layerSampleCount

        sourceComponent: Component {
            Item {
                Superellipse {
                    x: root.visibleContentRect.x
                    y: root.visibleContentRect.y
                    width: root.visibleContentRect.width
                    height: root.visibleContentRect.height
                    fillColor: "white"
                    curveDepth: Math.min(root.curveDepth, width / 2, height / 2)
                    exponent: root.exponent
                    antialiasing: true
                }
            }
        }
    }

    Item {
        id: loadingIndicator
        objectName: "mediaLoadingIndicator"
        z: 2
        readonly property real availableSize: Math.min(root.width, root.height)
        readonly property real devicePixelRatio: Math.max(1, Screen.devicePixelRatio)
        readonly property real requestedSize: Math.max(14, Math.min(28, availableSize * 0.34))
        readonly property real snappedSize: Math.max(1,
            Math.round(requestedSize * devicePixelRatio) / devicePixelRatio)
        width: snappedSize
        height: snappedSize
        x: Math.round(((root.width - width) / 2) * devicePixelRatio) / devicePixelRatio
        y: Math.round(((root.height - height) / 2) * devicePixelRatio) / devicePixelRatio
        visible: root.showLoadingIndicator && root.loading && !root.retainingSizedTexture
                 && !root.manualLoadRequired

        GenySpinner {
            objectName: "mediaLoadingSpinner"
            anchors.fill: parent
            visible: loadingIndicator.availableSize >= 56
            // GenySpinner resolves the enclosing Flickable itself and keeps
            // animation phase stable through brief state/visibility churn.
            running: visible
            strokeWidth: Math.max(1.5, width * 0.10)
            color: Theme.accent
        }

        // A loading fallback is a font glyph, not an SVG that needs a hidden
        // source texture plus MultiEffect colourization. Keeping it shader-free
        // also prevents invisible loading icons from taxing every image tile.
        Text {
            objectName: "mediaLoadingFallbackIcon"
            anchors.fill: parent
            visible: loadingIndicator.availableSize < 56
            text: Theme.iconGlyph(Theme.iconImage)
            color: Theme.textTertiary
            font.family: Theme.iconFamily(Theme.iconImage)
            font.styleName: Theme.iconUsesRegular(Theme.iconImage) ? "Regular" : "Solid"
            font.weight: Theme.iconUsesRegular(Theme.iconImage) ? Font.Normal : Font.Black
            font.pixelSize: Math.max(12, Math.min(18, loadingIndicator.width))
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
            antialiasing: true
            opacity: 0.72
        }
    }

    Text {
        z: 2
        anchors.centerIn: parent
        width: Math.max(22, font.pixelSize + 2)
        height: width
        visible: root.showFailureIndicator && !root.policyBlocked
                 && root.source.toString().length > 0 && root.loadFailed
        text: Theme.iconGlyph(Theme.iconImage)
        color: Theme.textTertiary
        font.family: Theme.iconFamily(Theme.iconImage)
        font.styleName: Theme.iconUsesRegular(Theme.iconImage) ? "Regular" : "Solid"
        font.weight: Theme.iconUsesRegular(Theme.iconImage) ? Font.Normal : Font.Black
        font.pixelSize: Math.max(14, Math.min(26, Math.min(root.width, root.height) * 0.24))
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
        antialiasing: true
        opacity: 0.66
    }

    Item {
        id: manualLoadOverlay
        z: 3
        anchors.fill: parent
        visible: root.manualLoadRequired && root.showManualLoadIndicator

        Loader {
            objectName: "mediaManualLoadLoader"
            anchors.fill: parent
            active: manualLoadOverlay.visible
            sourceComponent: Component {
                Item {
                    Superellipse {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - Theme.space3 * 2, manualLoadRow.implicitWidth + Theme.space3 * 2)
                        height: Math.max(34, manualLoadRow.implicitHeight + Theme.space2 * 2)
                        fillColor: Theme.surfaceElevated
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderStrong
                        curveDepth: height / 2
                        elevation: Theme.elevationControl

                        Row {
                            id: manualLoadRow
                            anchors.centerIn: parent
                            spacing: Theme.space1
                            layoutDirection: Theme.uiDirection
                            Icon { text: Theme.iconDownload; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                            DirectionAwareLabel {
                                text: qsTrId("genycaster.storage.tap_to_load")
                                color: Theme.ink
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }
                    }
                }
            }
        }

        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: {
                root.manualLoadRequested = true
            }
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        Accessible.role: Accessible.Button
        Accessible.name: qsTrId("genycaster.storage.tap_to_load")
    }

    Loader {
        z: 4
        active: root.borderWidth > 0
        x: root.visibleContentRect.x
        y: root.visibleContentRect.y
        width: root.visibleContentRect.width
        height: root.visibleContentRect.height
        sourceComponent: Component {
            Superellipse {
                fillColor: "transparent"
                borderWidth: root.borderWidth
                borderColor: root.borderColor
                curveDepth: Math.min(root.curveDepth, width / 2, height / 2)
                exponent: root.exponent
            }
        }
    }
}
