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
    // Feed/list delegates suspend pending image requests while their Flickable
    // moves. Already-presented images stay resident; unfinished requests resume
    // through the provider's cache after settling.
    property bool deferNewLoadsWhileScrolling: true
    // Gallery siblings use a small per-index delay so cache hits and texture
    // creation are admitted over separate frames instead of one render burst.
    property int loadDelayMs: 0
    property var scrollHost: null
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
    readonly property real sourceImplicitWidth: imageSource.implicitWidth
    readonly property real sourceImplicitHeight: imageSource.implicitHeight
    readonly property Item renderItem: imageSource
    readonly property real paintedWidth: imageSource.paintedWidth
    readonly property real paintedHeight: imageSource.paintedHeight
    readonly property real paintedX: Math.max(0, (root.width - imageSource.paintedWidth) / 2)
    readonly property real paintedY: Math.max(0, (root.height - imageSource.paintedHeight) / 2)
    readonly property real paintedAspectRatio: imageSource.paintedHeight > 1
                                               ? imageSource.paintedWidth / imageSource.paintedHeight : 0
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
                                        || (root.decodedReady && !root.presentationReady))
    readonly property bool decodedReady: imageSource.status === Image.Ready && !root.providerFallback
    readonly property bool ready: root.decodedReady && root.presentationReady
    readonly property bool hasRenderableGeometry: root.width >= 1 && root.height >= 1
    readonly property bool requiresFramePacedPresentation: root.framePacedPresentation
                                                            && root.width * root.height >= 160 * 160
    readonly property bool maskActive: root.visible && root.clipToSuperellipse
                                       && imageSource.visible && root.hasRenderableGeometry
    readonly property int stableSourceWidth: root.quantizedDecodeDimension(root.requestedSourceWidth, root.width)
    readonly property int stableSourceHeight: root.quantizedDecodeDimension(root.requestedSourceHeight, root.height)

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
        if (!root.componentReady)
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
                    root.activeSource = requested
                }
            }
            return
        }

        // New/recycled URLs never start while the Flickable is moving.
        // An unfinished current request is detached by onScrollMovingChanged so
        // Qt cannot prepare/upload its texture inside the kinetic-scroll frame.
        if (current !== requested || (!root.ready && !root.loadFailed))
            root.activeSource = ""
    }

    function synchronizePresentation() {
        if (!root.decodedReady) {
            root.presentationReady = false
            return
        }
        // Pending requests are detached on motion to prevent texture upload.
        // Also gate presentation so a late state notification cannot activate
        // mask layers and dependent shadows during the gesture.
        // Once admitted, an image stays visible throughout subsequent motion.
        if (!root.componentReady || (root.deferNewLoadsWhileScrolling && root.scrollMoving))
            return
        if (root.presentationReady)
            return
        if (root.requiresFramePacedPresentation) {
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
        root.presentationGeneration += 1
        root.presentationAdmissionPending = false
        root.presentationReady = false
    }

    function quantizedDecodeDimension(requested, logicalSize) {
        const dpr = Math.max(1, Screen.devicePixelRatio)
        const physical = Math.max(Number(requested || 0),
                                  Math.ceil(Math.max(1, logicalSize) * dpr * renderingController.mediaDecodeScale))
        const quantum = Math.max(32, Number(root.decodeQuantum || 128))
        return Math.max(quantum, Math.ceil(physical / quantum) * quantum)
    }

    function syncRetainWhileLoading() {
        // Image.retainWhileLoading was added in Qt 6.8. Use dynamic property
        // discovery so the QML source remains compatible with Genycaster's older
        // supported Qt baseline while Qt 6.8+ keeps the previous frame until the
        // replacement decode is ready.
        const propertyName = "retainWhileLoading"
        if (imageSource[propertyName] !== undefined)
            imageSource[propertyName] = root.retainPreviousWhileLoading
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
        root.invalidatePresentationAdmission()
        presentationTimer.restart()
    }
    onDecodedReadyChanged: {
        if (!root.decodedReady)
            root.invalidatePresentationAdmission()
        root.synchronizePresentation()
    }
    onRetainPreviousWhileLoadingChanged: root.syncRetainWhileLoading()
    onSourceChanged: {
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
            sourceStartTimer.stop()
            if (!root.ready && !root.loadFailed) {
                // Detach the QML Image so Qt cannot build/upload a texture while
                // the render thread is animating the Flickable. The provider's
                // cancellation handle deliberately keeps already-admitted
                // fetch/decode work alive to populate its bounded cache.
                root.invalidatePresentationAdmission()
                root.activeSource = ""
            }
        } else if (!root.scrollMoving) {
            sourceStartTimer.restart()
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
        if (!root.componentReady || root.ready || root.loadFailed)
            return
        root.activeSource = ""
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

    Connections {
        target: renderingController

        function onMediaPresentationGranted(requester, generation, presentationSequence) {
            if (requester !== root || Number(generation) !== root.presentationGeneration)
                return
            root.presentationAdmissionPending = false
            if (!root.decodedReady || (root.deferNewLoadsWhileScrolling && root.scrollMoving))
                return
            root.presentationReady = true
        }
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

    // The image owns its masking layer directly. This avoids the previous
    // hidden-Image -> sibling MultiEffect proxy path, which behaved differently
    // from a plain Image for very wide profile banners and added an avoidable
    // intermediate texture/proxy.
    Image {
        id: imageSource
        z: 1
        anchors.fill: parent
        source: root.hasRenderableGeometry ? root.activeSource : ""
        sourceSize.width: root.stableSourceWidth
        sourceSize.height: root.stableSourceHeight
        fillMode: root.fillMode
        asynchronous: root.asynchronous
        // RemoteImageProvider already has bounded memory + disk caches. Avoid
        // a second QML cache layer so transient one-pixel failure sentinels are
        // never sticky for the lifetime of the scene graph image key.
        cache: root.remoteSource ? false : root.cache
        smooth: true
        mipmap: false
        visible: root.ready || (root.retainPreviousWhileLoading && imageSource.status === Image.Loading)
        layer.enabled: root.maskActive
        layer.smooth: true
        // Assigning an inline effect unconditionally constructs QQuickMultiEffect
        // even while the layer is disabled. Keep only a Component descriptor and
        // give it to the layer once a clipped image is actually presented.
        layer.effect: root.maskActive ? maskEffectComponent : null
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
        // the image is admitted at rest, not when its placeholder enters view.
        anchors.fill: parent
        visible: false
        active: root.maskActive
        layer.enabled: root.maskActive
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
        visible: root.showLoadingIndicator && root.loading && !root.manualLoadRequired

        GenySpinner {
            objectName: "mediaLoadingSpinner"
            anchors.fill: parent
            visible: loadingIndicator.availableSize >= 56 && !Theme.reduceMotion
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
            visible: loadingIndicator.availableSize < 56 || Theme.reduceMotion
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
