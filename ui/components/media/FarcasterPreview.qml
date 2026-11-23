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

    // A single scroll owner: the Composer's outer ScrollView. Nested
    // ScrollViews consumed wheel/touch gestures and cut the media in half.
    // Reserve the complete intrinsic Cast, including its fitted image.
    implicitHeight: Math.max(root.compact ? 190 : 245,
                             previewColumn.implicitHeight + Theme.space4)

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
            // This is media the author deliberately attached. A canonical
            // remote URL must also render in the preview (the media gateway
            // handles network policy), not a blank square.
            const presentationSource = localPreview.length > 0 ? localPreview : url
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

    Item {
        id: previewViewport
        anchors.fill: parent
        anchors.margins: Theme.space2

        ColumnLayout {
            id: previewColumn
            width: Math.max(0, previewViewport.width)
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
                                Layout.preferredHeight: modelData.sticker
                                    ? (root.compact ? 156 : 188)
                                    : root.previewMedia.length > 1
                                        ? Math.max(110, Math.min(680,
                                            width / Math.max(0.3, mediaTile.imageAspectRatio)))
                                        : Math.max(160, Math.min(1800,
                                            width / Math.max(0.3, mediaTile.imageAspectRatio)))

                                StickerDraftTile {
                                    visible: Boolean(parent.modelData.sticker)
                                    anchors.centerIn: parent
                                    tileSize: Math.min(parent.width, root.compact ? 148 : 176)
                                    imageUrl: String(parent.modelData.previewSource || "")
                                    altText: String(parent.modelData.altText || "")
                                    removable: false
                                }

                                MediaPreview {
                                    id: mediaTile
                                    visible: !Boolean(parent.modelData.sticker)
                                    anchors.fill: parent
                                    source: String(parent.modelData.previewSource || "")
                                    mediaType: String(parent.modelData.mediaType || "")
                                    // Social previews must show the full authored
                                    // image: no square crop hiding the content.
                                    fitImage: true
                                    minimalChrome: true
                                    showBorder: false
                                    fillColor: "transparent"
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
