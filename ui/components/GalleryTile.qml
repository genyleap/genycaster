import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property var embed: ({})
    property string continuationText: ""
    property bool preserveAspect: false
    // The performance-safe defaults avoid briefly constructing layer effects
    // before GalleryLayout applies its multi-image bindings. Single-image slots
    // opt into both features explicitly.
    property bool ambientEnabled: false
    property bool useLayeredMask: false
    property int loadDelayMs: 0
    property color cornerCoverColor: Theme.background

    readonly property real cornerRadius: Math.max(
                                                    0,
                                                    Math.min(Theme.shapeMedia,
                                                             root.width / 2,
                                                             root.height / 2))
    readonly property real cornerCubicFactor: {
        const safeExponent = Math.max(2.0, Theme.superellipseExponent)
        const diagonal = Math.pow(0.5, 1 / safeExponent)
        return Math.max(0.5522847498, Math.min(0.985, (8 * diagonal - 4) / 3))
    }

    signal openRequested(string url)
    signal aspectRatioResolved(real ratio)

    property real reportedAspect: 0

    function reportDecodedAspect() {
        if (preview.status !== Image.Ready)
            return
        const paintedRatio = Number(preview.paintedAspectRatio || 0)
        const sourceWidth = Number(preview.sourceImplicitWidth || 0)
        const sourceHeight = Number(preview.sourceImplicitHeight || 0)
        const ratio = paintedRatio > 0
                      ? paintedRatio
                      : (sourceWidth > 1 && sourceHeight > 1 ? sourceWidth / sourceHeight : 0)
        if (!(ratio > 0))
            return
        if (Math.abs(root.reportedAspect - ratio) < 0.015)
            return
        root.reportedAspect = ratio
        root.aspectRatioResolved(ratio)
    }

    function resolvedSource() {
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
        active: root.ambientEnabled
        asynchronous: true

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
        fillMode: root.preserveAspect ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        curveDepth: Theme.shapeMedia
        clipToSuperellipse: root.useLayeredMask
        fillColor: preview.ready && root.preserveAspect ? "transparent" : Theme.surface
        borderWidth: root.preserveAspect ? 0 : Theme.strokeHairline
        borderColor: Theme.borderSubtle
        onStatusChanged: root.reportDecodedAspect()
        onSourceImplicitWidthChanged: root.reportDecodedAspect()
        onSourceImplicitHeightChanged: root.reportDecodedAspect()
        onPaintedWidthChanged: root.reportDecodedAspect()
        onPaintedHeightChanged: root.reportDecodedAspect()
    }

    // Multi-image feed galleries stay on Image's direct scene-graph texture path.
    // Cover only the few corner pixels with a vector inverse instead of allocating
    // a full-size image layer, a full-size mask layer and a MultiEffect per tile.
    Shape {
        anchors.fill: parent
        z: 2
        // Keep the vector geometry resident from the asynchronously incubated
        // tile's first frame. Image completion then changes only the direct
        // texture visibility and cannot trigger another corner-path build wave.
        visible: !root.useLayeredMask
        preferredRendererType: Shape.CurveRenderer
        asynchronous: false

        ShapePath {
            fillColor: root.cornerCoverColor
            fillRule: ShapePath.OddEvenFill
            strokeColor: "transparent"
            strokeWidth: -1

            startX: 0
            startY: 0
            PathLine { x: root.width; y: 0 }
            PathLine { x: root.width; y: root.height }
            PathLine { x: 0; y: root.height }
            PathLine { x: 0; y: 0 }

            PathMove { x: root.cornerRadius; y: 0 }
            PathLine { x: root.width - root.cornerRadius; y: 0 }
            PathCubic {
                x: root.width
                y: root.cornerRadius
                control1X: root.width - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control1Y: 0
                control2X: root.width
                control2Y: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
            }
            PathLine { x: root.width; y: root.height - root.cornerRadius }
            PathCubic {
                x: root.width - root.cornerRadius
                y: root.height
                control1X: root.width
                control1Y: root.height - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control2X: root.width - root.cornerRadius
                           + root.cornerCubicFactor * root.cornerRadius
                control2Y: root.height
            }
            PathLine { x: root.cornerRadius; y: root.height }
            PathCubic {
                x: 0
                y: root.height - root.cornerRadius
                control1X: root.cornerRadius - root.cornerCubicFactor * root.cornerRadius
                control1Y: root.height
                control2X: 0
                control2Y: root.height - root.cornerRadius
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
