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
            visible: root.hasTransientUpload
            Layout.fillWidth: true
            implicitHeight: uploadRow.implicitHeight + Theme.space3 * 2
            fillColor: Theme.surfaceMuted
            borderWidth: 1
            borderColor: root.insertFailed || mediaUpload.errorText.length > 0
                ? Theme.warning : Theme.borderMuted
            curveDepth: Theme.curveControl

            RowLayout {
                id: uploadRow
                anchors.fill: parent
                anchors.margins: Theme.space3
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3

                MediaPreview {
                    visible: mediaUpload.localPreviewUrl.toString().length > 0
                    Layout.preferredWidth: root.width < 620 ? 126 : 92
                    Layout.preferredHeight: root.width < 620 ? 112 : 86
                    fitImage: true
                    source: mediaUpload.localPreviewUrl
                    mediaType: mediaUpload.selectedContentType
                    curveDepth: Theme.curveControl
                    interactive: mediaUpload.previewKind === "video"
                }

                Superellipse {
                    visible: mediaUpload.localPreviewUrl.toString().length === 0
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
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
                    spacing: Theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2

                        DirectionAwareLabel {
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
                            text: root.formatBytes(mediaUpload.totalBytes)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.insertFailed
                            ? qsTrId("genycaster.media.insert_failed")
                            : mediaUpload.errorText.length > 0
                              ? mediaUpload.errorText
                              : root.mediaPhaseText(mediaUpload.phase)
                        color: root.insertFailed || mediaUpload.errorText.length > 0
                            ? Theme.warning : Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        objectName: "smartMediaPipelineProgressSteps"
                        visible: mediaUpload.busy
                            && String(mediaUpload.selectedContentType || "").startsWith("image/")
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Repeater {
                            // Concise stage titles fit narrow mobile composers;
                            // the full, localized description is above.
                            model: [
                                qsTrId("genycaster.media.step_compressing"),
                                qsTrId("genycaster.media.step_uploading"),
                                qsTrId("genycaster.media.step_finalizing")
                            ]
                            delegate: RowLayout {
                                required property int index
                                required property string modelData
                                Layout.fillWidth: true
                                spacing: Theme.space1

                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: index <= root.processingStep
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
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        GenySpinner {
                            visible: mediaUpload.phase === "compressing"
                                || mediaUpload.phase === "preparing"
                                || mediaUpload.phase === "optimizing"
                                || mediaUpload.phase === "finalizing"
                            running: visible && mediaUpload.busy
                            Layout.preferredWidth: visible ? 18 : 0
                            Layout.preferredHeight: visible ? 18 : 0
                            Accessible.ignored: true
                        }
                    }

                    Superellipse {
                        // Network bytes are measurable; backend conversion
                        // and storage are not. Never show a fake 100% upload
                        // bar while waiting on QStorage finalization.
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
        }

        ColumnLayout {
            visible: root.hasAttachedMedia
            Layout.fillWidth: true
            spacing: Theme.space1

            // One media strip is the canonical attachment presentation.
            // No duplicate "Image / Media" document card, framed metadata
            // row, thumbnail-only preview or on-image size badge.
            Flickable {
                objectName: "composerOriginalImageStrip"
                Layout.fillWidth: true
                // Compact source-quality tray: the file is an attachment,
                // not a second full-height editor. The inspector opens on tap.
                Layout.preferredHeight: root.width < 620 ? 216 : 248
                clip: true
                contentWidth: attachmentStrip.width
                contentHeight: height
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: attachmentStrip
                    height: parent.height
                    spacing: Theme.space3

                    Repeater {
                        model: root.regularAttachedMedia

                        delegate: Item {
                            required property var modelData
                            width: Math.max(180, Math.min(root.width - 8,
                                root.width < 620 ? 410 : 500))
                            height: attachmentStrip.height

                            MediaPreview {
                                id: attachedPreview
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: parent.height - 48
                                // The attached URL is already the canonical
                                // published image. Decode original pixels,
                                // never the transient 96px selection JPEG.
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

                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 42
                                spacing: Theme.space1

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: String(modelData.fileName || "")
                                    font.pixelSize: Theme.typeCaptionSize
                                    color: Theme.ink
                                    elide: Text.ElideMiddle
                                    forceLeftToRight: true
                                }

                                DirectionAwareLabel {
                                    visible: Number(modelData.sizeBytes || 0) > 0
                                    text: root.savingsText(modelData.originalSizeBytes,
                                                          modelData.sizeBytes).length > 0
                                        ? root.formatBytes(modelData.originalSizeBytes)
                                          + " → " + root.formatBytes(modelData.sizeBytes)
                                          + " · " + root.savingsText(
                                                modelData.originalSizeBytes, modelData.sizeBytes)
                                        : root.formatBytes(modelData.sizeBytes)
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    Layout.maximumWidth: Math.max(0, Math.min(138, parent.width * 0.34))
                                    elide: Text.ElideRight
                                    forceLeftToRight: true
                                }

                                ActionButton {
                                    // Reserve real layout width for Remove: the
                                    // control must never get clipped by filename.
                                    Layout.preferredWidth: Theme.compactControlHeight
                                    Layout.minimumWidth: Theme.compactControlHeight
                                    Layout.preferredHeight: Theme.compactControlHeight
                                    quiet: true
                                    compact: true
                                    iconOnly: true
                                    iconGlyph: Theme.iconClose
                                    text: qsTrId("genycaster.composer.remove_block")
                                    onClicked: richComposer.removeBlock(Number(modelData.blockIndex))
                                }
                            }
                        }
                    }

                    Item {
                        visible: mediaUpload.available && !mediaUpload.busy
                            && root.attachedMedia.length + root.queuedFiles.length < root.maxAttachments
                        width: visible ? Theme.compactControlHeight + Theme.space2 : 0
                        height: parent.height

                        ActionButton {
                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.topMargin: Theme.space2
                            quiet: true
                            compact: true
                            iconOnly: true
                            iconGlyph: Theme.iconPlus
                            text: qsTrId("genycaster.media.choose_file")
                            onClicked: root.browseRequested(root.blockIndex)
                        }
                    }
                }
            }
        }
    }
}
