import QtQuick
import QtQuick.Effects
import Farcaster.Client

/**
 * Full-colour media shadow modeled after CuteMobile's ColorizedBox.
 *
 * CuteMobile's visual contract is intentionally simple:
 * - render a second copy of the media behind the sharp image;
 * - make the copy 20 logical pixels smaller than the media;
 * - keep it centered horizontally and move it 15 pixels downward;
 * - blur against transparent pixels so only a chromatic falloff survives;
 * - let the sharp media occlude the blurred copy itself.
 *
 * Qt 6 implementation:
 * A transparent offscreen canvas replaces FastBlur::transparentBorder. The
 * captured media clone sits inside that canvas with the same inset/offset
 * geometry as the original component. MultiEffect then blurs the canvas while
 * auto-padding stays disabled, so the effect can never grow into a card-sized
 * coloured rectangle.
 */
Item {
    id: root

    signal captureRequested()

    property Item sourceItem: null
    property url source: ""
    property bool active: root.sourceItem !== null || root.source.toString().length > 0

    // Public tuning remains deliberately small. Geometry follows CuteMobile;
    // callers may only tune intensity and the offscreen sampling budget.
    property real strength: Theme.dark ? 0.76 : 0.64
    property real blurStrength: 1.0
    property int sampleSize: 192
    property real saturation: 0.0
    // Ambient blur needs two extra offscreen textures. Scrollable timelines
    // deliberately omit that decorative work for their complete lifetime;
    // creating it after each fling merely moves the frame spike to settling.
    // A non-scrolling surface, or an explicit false value, keeps the effect.
    property bool deferWhileScrolling: true
    property var scrollHost: null
    property bool componentReady: false
    property bool effectAdmitted: false
    property bool captureDirty: true
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    readonly property bool scrollableContext: root.scrollHost !== null
    readonly property bool effectAllowed: !root.deferWhileScrolling || !root.scrollableContext
    // Artwork is static. Snapshot it when its source/geometry changes; scrolling
    // and pointer-only frames can reuse the existing texture.
    readonly property real renderScale: Math.max(0.62, Math.min(1.0, renderingController.effectScale))
    readonly property bool effectActive: root.visible && root.effectAllowed && root.effectAdmitted
    readonly property bool captureLive: false

    // Kept for source compatibility with the previous AmbientMediaBackdrop API.
    // CuteMobile used fixed 10px inset / 15px vertical offset, so spread and
    // shadowScale are intentionally not used to alter the topology anymore.
    property real spread: 15
    property real verticalOffset: 15
    property real shadowScale: 1.0

    readonly property bool usesExternalItem: root.sourceItem !== null
    readonly property Item effectiveSourceItem: root.usesExternalItem ? root.sourceItem : fallbackImage
    readonly property bool fallbackReady: fallbackImage.status === Image.Ready
    readonly property bool sourceReady: root.usesExternalItem ? root.active : root.fallbackReady

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

    function synchronizeEffect() {
        if (!root.effectAllowed || !root.active || !root.sourceReady) {
            root.effectAdmitted = false
            root.captureDirty = true
            return
        }
        if (root.componentReady) {
            root.effectAdmitted = true
            if (root.captureDirty)
                captureUpdateTimer.restart()
        }
    }

    function invalidateCapture() {
        root.captureDirty = true
        if (root.componentReady)
            captureUpdateTimer.restart()
    }

    function initializeEffect() {
        root.resolveScrollHost()
        root.componentReady = true
        root.synchronizeEffect()
    }
    Component.onCompleted: effectUpdateTimer.start()
    onParentChanged: {
        if (root.componentReady)
            root.resolveScrollHost()
    }
    onScrollHostChanged: root.synchronizeEffect()
    onEffectAllowedChanged: root.synchronizeEffect()
    onActiveChanged: root.synchronizeEffect()
    onSourceReadyChanged: root.synchronizeEffect()
    onSourceItemChanged: {
        root.effectAdmitted = false
        root.captureDirty = true
        effectUpdateTimer.restart()
    }
    onSourceChanged: {
        root.effectAdmitted = false
        root.captureDirty = true
        effectUpdateTimer.restart()
    }
    onDeferWhileScrollingChanged: root.synchronizeEffect()

    Timer {
        id: effectUpdateTimer
        interval: 0
        repeat: false
        // Context-owned signal delivery is discarded when a delegate's QML
        // context dies, even if its visual QObject has not been deleted yet.
        onTriggered: {
            if (!root.componentReady)
                root.initializeEffect()
            else
                root.synchronizeEffect()
        }
    }

    Timer {
        id: captureUpdateTimer
        interval: 0
        repeat: false
        // Coalesce initial Image/Loader geometry notifications and let source
        // admission bindings settle before requesting one offscreen render.
        onTriggered: {
            if (!root.captureDirty || !root.effectActive || effectLoader.item === null)
                return
            root.captureDirty = false
            root.captureRequested()
        }
    }

    readonly property bool sourceUsesAspectFit: root.usesExternalItem
                                                 && root.sourceItem.fillMode !== undefined
                                                 && root.sourceItem.fillMode === Image.PreserveAspectFit
                                                 && root.sourceItem.paintedWidth !== undefined
                                                 && root.sourceItem.paintedHeight !== undefined

    readonly property real sourcePaintedWidth: root.sourceUsesAspectFit
                                                ? Math.max(1, Number(root.sourceItem.paintedWidth || 0))
                                                : Math.max(1, Number(root.effectiveSourceItem ? root.effectiveSourceItem.width : root.width))
    readonly property real sourcePaintedHeight: root.sourceUsesAspectFit
                                                 ? Math.max(1, Number(root.sourceItem.paintedHeight || 0))
                                                 : Math.max(1, Number(root.effectiveSourceItem ? root.effectiveSourceItem.height : root.height))

    readonly property real mediaWidth: Math.max(1, Math.min(root.width, root.sourcePaintedWidth))
    readonly property real mediaHeight: Math.max(1, Math.min(root.height, root.sourcePaintedHeight))
    readonly property real mediaX: Math.max(0, (root.width - root.mediaWidth) / 2)
    readonly property real mediaY: Math.max(0, (root.height - root.mediaHeight) / 2)

    readonly property rect captureRect: root.sourceUsesAspectFit
                                         ? Qt.rect(Math.max(0, (root.sourceItem.width - root.sourcePaintedWidth) / 2),
                                                   Math.max(0, (root.sourceItem.height - root.sourcePaintedHeight) / 2),
                                                   root.sourcePaintedWidth,
                                                   root.sourcePaintedHeight)
                                         : Qt.rect(0, 0,
                                                   Math.max(1, root.effectiveSourceItem ? root.effectiveSourceItem.width : root.width),
                                                   Math.max(1, root.effectiveSourceItem ? root.effectiveSourceItem.height : root.height))

    // Exact ColorizedBox geometry, scaled down only for very small thumbnails.
    readonly property real insetX: Math.min(10, Math.max(3, root.mediaWidth * 0.035))
    readonly property real insetY: Math.min(10, Math.max(3, root.mediaHeight * 0.035))
    readonly property real cloneWidth: Math.max(1, root.mediaWidth - root.insetX * 2)
    readonly property real cloneHeight: Math.max(1, root.mediaHeight - root.insetY * 2)
    readonly property real cloneX: root.mediaX + root.insetX
    readonly property real cloneY: root.mediaY + root.insetY
                                 + Math.min(15, Math.max(5, root.mediaHeight * 0.035))

    // FastBlur::transparentBorder must give the blur kernel enough transparent
    // pixels to decay to zero before the offscreen texture ends. A percentage
    // of media size is not sufficient for a 64px blur and produces a visible
    // rectangular cut line. Size the feather from the effective blur budget,
    // then keep a media-relative floor for small cards.
    readonly property real effectiveBlurRadius: 64 * 0.55 * Math.max(0.94, Math.min(1.0, root.blurStrength))
    readonly property real transparentBorder: Math.min(128,
                                                       Math.max(72,
                                                                root.effectiveBlurRadius * 2.65,
                                                                Math.min(root.mediaWidth, root.mediaHeight) * 0.14))
    readonly property real canvasX: root.cloneX - root.transparentBorder
    readonly property real canvasY: root.cloneY - root.transparentBorder
    readonly property real canvasWidth: root.cloneWidth + root.transparentBorder * 2
    readonly property real canvasHeight: root.cloneHeight + root.transparentBorder * 2

    readonly property real canvasAspect: root.canvasHeight > 1 ? root.canvasWidth / root.canvasHeight : 1
    readonly property int effectiveSampleSize: Math.max(112, Math.round(root.sampleSize * root.renderScale))
    readonly property int layerPixelWidth: root.canvasAspect >= 1
                                           ? root.effectiveSampleSize
                                           : Math.max(80, Math.round(root.effectiveSampleSize * root.canvasAspect))
    readonly property int layerPixelHeight: root.canvasAspect >= 1
                                            ? Math.max(80, Math.round(root.effectiveSampleSize / root.canvasAspect))
                                            : root.effectiveSampleSize

    visible: root.effectAllowed && root.active && root.sourceReady
             && root.width > 4 && root.height > 4
    clip: false

    Image {
        id: fallbackImage
        anchors.fill: parent
        source: root.effectAllowed && !root.usesExternalItem ? root.source : ""
        sourceSize.width: root.effectiveSampleSize
        sourceSize.height: root.effectiveSampleSize
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        visible: false
    }

    // Do not even instantiate the effect/capture tree for feed delegates. A
    // disabled layer avoids rendering, but constructing hundreds of
    // MultiEffect and ShaderEffectSource objects still compiles/materializes
    // resources as delegates enter the cache buffer.
    Loader {
        id: effectLoader
        objectName: "ambientEffectLoader"
        x: root.canvasX
        y: root.canvasY
        width: root.canvasWidth
        height: root.canvasHeight
        active: root.effectActive
        onLoaded: root.invalidateCapture()

        sourceComponent: Component {
            // This is the Qt 6 equivalent of CuteMobile's FastBlur item. The
            // canvas is larger than the clone, so its layer contains a real
            // transparent border and the blur fades before its texture ends.
            Item {
                id: blurCanvas
                opacity: Math.max(0, Math.min(1, root.strength))

                layer.enabled: true
                layer.live: true
                layer.samples: 1
                layer.smooth: true
                layer.mipmap: false
                layer.textureSize: Qt.size(root.layerPixelWidth, root.layerPixelHeight)
                layer.effect: MultiEffect {
                    blurEnabled: true
                    blur: Math.max(0.94, Math.min(1.0, root.blurStrength))
                    blurMax: 64
                    // FastBlur internally downsampled its input. A modest
                    // multiplier keeps the same falloff without extra lookups.
                    blurMultiplier: 0.55
                    autoPaddingEnabled: false
                    saturation: Math.max(-0.05, Math.min(0.08, root.saturation))
                    brightness: 0.0
                    contrast: 0.0
                }

                ShaderEffectSource {
                    id: mediaClone
                    objectName: "ambientMediaCapture"
                    x: root.transparentBorder
                    y: root.transparentBorder
                    width: root.cloneWidth
                    height: root.cloneHeight
                    sourceItem: root.effectiveSourceItem
                    sourceRect: root.captureRect
                    textureSize: Qt.size(Math.max(64, Math.min(root.effectiveSampleSize, Math.round(root.cloneWidth))),
                                         Math.max(64, Math.min(root.effectiveSampleSize, Math.round(root.cloneHeight))))
                    live: root.captureLive
                    samples: 1
                    onSourceItemChanged: root.invalidateCapture()
                    onSourceRectChanged: root.invalidateCapture()
                    onTextureSizeChanged: root.invalidateCapture()
                    recursive: false
                    smooth: true
                    mipmap: false
                    hideSource: false
                }

                Connections {
                    target: root
                    function onCaptureRequested() {
                        mediaClone.scheduleUpdate()
                    }
                }
            }
        }
    }
}
