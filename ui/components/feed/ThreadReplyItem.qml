import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

/**
 * @brief Compact recursive conversation row used by cast detail threads.
 *
 * Keeps each reply inside the detail column while exposing the same interaction
 * contract as a feed cast. Nesting is visually capped by the supplied data.
 */
ColumnLayout {
    id: root

    property var reply: ({})
    property int depth: 1
    property bool compact: false
    property bool interactionsAvailable: false
    property bool enrichmentActive: true
    property bool scrollMoving: false
    property double accountFid: 0
    property bool highlighted: false
    property bool parentAuthoredByMe: false
    readonly property bool replyToMyCast: root.parentAuthoredByMe && !root.ownReply

    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()
    signal removeCastRequested(var cast)

    readonly property bool ownReply: Number(root.reply.authorFid || 0) > 0
                                     && Number(root.reply.authorFid || 0) === Number(root.accountFid)
                                     && String(root.reply.castId || "").length > 0

    function openReplyMenu() {
        replyMenuLoader.active = true
        const menu = replyMenuLoader.item
        if (!menu) return
        const point = replyMoreButton.mapToItem(Overlay.overlay, 0, replyMoreButton.height + Theme.space1)
        menu.x = Math.max(Theme.space2, Math.min(Overlay.overlay.width - menu.width - Theme.space2,
                                                point.x + replyMoreButton.width - menu.width))
        menu.y = Math.max(Theme.space2, Math.min(Overlay.overlay.height - menu.height - Theme.space2, point.y))
        menu.open()
    }

    function embedFamily(embed) {
        const kind = String(embed && embed.kind ? embed.kind : "").toLowerCase()
        if (kind === "image" || kind === "video" || kind === "audio" || kind === "file")
            return kind
        if (typeof mediaClassifier !== "undefined" && mediaClassifier !== null) {
            const classified = mediaClassifier.classify(embed || {}, {})
            const family = String(classified.family || "web")
            if (family === "image" || family === "video" || family === "audio" || family === "file")
                return family
        }
        return kind === "cast-reference" || kind === "collectible" || kind === "nft"
               ? kind : "link"
    }

    function isVisualImage(embed) {
        return root.embedFamily(embed) === "image"
    }

    readonly property var visualEmbeds: {
        const values = root.reply.embeds || []
        const result = []
        for (let index = 0; index < values.length; ++index) {
            if (root.isVisualImage(values[index]))
                result.push(values[index])
        }
        return result
    }

    readonly property var supportingEmbeds: {
        const values = root.reply.embeds || []
        const result = []
        for (let index = 0; index < values.length; ++index) {
            if (!root.isVisualImage(values[index]))
                result.push(values[index])
        }
        return result
    }

    readonly property var protocolEmbedUrls: {
        const values = root.reply.embeds || []
        const result = []
        for (let index = 0; index < values.length; ++index) {
            const url = String(values[index].url || values[index].value || "")
            if (url.length > 0)
                result.push(url)
        }
        return result
    }

    Layout.fillWidth: true
    spacing: Theme.space2

    DirectionAwareLabel {
        visible: root.highlighted
        Layout.fillWidth: true
        text: qsTrId("genycaster.detail.your_confirmed_reply")
        color: Theme.accent
        font.pixelSize: Theme.typeCaptionSize
        font.weight: Theme.weightBold
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space1
        layoutDirection: Theme.uiDirection

        GenyThreadRail {
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.minimumHeight: replyCard.implicitHeight
            compact: root.compact
            depth: root.depth
            highlighted: root.highlighted
        }

        Superellipse {
            id: replyCard
            Layout.fillWidth: true
            implicitHeight: replyContent.implicitHeight + (root.compact ? Theme.space3 : Theme.space4) * 2
            // Replies from other accounts on my own cast deserve the accent
            // treatment. My own replies should not monopolize emphasis.
            // Timeline replies share the page canvas. Ownership and selection
            // use a restrained edge, not a full purple highlighted tile.
            fillColor: Theme.backgroundPrimary
            // All conversation replies share one unobtrusive surface.
            // Notification emphasis lives in the label, not a green outline.
            borderWidth: 0
            borderColor: "transparent"
            curveDepth: Theme.curveModal

            ColumnLayout {
                id: replyContent
                anchors.fill: parent
                anchors.margins: root.compact ? Theme.space3 : Theme.space4
                spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                Avatar {
                    avatarSize: root.compact ? 40 : 44
                    sourceUrl: String(root.reply.avatarUrl || "")
                    displayName: String(root.reply.displayName || root.reply.author || "")
                    TapHandler {
                        enabled: Number(root.reply.authorFid || 0) > 0
                        onTapped: root.profileRequested(Number(root.reply.authorFid || 0))
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(root.reply.displayName || root.reply.author || qsTrId("genycaster.notifications.someone"))
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }

                    SourceClientBadge {
                        objectName: "replySourceClientBadge"
                        clientName: String(root.reply.sourceClient || "")
                    }

                    DirectionAwareLabel {
                        visible: Number(root.reply.createdAtMs || 0) > 0
                        Layout.fillWidth: true
                        text: LocaleFormat.formatTimestamp(Number(root.reply.createdAtMs || 0))
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }

                ActionButton {
                    id: replyMoreButton
                    objectName: "replyMoreButton"
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconMore
                    text: qsTrId("genycaster.cast.view_conversation")
                    onClicked: root.openReplyMenu()
                }

                Icon {
                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    color: Theme.textTertiary
                    preferredSize: 10
                    TapHandler {
                        enabled: String(root.reply.castId || "").length > 0
                        onTapped: root.castRequested(String(root.reply.castId || ""))
                    }
                }
            }

            RichCastRenderer {
                Layout.fillWidth: true
                castId: String(root.reply.castId || "")
                plainText: String(root.reply.text || "")
                compact: true
                suppressedEmbedUrls: root.protocolEmbedUrls
                onExternalRequested: url => root.externalRequested(url)
                onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                onCastRequested: castId => root.castRequested(castId)
            }

            CastEmbedsPanel {
                visible: root.visualEmbeds.length > 0 || root.supportingEmbeds.length > 0
                Layout.fillWidth: true
                compact: true
                visualEmbeds: root.visualEmbeds
                supportingEmbeds: root.supportingEmbeds
                onExternalRequested: url => root.externalRequested(url)
                onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                onCastRequested: castId => root.castRequested(castId)
            }

            EngagementBar {
                Layout.fillWidth: true
                compact: root.compact
                showViews: true
                enrichmentActive: root.enrichmentActive
                scrollMoving: root.scrollMoving
                replyCount: Number(root.reply.replyCount || 0)
                recastCount: Number(root.reply.recastCount || 0)
                likeCount: Number(root.reply.likeCount || 0)
                viewCount: root.reply.viewCount === undefined ? null : root.reply.viewCount
                castId: String(root.reply.castId || "")
                castData: root.reply
                authorFid: Number(root.reply.authorFid || 0)
                castText: String(root.reply.text || "")
                contextText: String(root.reply.text || "")
                replyInteractive: root.interactionsAvailable && Boolean(root.reply.castId)
                recastInteractive: replyInteractive
                likeInteractive: replyInteractive
                recastedByMe: Boolean(root.reply.recastedByMe || false)
                likedByMe: Boolean(root.reply.likedByMe || false)
                onReplyRequested: root.replyRequested(root.reply)
                onQuoteRequested: root.quoteRequested(root.reply)
                onRecastRequested: active => root.reactionRequested("recast", root.reply, active)
                onLikeRequested: active => root.reactionRequested("like", root.reply, active)
                onAuthenticationRequested: root.authenticationRequested()
            }

            // Actions live in the same three-dot menu as a top-level Cast.
            // An always-visible Delete button makes replies inconsistent.
            }
        }
    }

    Loader {
        id: replyMenuLoader
        active: false
        sourceComponent: Component {
            Popup {
                id: replyMenu
                parent: Overlay.overlay
                width: 224
                padding: Theme.space1
                modal: true
                dim: false
                focus: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                background: PopupSurface { curveDepth: Theme.curveContainer }
                contentItem: ColumnLayout {
                    spacing: 0
                    ActionButton {
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconComment
                        text: qsTrId("genycaster.cast.view_conversation")
                        onClicked: { replyMenu.close(); root.castRequested(String(root.reply.castId || "")) }
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconUser
                        text: qsTrId("genycaster.cast.open_author_profile")
                        enabled: Number(root.reply.authorFid || 0) > 0
                        onClicked: { replyMenu.close(); root.profileRequested(Number(root.reply.authorFid)) }
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconCopy
                        text: qsTrId("genycaster.cast.copy_cast_id")
                        onClicked: { replyMenu.close(); presentationBridge.copyText(String(root.reply.castId || "")) }
                    }
                    ActionButton {
                        visible: String(root.reply.text || "").length > 0
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconCopy
                        text: qsTrId("genycaster.cast.copy_text")
                        onClicked: { replyMenu.close(); presentationBridge.copyText(String(root.reply.text || "")) }
                    }
                    Rectangle {
                        visible: root.ownReply
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.divider
                    }
                    ActionButton {
                        visible: root.ownReply
                        enabled: root.interactionsAvailable
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        destructive: true
                        iconGlyph: Theme.iconClose
                        text: qsTrId("genycaster.cast.delete")
                        onClicked: { replyMenu.close(); root.removeCastRequested(root.reply) }
                    }
                }
            }
        }
    }

}
