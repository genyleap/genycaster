import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

Superellipse {
    id: root

    property string previewText: richComposer.safeText
    property bool compact: false
    property var attachmentMetadata: ({})

    // Keep preview geometry deterministic. Long text/media scrolls inside the
    // preview instead of resizing the composer after asynchronous media work.
    implicitHeight: root.previewMedia.length > 0
                    ? (root.compact ? 350 : 420)
                    : (root.compact ? 220 : 250)

    readonly property string authorName: String(presentationBridge.accountName || presentationBridge.identityName || qsTrId("genycaster.app.genycaster"))
    readonly property string authorUsername: String(presentationBridge.accountUsername || "")
    readonly property string authorAvatar: String(presentationBridge.accountAvatarUrl || "")
    readonly property var previewMedia: {
        const source = richComposer.blocks || []
        const result = []
        for (let index = 0; index < source.length; ++index) {
            if (result.length >= 2)
                break
            const block = source[index] || ({})
            if (String(block.kind || "") !== "media")
                continue
            const url = String(block.url || "").trim()
            if (url.length === 0)
                continue
            const metadata = root.attachmentMetadata && root.attachmentMetadata[url]
                             ? root.attachmentMetadata[url] : ({})
            const localPreview = String(metadata.localPreviewUrl || "").trim()
            const sticker = StickerCatalog.isStickerUrl(url)
            const presentationSource = localPreview.length > 0
                                     ? localPreview
                                     : (sticker || url.startsWith("file:")
                                        || url.startsWith("qrc:") || url.startsWith("image:") ? url : "")
            result.push({
                url: url,
                sticker: sticker,
                previewSource: presentationSource,
                mediaType: String(block.mediaType || metadata.mediaType || ""),
                altText: String(block.altText || "")
            })
        }
        return result
    }
    readonly property string visualPreviewText: {
        const mediaUrls = []
        for (let index = 0; index < root.previewMedia.length; ++index)
            mediaUrls.push(String(root.previewMedia[index].url || "").trim())

        const lines = String(root.previewText || "").split("\n")
        const kept = []
        for (let index = 0; index < lines.length; ++index) {
            const trimmed = String(lines[index] || "").trim()
            if (mediaUrls.indexOf(trimmed) >= 0)
                continue
            kept.push(lines[index])
        }
        return kept.join("\n").trim()
    }

    fillColor: Theme.previewSurface
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer

    ScrollView {
        id: previewViewport
        anchors.fill: parent
        anchors.margins: Theme.space3
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: previewColumn
            width: Math.max(0, previewViewport.availableWidth)
            spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.composer.farcaster_preview")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }

                Label {
                    LayoutMirroring.enabled: false
                    text: LocaleFormat.formatInteger(richComposer.publishedBytes)
                          + " / " + LocaleFormat.formatInteger(richComposer.maximumPublishedBytes)
                    color: richComposer.withinProtocolLimit ? Theme.textTertiary : Theme.danger
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                }
            }

            Superellipse {
                Layout.fillWidth: true
                implicitHeight: castContent.implicitHeight + Theme.space4 * 2
                fillColor: Theme.surfaceRaised
                borderWidth: 1
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: castContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space4
                    spacing: Theme.space3

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2

                        Avatar {
                            avatarSize: root.compact ? 36 : 42
                            sourceUrl: root.authorAvatar
                            displayName: root.authorName
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.authorName
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeMetaSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                LayoutMirroring.enabled: false
                                text: root.authorUsername.length > 0 ? "@" + root.authorUsername + " · now" : "now"
                                color: Theme.textTertiary
                                font.family: Theme.latinFontFamily
                                font.pixelSize: Theme.typeCaptionSize
                                horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                            }
                        }

                        Icon {
                            text: Theme.iconMore
                            color: Theme.textTertiary
                            preferredSize: Theme.iconSize
                        }
                    }

                    RichCastRenderer {
                        Layout.fillWidth: true
                        visible: root.visualPreviewText.length > 0 || root.previewMedia.length === 0
                        plainText: root.visualPreviewText.length > 0
                                    ? root.visualPreviewText
                                    : qsTrId("genycaster.composer.preview_empty")
                        compact: root.compact
                        // Composer surface loops authored emoji; the shared
                        // renderer owns cache, viewport and reduced-motion policy.
                        emojiSurface: 0
                    }

                    GridLayout {
                        visible: root.previewMedia.length > 0
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        columns: root.previewMedia.length > 1 ? 2 : 1
                        columnSpacing: Theme.space1
                        rowSpacing: Theme.space1

                        Repeater {
                            model: root.previewMedia
                            delegate: Item {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: modelData.sticker ? (root.compact ? 156 : 188)
                                    : root.previewMedia.length > 1 ? (root.compact ? 132 : 178)
                                    : (root.compact ? 190 : 250)

                                StickerDraftTile {
                                    visible: Boolean(parent.modelData.sticker)
                                    anchors.centerIn: parent
                                    tileSize: Math.min(parent.width, root.compact ? 148 : 176)
                                    imageUrl: String(parent.modelData.previewSource || "")
                                    altText: String(parent.modelData.altText || "")
                                    removable: false
                                }

                                MediaPreview {
                                    visible: !Boolean(parent.modelData.sticker)
                                    anchors.fill: parent
                                    source: String(parent.modelData.previewSource || "")
                                    mediaType: String(parent.modelData.mediaType || "")
                                    curveDepth: Theme.curveControl
                                    interactive: String(parent.modelData.mediaType || "").startsWith("video/")
                                    primeVideoFrame: false
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.divider
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space5

                        Repeater {
                            model: [Theme.iconReply, Theme.iconShare, Theme.iconHeart]
                            delegate: Icon {
                                required property string modelData
                                text: modelData
                                color: Theme.textTertiary
                                preferredSize: Theme.iconSize
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.space1 }
        }
    }
}
