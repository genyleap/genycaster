import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

NeonCard {
    id: root
    // Attachments are content, not a stack of framed cards.
    interactive: false
    ambientNeon: false
    neonStrength: 0

    property int blockIndex: -1
    property int maxAttachments: 1
    property bool insertFailed: false
    property var queuedFiles: []
    property var attachmentMetadata: ({})
    property string inspectedImageUrl: ""
    property string inspectedFileName: ""
    property string inspectedMediaType: ""

    function inspectImage(item) {
        if (!String(item.mediaType || "").startsWith("image/")) return
        root.inspectedImageUrl = String(item.previewUrl || item.url || "")
        root.inspectedFileName = String(item.fileName || "")
        root.inspectedMediaType = String(item.mediaType || "")
        if (root.inspectedImageUrl.length > 0) mediaInspector.open()
    }

    signal browseRequested(int blockIndex)
    signal filesDropped(var urls, int blockIndex)
    signal resetFailureRequested()
    signal finished()

    GenyModal {
        id: mediaInspector
        objectName: "composerFullResolutionMediaInspector"
        parent: Overlay.overlay
        width: parent ? Math.min(1080, Math.max(280, parent.width - 28)) : 900
        height: parent ? Math.min(820, Math.max(300, parent.height - 28)) : 710
        onClosed: root.inspectedImageUrl = ""

        contentItem: ColumnLayout {
            spacing: Theme.space2
            RowLayout {
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.inspectedFileName
                    elide: Text.ElideMiddle
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    forceLeftToRight: true
                }
                ActionButton {
                    iconOnly: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconClose
                    Accessible.name: qsTrId("genycaster.shell.close")
                    onClicked: mediaInspector.close()
                }
            }
            MediaPreview {
                Layout.fillWidth: true
                Layout.fillHeight: true
                source: root.inspectedImageUrl
                mediaType: root.inspectedMediaType
                fitImage: true
                fullQualityImage: true
                minimalChrome: true
                interactive: false
                showBorder: false
                curveDepth: Theme.curveContainer
            }
        }
    }

    readonly property var attachedMedia: {
        const source = richComposer.blocks || []
        const result = []
        for (let index = 0; index < source.length; ++index) {
            const block = source[index]
            if (String(block.kind || "") !== "media")
                continue
            const url = String(block.url || "")
            const metadata = root.attachmentMetadata[url] || ({})
            result.push({
                blockIndex: index,
                url: url,
                mediaType: String(block.mediaType || metadata.mediaType || ""),
                altText: String(block.altText || ""),
                fileName: String(metadata.fileName || root.fileNameFromUrl(url)),
                // The selected-file JPEG thumbnail (96px) is used only while
                // transferring. After upload, decode the canonical image.
                previewUrl: url,
                sizeBytes: Number(metadata.sizeBytes || 0),
                originalSizeBytes: Number(metadata.originalSizeBytes || metadata.sizeBytes || 0),
                sticker: StickerCatalog.isStickerUrl(url)
            })
        }
        return result
    }
    readonly property var regularAttachedMedia: root.attachedMedia.filter(item => !item.sticker)
    readonly property bool hasAttachedMedia: regularAttachedMedia.length > 0
    readonly property bool hasTransientUpload: mediaUpload.busy || mediaUpload.canRetry
                                                || mediaUpload.errorText.length > 0
                                                || root.insertFailed
    readonly property bool hasQueue: root.queuedFiles && root.queuedFiles.length > 0

    function formatBytes(value) {
        const bytes = Number(value || 0)
        if (!Number.isFinite(bytes) || bytes <= 0)
            return ""
        const units = ["B", "KB", "MB", "GB"]
        let amount = bytes
        let unit = 0
        while (amount >= 1024 && unit < units.length - 1) {
            amount /= 1024
            ++unit
        }
        const digits = unit === 0 ? 0 : (amount >= 10 ? 1 : 2)
        return LocaleFormat.formatNumber(amount, digits) + " " + units[unit]
    }

    function savingsText(sourceBytes, storedBytes) {
        const source = Number(sourceBytes || 0)
        const stored = Number(storedBytes || 0)
        if (!Number.isFinite(source) || !Number.isFinite(stored)
                || source <= 0 || stored <= 0 || stored >= source) return ""
        return qsTrId("genycaster.media.saved").arg(
            LocaleFormat.formatInteger(Math.round(100 * (source - stored) / source)))
    }

    readonly property int processingStep: {
        const phase = mediaUpload.phase
        if (phase === "uploading" || phase === "authorizing"
                || phase === "queued") return 1
        if (phase === "optimizing" || phase === "finalizing"
                || phase === "verifying") return 2
        return 0 // Preparing/Compressing happens locally, before network.
    }

    function fileNameFromUrl(rawUrl) {
        const value = String(rawUrl || "")
        if (value.length === 0)
            return qsTrId("genycaster.composer.media")
        const withoutQuery = value.split("?")[0].split("#")[0]
        const slash = withoutQuery.lastIndexOf("/")
        const name = slash >= 0 ? withoutQuery.substring(slash + 1) : withoutQuery
        return name.length > 0 ? name : value
    }

    function queueFileName(value) {
        const url = value && value.toString ? value.toString() : String(value || "")
        return root.fileNameFromUrl(url)
    }

    function mediaPhaseText(phase) {
        if (phase === "compressing") return qsTrId("genycaster.media.compressing_local")
        if (phase === "optimizing") return qsTrId("genycaster.media.optimizing")
        if (phase === "finalizing") return qsTrId("genycaster.media.finalizing")
        if (phase === "queued") return qsTrId("genycaster.media.queued")
        if (phase === "preparing") return qsTrId("genycaster.media.preparing")
        if (phase === "authorizing") return qsTrId("genycaster.media.authorizing")
        if (phase === "uploading") return qsTrId("genycaster.media.uploading")
        if (phase === "verifying") return qsTrId("genycaster.media.verifying")
        if (phase === "cancelling") return qsTrId("genycaster.media.cancelling")
        if (phase === "cancelled") return qsTrId("genycaster.media.cancelled")
        if (phase === "completed") return qsTrId("genycaster.media.completed")
        return qsTrId("genycaster.media.upload_failed")
    }

    fillColor: "transparent"
    borderWidth: 0
    borderColor: "transparent"
    curveDepth: Theme.curveLarge
    implicitHeight: contentColumn.implicitHeight

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 0
        spacing: Theme.space2

        RowLayout {
            visible: !root.hasAttachedMedia || root.hasTransientUpload || root.hasQueue
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Icon {
                text: Theme.iconImage
                color: Theme.ink
                preferredSize: Theme.typeLabelSize
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.composer.media")
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }

            DirectionAwareLabel {
                visible: root.hasAttachedMedia
                text: LocaleFormat.formatInteger(root.attachedMedia.length) + " / "
                      + LocaleFormat.formatInteger(root.maxAttachments)
                color: root.attachedMedia.length >= root.maxAttachments
                    ? Theme.proAccent : Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }

            DirectionAwareLabel {
                visible: root.hasQueue
                text: LocaleFormat.formatInteger(root.queuedFiles.length)
                      + " · " + qsTrId("genycaster.media.queued")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }

            ActionButton {
                visible: !mediaUpload.busy
                quiet: true
                compact: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                Accessible.name: qsTrId("genycaster.shell.close")
                onClicked: root.finished()
            }
        }

        Item {
            id: dropTarget
            visible: !root.hasAttachedMedia && !root.hasTransientUpload
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? (root.width < 620 ? 100 : 112) : 0

            Superellipse {
                anchors.fill: parent
                fillColor: dropArea.containsDrag ? Theme.primarySurface : Theme.surfaceMuted
                borderWidth: 0
                curveDepth: Theme.curveLarge
                Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
            }

            VectorStroke {
                qualityScale: renderingController.geometryQualityScale
                anchors.fill: parent
                pathKind: VectorStroke.RoundedRectangle
                strokeColor: dropArea.containsDrag ? Theme.accent : Theme.borderStrong
                strokeWidth: dropArea.containsDrag ? 1.6 : 1.15
                cornerRadius: Theme.curveLarge
                dashLength: 8
                gapLength: 6
                inset: 1
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - Theme.space6, 520)
                spacing: root.hasAttachedMedia ? Theme.space1 : Theme.space2

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: Theme.iconUpload
                    color: dropArea.containsDrag ? Theme.accent : Theme.ink
                    preferredSize: root.hasAttachedMedia ? Theme.typeBodySize : Theme.typeTitle3Size
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.media.choose_file")
                    color: Theme.ink
                    font.pixelSize: root.hasAttachedMedia ? Theme.typeLabelSize : Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: mediaUpload.available
                        ? qsTrId("genycaster.media.choose_file_hint")
                        : qsTrId("genycaster.media.upload_unavailable")
                    color: mediaUpload.available ? Theme.inkMuted : Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: root.hasAttachedMedia ? 1 : 2
                }
            }

            DropArea {
                id: dropArea
                anchors.fill: parent
                enabled: mediaUpload.available && !mediaUpload.busy
                    && root.attachedMedia.length + root.queuedFiles.length < root.maxAttachments

                onDropped: drop => {
                    if (!drop.hasUrls || drop.urls.length === 0) {
                        drop.accepted = false
                        return
                    }
                    root.resetFailureRequested()
                    root.filesDropped(drop.urls, root.blockIndex)
                    drop.acceptProposedAction()
                }
            }

            TapHandler {
                enabled: mediaUpload.available && !mediaUpload.busy
                    && root.attachedMedia.length + root.queuedFiles.length < root.maxAttachments
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.browseRequested(root.blockIndex)
            }
        }

        Superellipse {
            objectName: "responsiveComposerUploadStatusCard"
            visible: root.hasTransientUpload
            Layout.fillWidth: true
            Layout.preferredHeight: uploadContent.implicitHeight + Theme.space3 * 2
            fillColor: Theme.surfaceMuted
            borderWidth: 1
            borderColor: root.insertFailed || mediaUpload.errorText.length > 0
                ? Theme.warning : Theme.borderMuted
            curveDepth: Theme.curveControl

            ColumnLayout {
                id: uploadContent
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space2

                // Filename and actions share a compact header; progress and
                // errors are full-width rows below, never squeezed beside a
                // 126px thumbnail or a long filename on mobile.
                RowLayout {
                    id: uploadHeader
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    MediaPreview {
                        visible: mediaUpload.localPreviewUrl.toString().length > 0
                        Layout.preferredWidth: root.width < 620 ? 64 : 88
                        Layout.preferredHeight: root.width < 620 ? 64 : 80
                        fitImage: true
                        source: mediaUpload.localPreviewUrl
                        mediaType: mediaUpload.selectedContentType
                        curveDepth: Theme.curveControl
                        interactive: false
                    }

                    Superellipse {
                        visible: mediaUpload.localPreviewUrl.toString().length === 0
                        Layout.preferredWidth: root.width < 620 ? 48 : 64
                        Layout.preferredHeight: root.width < 620 ? 48 : 64
                        fillColor: Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconFile
                            color: Theme.inkMuted
                            preferredSize: Theme.typeBodySize
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            objectName: "composerUploadingFileName"
                            Layout.fillWidth: true
                            text: mediaUpload.fileName.length > 0
                                ? mediaUpload.fileName : qsTrId("genycaster.composer.media")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideMiddle
                        }
                        DirectionAwareLabel {
                            visible: mediaUpload.totalBytes > 0
                            Layout.fillWidth: true
                            text: root.formatBytes(mediaUpload.totalBytes)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                        }
                    }

                    ActionButton {
                        visible: mediaUpload.busy
                        quiet: true
                        compact: true
                        iconOnly: true
                        iconGlyph: Theme.iconClose
                        Accessible.name: qsTrId("genycaster.media.cancel")
                        onClicked: mediaUpload.cancel()
                    }
                    ActionButton {
                        visible: mediaUpload.canRetry && !mediaUpload.busy
                        quiet: true
                        compact: true
                        iconOnly: true
                        iconGlyph: Theme.iconRefresh
                        Accessible.name: qsTrId("genycaster.media.retry")
                        onClicked: mediaUpload.retry()
                    }
                }

                DirectionAwareLabel {
                    objectName: "composerUploadFullWidthStatus"
                    Layout.fillWidth: true
                    text: root.insertFailed ? qsTrId("genycaster.media.insert_failed")
                        : mediaUpload.errorText.length > 0 ? mediaUpload.errorText
                        : root.mediaPhaseText(mediaUpload.phase)
                    color: root.insertFailed || mediaUpload.errorText.length > 0
                        ? Theme.warning : Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }

                GridLayout {
                    objectName: "smartMediaPipelineProgressSteps"
                    visible: mediaUpload.busy
                        && String(mediaUpload.selectedContentType || "").startsWith("image/")
                    Layout.fillWidth: true
                    columns: root.width < 480 ? 1 : 3
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space1
                    Repeater {
                        model: [
                            qsTrId("genycaster.media.step_compressing"),
                            qsTrId("genycaster.media.step_uploading"),
                            qsTrId("genycaster.media.step_finalizing")
                        ]
                        delegate: RowLayout {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space1
                            Superellipse {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                curveDepth: Theme.curveTiny
                                fillColor: index <= root.processingStep
                                    ? Theme.accent : Theme.borderStrong
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: modelData
                                color: index === root.processingStep
                                    ? Theme.ink : Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: index === root.processingStep
                                    ? Theme.weightDemiBold : Theme.weightRegular
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    visible: mediaUpload.busy && mediaUpload.phase !== "uploading"
                    spacing: Theme.space2
                    GenySpinner {
                        running: visible
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Accessible.ignored: true
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.mediaPhaseText(mediaUpload.phase)
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                }

                Superellipse {
                    // Show progress only for measured network bytes.
                    visible: mediaUpload.busy && mediaUpload.phase === "uploading"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    fillColor: Theme.borderMuted
                    curveDepth: 2
                    Superellipse {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * Math.max(0.015, mediaUpload.progress)
                        fillColor: Theme.accent
                        curveDepth: 2
                    }
                }
            }
        }

        ColumnLayout {
            visible: root.hasAttachedMedia
            Layout.fillWidth: true
            spacing: Theme.space1

            // Each upload has its own bounded card: filename, size, remove
            // action and fitted media. Grid cells share dimensions regardless
            // of photo aspect ratio or a long filename.
            GridLayout {
                id: attachmentGrid
                objectName: "composerAttachmentGrid"
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                columns: root.width >= 535 ? 2 : 1
                columnSpacing: Theme.space2
                rowSpacing: Theme.space2

                Repeater {
                    model: root.regularAttachedMedia

                    delegate: Superellipse {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredHeight: root.width >= 535 ? 225 : 200
                        fillColor: Theme.surfaceMuted
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveControl

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            spacing: Theme.space1

                            RowLayout {
                                layoutDirection: Theme.uiDirection
                                Layout.fillWidth: true
                                spacing: Theme.space2

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 0

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightDemiBold
                                        color: Theme.ink
                                        elide: Text.ElideMiddle
                                        forceLeftToRight: true
                                        text: String(modelData.fileName || "")
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        font.pixelSize: Theme.typeCaptionSize
                                        color: Theme.inkMuted
                                        elide: Text.ElideRight
                                        forceLeftToRight: true
                                        text: root.savingsText(modelData.originalSizeBytes,
                                                              modelData.sizeBytes).length > 0
                                            ? root.formatBytes(modelData.sizeBytes) + " · "
                                              + root.savingsText(modelData.originalSizeBytes,
                                                                 modelData.sizeBytes)
                                            : root.formatBytes(modelData.sizeBytes)
                                    }
                                }

                                ActionButton {
                                    compact: true
                                    quiet: true
                                    iconOnly: true
                                    iconGlyph: Theme.iconClose
                                    text: qsTrId("genycaster.composer.remove_block")
                                    Accessible.name: qsTrId("genycaster.composer.remove_block")
                                    onClicked: richComposer.removeBlock(Number(modelData.blockIndex))
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                MediaPreview {
                                    id: attachedPreview
                                    anchors.fill: parent
                                    source: modelData.previewUrl
                                    mediaType: modelData.mediaType
                                    fitImage: true
                                    fullQualityImage: String(modelData.mediaType || "").startsWith("image/")
                                    minimalChrome: true
                                    showBorder: false
                                    fillColor: "transparent"
                                    interactive: String(modelData.mediaType || "").startsWith("video/")
                                    primeVideoFrame: false
                                }

                                MouseArea {
                                    anchors.fill: attachedPreview
                                    enabled: String(modelData.mediaType || "").startsWith("image/")
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.inspectImage(modelData)
                                }
                            }
                        }
                    }
                }

                Superellipse {
                    visible: mediaUpload.available && !mediaUpload.busy
                        && root.attachedMedia.length + root.queuedFiles.length < root.maxAttachments
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: root.width >= 535 ? 225 : 200
                    fillColor: Theme.surfaceMuted
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveControl

                    ActionButton {
                        anchors.centerIn: parent
                        compact: true
                        quiet: true
                        iconGlyph: Theme.iconPlus
                        text: qsTrId("genycaster.media.choose_file")
                        Accessible.name: qsTrId("genycaster.media.choose_file")
                        onClicked: root.browseRequested(root.blockIndex)
                    }
                }
            }
        }
    }
}
