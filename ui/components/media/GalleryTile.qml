import QtQuick
import QtQuick.Shapes
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

Item {
    id: root
    clip: true

    property var embed: ({})
    readonly property bool officialSticker: StickerCatalog.isStickerUrl(
        String(root.embed.url || root.embed.value || ""))
    property string continuationText: ""
    property bool preserveAspect: false
    // Decoded artwork fits the reserved slot independently of layout.
    // The performance-safe defaults avoid briefly constructing layer effects
    // before GalleryLayout applies its multi-image bindings. Single-image slots
    // opt into both features explicitly.
    property bool ambientEnabled: false
    property bool asynchronousAmbient: true
    property bool useLayeredMask: false
    property int loadDelayMs: 0
    property color cornerCoverColor: Theme.background

    readonly property rect cornerTargetRect: root.preserveAspect && preview.ready
                                                     ? preview.visibleContentRect
                                                     : Qt.rect(0, 0, root.width, root.height)
    readonly property real cornerRadius: Math.max(
                                                    0,
                                                    Math.min(Theme.shapeMedia,
                                                             root.cornerTargetRect.width / 2,
                                                             root.cornerTargetRect.height / 2))
    readonly property real cornerCubicFactor: {
        const safeExponent = Math.max(2.0, Theme.superellipseExponent)
        const diagonal = Math.pow(0.5, 1 / safeExponent)
        return Math.max(0.5522847498, Math.min(0.985, (8 * diagonal - 4) / 3))
    }

    signal openRequested(string url)
    signal aspectRatioResolved(real ratio)

    property real reportedAspect: 0
    property string reportedMediaIdentity: ""

    function reportDecodedAspect() {
        if (root.reportedAspect > 0 || preview.status !== Image.Ready)
            return
        const sourceWidth = Number(preview.sourceImplicitWidth || 0)
        const sourceHeight = Number(preview.sourceImplicitHeight || 0)
        const ratio = sourceWidth > 1 && sourceHeight > 1
                      ? sourceWidth / sourceHeight : 0
        if (!(ratio > 0))
            return
        if (Math.abs(root.reportedAspect - ratio) < 0.015)
            return
        root.reportedAspect = ratio
        root.aspectRatioResolved(ratio)
    }

    // Our own media host sends the appropriate CORS headers for native and
    // WebAssembly. If the remote-image provider fails, retry that canonical
    // origin directly rather than leaving a giant empty gallery tile. Never
    // bypass a user's disabled photo-auto-download setting.
    property bool originFallbackAttempted: false
    readonly property string ownMediaUrl: {
        const target = String(root.embed.url || root.embed.value || "").trim()
        return /^https:\/\/media-farcaster\.genyleap\.com\/m\/v1\/[a-z0-9/_-]+\.(?:jpe?g|png|webp|gif)(?:\?[^#]*)?$/i.test(target)
               ? target : ""
    }
    onEmbedChanged: {
        // Refreshing metadata for the SAME source must not revoke a ready
        // image's geometry. A genuinely recycled identity starts at zero.
        const identity = String(root.embed.url || root.embed.imageSource || root.embed.value || "")
        if (identity !== root.reportedMediaIdentity) {
            root.reportedMediaIdentity = identity
            root.reportedAspect = 0
            root.originFallbackAttempted = false
        }
    }
    function resolvedSource() {
        if (root.officialSticker) {
            const canonical = String(root.embed.url || root.embed.value || "")
            // Android/WASM Qt imageformat plugins do not consistently decode
            // third-party WebP sticker thumbnails. The server's pinned-ID
            // preview is a CORS-safe PNG; the original cast embed is unchanged.
            return StickerCatalog.previewForPublishedUrl(canonical,
                openProofConnections
                  ? String(openProofConnections.stickerPreviewBaseUrl || "") : "",
                Qt.platform.os === "wasm" || Qt.platform.os === "android")
        }
        if (root.originFallbackAttempted && root.ownMediaUrl.length > 0)
            return root.ownMediaUrl
        const direct = String(root.embed.imageSource || "")
        if (direct.length > 0)
            return direct
        const target = String(root.embed.url || root.embed.value || "")
        if (typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null)
            return linkPreviewResolver.imageSourceForUrl(target)
        return target
    }

    Loader {
        id: ambientLoader
        objectName: "galleryAmbientLoader"
        anchors.fill: parent
        active: root.ambientEnabled && !root.officialSticker
        asynchronous: root.asynchronousAmbient

        sourceComponent: Component {
            AmbientMediaBackdrop {
                sourceItem: preview.renderItem
                active: root.ambientEnabled && preview.ready
                spread: root.preserveAspect ? 13 : 11
                strength: Theme.dark ? 0.86 : 0.76
                blurStrength: 1.0
                saturation: 0.0
                sampleSize: root.preserveAspect ? 192 : 160
                verticalOffset: root.preserveAspect ? 12 : 10
                shadowScale: 0.955
            }
        }
    }

    SuperellipseImage {
        id: preview
        objectName: "galleryPreview"
        anchors.fill: parent
        source: root.resolvedSource()
        loadDelayMs: root.loadDelayMs
        mediaCategory: "photos"
        fillMode: root.officialSticker || root.preserveAspect
                  ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        curveDepth: Theme.shapeMedia
        clipToSuperellipse: root.useLayeredMask && !root.officialSticker
        fillColor: root.officialSticker ? "transparent"
                   : !preview.ready ? Theme.surfaceMuted
                   : root.preserveAspect ? "transparent" : Theme.surface
        smoothReveal: true
        borderWidth: 0
        onLoadFailedChanged: {
            if (loadFailed && !root.originFallbackAttempted
                    && root.ownMediaUrl.length > 0
                    && dataStorage.autoDownloadAllowed("photos")
                    && String(root.embed.imageSource || "").startsWith("image://remote/"))
                root.originFallbackAttempted = true
        }
        onStatusChanged: aspectReportTimer.restart()
        onSourceImplicitWidthChanged: aspectReportTimer.restart()
        onSourceImplicitHeightChanged: aspectReportTimer.restart()
        // Painted dimensions are viewport geometry, not source metadata.
    }

    // Read both intrinsic dimensions after the Image.Ready transaction.
    // Independent width/height notifications and later resolution changes
    // must not be treated as new source aspect ratios.
    Timer {
        id: aspectReportTimer
        interval: 0
        repeat: false
        onTriggered: root.reportDecodedAspect()
    }

    // Multi-image feed galleries stay on Image's direct scene-graph texture path.
    // Cover only the few corner pixels with a vector inverse instead of allocating
    // a full-size image layer, a full-size mask layer and a MultiEffect per tile.
    Shape {
        id: cornerCover
        x: root.cornerTargetRect.x
        y: root.cornerTargetRect.y
        width: root.cornerTargetRect.width
        height: root.cornerTargetRect.height
        z: 2
        // PreserveAspectFit can paint a single image inside a smaller rectangle
        // than the reserved tile. Round that painted rectangle itself; masking
        // only the outer tile leaves the visible image with square corners.
        visible: !root.useLayeredMask && preview.ready && width > 0 && height > 0
                 && !root.officialSticker
        preferredRendererType: Shape.CurveRenderer
        asynchronous: false

        ShapePath {
            fillColor: root.cornerCoverColor
            fillRule: ShapePath.OddEvenFill
            strokeColor: "transparent"
            strokeWidth: -1

            startX: 0
            startY: 0
            PathLine { x: cornerCover.width; y: 0 }
            PathLine { x: cornerCover.width; y: cornerCover.height }
            PathLine { x: 0; y: cornerCover.height }
            PathLine { x: 0; y: 0 }

            PathMove { x: root.cornerRadius; y: 0 }
            PathLine { x: cornerCover.width - root.cornerRadius; y: 0 }
            PathCubic {
                x: cornerCover.width
                y: root.cornerRadius
                control1X: cornerCover.width - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control1Y: 0
                control2X: cornerCover.width
                control2Y: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
            }
            PathLine { x: cornerCover.width; y: cornerCover.height - root.cornerRadius }
            PathCubic {
                x: cornerCover.width - root.cornerRadius
                y: cornerCover.height
                control1X: cornerCover.width
                control1Y: cornerCover.height - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control2X: cornerCover.width - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control2Y: cornerCover.height
            }
            PathLine { x: root.cornerRadius; y: cornerCover.height }
            PathCubic {
                x: 0
                y: cornerCover.height - root.cornerRadius
                control1X: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
                control1Y: cornerCover.height
                control2X: 0
                control2Y: cornerCover.height - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
            }
            PathLine { x: 0; y: root.cornerRadius }
            PathCubic {
                x: root.cornerRadius
                y: 0
                control1X: 0
                control1Y: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
                control2X: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
                control2Y: 0
            }
        }
    }

    // SuperellipseImage owns the placeholder, spinner and failure/manual-load
    // states. A sibling cover here would obscure those states while loading.

    Superellipse {
        z: 3
        visible: root.continuationText.length > 0
        anchors.fill: parent
        fillColor: Theme.mediaOverlay
        curveDepth: Theme.shapeMedia

        DirectionAwareLabel {
            anchors.centerIn: parent
            text: root.continuationText
            color: Theme.inkInverse
            font.pixelSize: Theme.typeTitle2Size
            font.weight: Theme.weightDemiBold
        }
    }

    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            if (!preview.ready)
                return
            root.openRequested(String(root.embed.url || ""))
        }
    }

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTrId("genycaster.embed.image_embed")
}
