import QtQuick
import QtQuick.Layouts

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
    property double accountFid: 0
    property bool highlighted: false

    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()
    signal removeCastRequested(var cast)

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
    spacing: Theme.space1

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
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Rectangle {
            Layout.preferredWidth: root.highlighted ? 4 : 2
            Layout.fillHeight: true
            Layout.minimumHeight: replyContent.implicitHeight
            color: root.highlighted ? Theme.accent
                                     : root.depth <= 1 ? Theme.accent : Theme.borderSubtle
            opacity: root.depth <= 1 ? 0.72 : 0.7
        }

        ColumnLayout {
            id: replyContent
            Layout.fillWidth: true
            spacing: Theme.space1

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                Avatar {
                    avatarSize: root.compact ? 28 : 32
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
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
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
                replyCount: Number(root.reply.replyCount || 0)
                recastCount: Number(root.reply.recastCount || 0)
                likeCount: Number(root.reply.likeCount || 0)
                contextText: String(root.reply.text || "")
                replyInteractive: root.interactionsAvailable && Boolean(root.reply.castId)
                recastInteractive: replyInteractive
                likeInteractive: replyInteractive
                recastedByMe: Boolean(root.reply.recastedByMe || false)
                likedByMe: Boolean(root.reply.likedByMe || false)
                onReplyRequested: root.replyRequested(root.reply)
                onRecastRequested: active => root.reactionRequested("recast", root.reply, active)
                onLikeRequested: active => root.reactionRequested("like", root.reply, active)
                onAuthenticationRequested: root.authenticationRequested()
            }

            ActionButton {
                objectName: "deleteOwnedReply"
                visible: root.interactionsAvailable
                         && Number(root.reply.authorFid || 0) > 0
                         && Number(root.reply.authorFid || 0) === Number(root.accountFid)
                         && String(root.reply.castId || "").length > 0
                Layout.alignment: Theme.rtl ? Qt.AlignLeft : Qt.AlignRight
                compact: true
                quiet: true
                destructive: true
                text: qsTrId("genycaster.detail.delete_cast")
                onClicked: root.removeCastRequested(root.reply)
            }
        }
    }

    Repeater {
        model: root.depth < 4 ? (root.reply.threadReplies || []) : []

        delegate: ColumnLayout {
            id: nestedReplyHost

            required property var modelData

            Layout.fillWidth: true
            Layout.leftMargin: root.compact ? Theme.space3 : Theme.space4
            spacing: 0

            // A QML type cannot instantiate itself statically. Load the next
            // finite thread level dynamically so recursive conversations remain
            // supported without making ThreadReplyItem unavailable at startup.
            Loader {
                id: nestedReplyLoader

                Layout.fillWidth: true
                active: root.depth < 4
                        && nestedReplyHost.modelData !== undefined
                        && nestedReplyHost.modelData !== null
                source: Qt.resolvedUrl("ThreadReplyItem.qml")

                onLoaded: {
                    item.reply = nestedReplyHost.modelData
                    item.depth = root.depth + 1
                    item.compact = root.compact
                    item.interactionsAvailable = root.interactionsAvailable
                    item.accountFid = root.accountFid
                }
            }

            Connections {
                target: nestedReplyLoader.item
                enabled: nestedReplyLoader.status === Loader.Ready

                function onCastRequested(castId) { root.castRequested(castId) }
                function onProfileRequested(fid) { root.profileRequested(fid) }
                function onReplyRequested(cast) { root.replyRequested(cast) }
                function onReactionRequested(kind, cast, active) {
                    root.reactionRequested(kind, cast, active)
                }
                function onExternalRequested(url) { root.externalRequested(url) }
                function onMediaRequested(mediaItems, index) {
                    root.mediaRequested(mediaItems, index)
                }
                function onAuthenticationRequested() { root.authenticationRequested() }
                function onRemoveCastRequested(cast) { root.removeCastRequested(cast) }
            }
        }
    }
}
