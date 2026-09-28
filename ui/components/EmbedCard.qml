import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
    id: root

    property var embed
    property bool scrollMoving: false
    readonly property bool isCastReference: root.embed && root.embed.kind === "cast-reference"
    readonly property bool isImage: root.embed && root.embed.kind === "image"
    readonly property bool isVideo: root.embed && root.embed.kind === "video"
    readonly property bool isExternal: !root.isCastReference
    readonly property bool hasPreviewImage: root.isExternal && Boolean(root.embed.imageSource)
    readonly property string hostText: String(root.embed.host || "")
    readonly property string quotedName: String(root.embed.displayName || root.embed.author || "")
    readonly property bool quoteResolved: Boolean(root.embed.resolved)
                                          || Boolean(root.embed.text)
                                          || Boolean(root.embed.author)
                                          || Boolean(root.embed.displayName)
                                          || Boolean(root.embed.avatarUrl)
                                          || Number(root.embed.createdAtMs || 0) > 0
                                          || (root.embed.embeds || []).length > 0
    property bool quoteHydrationTimedOut: false
    readonly property bool quotedRtl: localizationController.isRightToLeftText(root.embed.text || root.quotedName)
    onEmbedChanged: root.quoteHydrationTimedOut = false

    signal openRequested(string url)

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    signal castRequested(string castId)

    function ltrText(value) {
        const isolate = String.fromCharCode(0x2066)
        const pop = String.fromCharCode(0x2069)
        return isolate + String(value || "") + pop
    }

    function declaredMediaAspect() {
        const ratio = Number(root.embed.aspectRatio || 0)
        if (ratio > 0)
            return ratio
        const width = Number(root.embed.width || 0)
        const height = Number(root.embed.height || 0)
        return width > 0 && height > 0 ? width / height : 0
    }

    function quotedPreviewMedia(values) {
        const embeds = values || []
        for (let index = 0; index < embeds.length; ++index) {
            const item = embeds[index] || ({})
            const kind = String(item.kind || "").toLowerCase()
            const source = String(item.imageSource || item.previewImage || item.artwork || "")
            if ((kind === "image" || kind === "video" || kind === "collectible" || kind === "nft")
                    && source.length > 0)
                return { source: source, kind: kind }
        }
        return ({ source: "", kind: "" })
    }

    readonly property var quotedMedia: root.quotedPreviewMedia(root.embed.embeds || [])

    Timer {
        id: quoteHydrationDeadline
        interval: 4500
        repeat: false
        running: root.isCastReference && !root.quoteResolved && !root.quoteHydrationTimedOut
        onTriggered: root.quoteHydrationTimedOut = true
    }

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Theme.space2 * 2
    curveDepth: Theme.shapeMedia
    fillColor: !root.scrollMoving && hover.hovered ? Theme.surfaceHover : Theme.surfaceMuted
    borderWidth: 1
    borderColor: Theme.borderMuted
    Behavior on fillColor {
        enabled: !root.scrollMoving
        ColorAnimation { duration: Theme.motionFast }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true

        RowLayout {
            visible: root.isCastReference
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: root.quotedRtl ? Qt.RightToLeft : Qt.LeftToRight

            Avatar {
                Layout.alignment: Qt.AlignTop
                avatarSize: 30
                sourceUrl: root.embed.avatarUrl || ""
                displayName: root.quotedName.length > 0 ? root.quotedName : qsTrId("genycaster.embed.quoted_cast")
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    layoutDirection: root.quotedRtl ? Qt.RightToLeft : Qt.LeftToRight

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.quotedName.length > 0 ? root.quotedName : qsTrId("genycaster.embed.quoted_cast")
                        color: Theme.ink
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: Boolean(root.embed.author)
                        LayoutMirroring.enabled: false
                        text: root.ltrText("@" + String(root.embed.author || ""))
                        font.family: Theme.latinFontFamily
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }
                }

                DirectionAwareLabel {
                    visible: Boolean(root.embed.text)
                    Layout.fillWidth: true
                    text: root.embed.text || ""
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    visible: !root.quoteResolved
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    layoutDirection: root.quotedRtl ? Qt.RightToLeft : Qt.LeftToRight

                    GenySpinner {
                        visible: !root.quoteHydrationTimedOut
                        Layout.preferredWidth: visible ? 14 : 0
                        Layout.preferredHeight: visible ? 14 : 0
                        running: visible
                        strokeWidth: 1.6
                        color: Theme.inkMuted
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.quoteHydrationTimedOut
                              ? qsTrId("genycaster.embed.open_quoted_cast")
                              : qsTrId("genycaster.embed.loading_quoted_cast")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeMetaSize
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    visible: Number(root.embed.replyCount || 0) > 0
                             || Number(root.embed.recastCount || 0) > 0
                             || Number(root.embed.likeCount || 0) > 0
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    layoutDirection: Qt.LeftToRight

                    RowLayout {
                        visible: Number(root.embed.replyCount || 0) > 0
                        spacing: 3
                        layoutDirection: Qt.LeftToRight
                        Icon { text: Theme.iconReply; color: Theme.inkSubtle; preferredSize: 12 }
                        Label { text: LocaleFormat.formatInteger(Number(root.embed.replyCount || 0)); color: Theme.inkSubtle; font.pixelSize: Theme.typeCaptionSize }
                    }
                    RowLayout {
                        visible: Number(root.embed.recastCount || 0) > 0
                        spacing: 3
                        layoutDirection: Qt.LeftToRight
                        Icon { text: Theme.iconRepost; color: Theme.inkSubtle; preferredSize: 12 }
                        Label { text: LocaleFormat.formatInteger(Number(root.embed.recastCount || 0)); color: Theme.inkSubtle; font.pixelSize: Theme.typeCaptionSize }
                    }
                    RowLayout {
                        visible: Number(root.embed.likeCount || 0) > 0
                        spacing: 3
                        layoutDirection: Qt.LeftToRight
                        Icon { text: Theme.iconHeart; color: Theme.inkSubtle; preferredSize: 12 }
                        Label { text: LocaleFormat.formatInteger(Number(root.embed.likeCount || 0)); color: Theme.inkSubtle; font.pixelSize: Theme.typeCaptionSize }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            Item {
                visible: String(root.quotedMedia.source || "").length > 0
                Layout.preferredWidth: visible ? 92 : 0
                Layout.preferredHeight: visible ? 68 : 0
                Layout.alignment: Qt.AlignVCenter

                SuperellipseImage {
                    anchors.fill: parent
                    source: root.quotedMedia.source || ""
                    mediaCategory: "photos"
                    fillMode: Image.PreserveAspectCrop
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    curveDepth: Theme.shapeMedia
                    showLoadingIndicator: false
                    showFailureIndicator: false
                }

                Superellipse {
                    visible: root.quotedMedia.kind === "video"
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    fillColor: Qt.rgba(0, 0, 0, 0.58)
                    curveDepth: 12
                    Icon { anchors.centerIn: parent; text: Theme.iconPlay; color: "white"; preferredSize: 12 }
                }
            }

            Icon {
                text: root.quotedRtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                color: Theme.inkSubtle
                preferredSize: Theme.typeCaptionSize
            }
        }

        Item {
            visible: root.isExternal && (root.isImage || root.isVideo)
            Layout.fillWidth: true
            Layout.preferredHeight: {
                if (!visible)
                    return 0
                const aspect = root.declaredMediaAspect()
                if (aspect > 0)
                    return Math.min(420, Math.max(180, width / aspect))
                // Never switch geometry after image decode. Unknown media uses a
                // deterministic presentation slot so the parent feed delegate
                // cannot jump when pixels arrive.
                return root.isVideo ? 198 : 190
            }

            SuperellipseImage {
                id: imagePreview
                anchors.fill: parent
                visible: root.isImage
                source: root.isImage && root.hasPreviewImage ? (root.embed.imageSource || "") : ""
                mediaCategory: "photos"
                fillMode: Image.PreserveAspectFit
                curveDepth: Theme.shapeMedia
                fillColor: Theme.dark ? Theme.nightCanvas : Theme.dayCanvas
                borderWidth: 0
            }

            ColumnLayout {
                visible: root.isImage && root.hasPreviewImage && imagePreview.loadFailed
                anchors.centerIn: parent
                width: Math.min(parent.width - Theme.space6 * 2, 320)
                spacing: Theme.space2

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: Theme.iconImage
                    color: Theme.inkMuted
                    preferredSize: Theme.typeTitle2Size
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.embed.media_unavailable")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            GenyVideoPlayer {
                visible: root.isVideo
                anchors.fill: parent
                source: root.isVideo ? (root.embed.url || "") : ""
                adaptiveSource: root.isVideo ? (root.embed.adaptiveUrl || "") : ""
                contentType: root.isVideo ? (root.embed.contentType || "") : ""
                renditions: root.isVideo ? (root.embed.renditions || []) : []
                posterSource: root.hasPreviewImage ? (root.embed.imageSource || "") : ""
                mediaLabel: root.hostText.length > 0
                            ? root.hostText : qsTrId("genycaster.embed.video")
                curveDepth: Theme.shapeMedia
                cornerMaskColor: root.fillColor
                compact: true
                openOriginalEnabled: true
                openOriginalUrl: root.embed.url || ""
                onOpenOriginalRequested: url => root.openRequested(url)
            }
        }

        RowLayout {
            visible: root.isExternal
            Layout.fillWidth: true
            LayoutMirroring.enabled: false
            spacing: Theme.space2
            layoutDirection: Qt.LeftToRight

            Superellipse {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveCompact

                Icon {
                    anchors.centerIn: parent
                    text: root.isVideo ? Theme.iconPlay : root.isImage ? Theme.iconShare : Theme.iconLink
                    color: Theme.accent
                    preferredSize: Theme.typeCaptionSize
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    forceLeftToRight: true
                    text: root.hostText.length > 0
                          ? root.hostText
                          : (root.isImage ? qsTrId("genycaster.embed.image")
                             : root.isVideo ? qsTrId("genycaster.embed.video")
                               : qsTrId("genycaster.embed.external_link"))
                    color: Theme.ink
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    forceLeftToRight: true
                    text: root.ltrText(root.embed.url || "")
                    font.family: Theme.latinFontFamily
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideMiddle
                }
            }

            Icon {
                text: Theme.iconArrowUpRight
                color: Theme.inkSubtle
                preferredSize: Theme.typeCaptionSize
            }
        }
    }

    HoverHandler {
        id: hover
        enabled: !root.scrollMoving
    }
    TapHandler {
        enabled: !root.isVideo
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            if (root.isCastReference)
                root.castRequested(root.embed.castId || "")
            else
                root.openRequested(root.embed.url || "")
        }
    }

    Accessible.role: Accessible.Link
    Accessible.name: root.isCastReference
                     ? qsTrId("genycaster.embed.quoted_cast")
                     : root.isVideo ? qsTrId("genycaster.embed.play_video")
                       : root.isImage ? qsTrId("genycaster.embed.image_embed")
                         : qsTrId("genycaster.embed.open_link")
    Accessible.description: root.isCastReference
                            ? qsTrId("genycaster.embed.view_quoted_cast_next")
                            : root.isImage
                              ? qsTrId("genycaster.embed.open_original_image_external")
                              : root.isVideo
                                ? qsTrId("genycaster.embed.video")
                                : qsTrId("genycaster.embed.open_safely_in_your_default_app_external")
}
