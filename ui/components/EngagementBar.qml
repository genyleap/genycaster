import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    objectName: "engagementBar"
    layoutDirection: Theme.uiDirection

    property int replyCount: 0
    property int recastCount: 0
    property int likeCount: 0
    // Optional verified impressions; unavailable provider values stay absent.
    property var viewCount: null
    property bool replyInteractive: false
    property bool recastInteractive: false
    property bool likeInteractive: false
    property bool recastedByMe: false
    property bool likedByMe: false
    // Keep the original per-cast contextual reactions. Render the contextual
    // glyph within the same small, square engagement hit target as a heart.
    property string contextText: ""
    readonly property string contextualLikeIcon: presentationBridge.contextualReactionIcon(root.contextText)
    readonly property string displayedLikeIcon: root.likedByMe && root.contextualLikeIcon === "actions/heart"
                                                 ? "actions/heart-filled" : root.contextualLikeIcon

    signal replyRequested()
    signal quoteRequested()
    signal recastRequested(bool active)
    signal likeRequested(bool active)
    signal authenticationRequested()

    spacing: Theme.space2
    Accessible.description: qsTrId("genycaster.engagement.value_replies").arg(LocaleFormat.formatInteger(root.replyCount))

    EngagementButton {
        objectName: "castReplyButton"
        iconGlyph: Theme.iconReply
        metricText: root.replyCount > 0 ? LocaleFormat.formatInteger(root.replyCount) : ""
        enabled: true
        Accessible.name: qsTrId("genycaster.engagement.reply_value_replies").arg(LocaleFormat.formatInteger(root.replyCount))
        // Reply always enters the central composer action. It owns the
        // authentication/signer gate and provides a visible reason when the
        // canonical target is unavailable; the old fallback swallowed intent.
        onClicked: root.replyRequested()
    }

    EngagementButton {
        iconGlyph: Theme.iconRepost
        metricText: root.recastCount > 0 ? LocaleFormat.formatInteger(root.recastCount) : ""
        active: root.recastedByMe
        enabled: true
        Accessible.name: root.recastedByMe
                         ? qsTrId("genycaster.engagement.undo_recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
                         : qsTrId("genycaster.engagement.recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
        onClicked: root.recastInteractive ? root.recastRequested(!root.recastedByMe) : root.authenticationRequested()
    }

    EngagementButton {
        iconGlyph: root.displayedLikeIcon
        metricText: root.likeCount > 0 ? LocaleFormat.formatInteger(root.likeCount) : ""
        active: root.likedByMe
        enabled: true
        Accessible.name: root.likedByMe
                         ? qsTrId("genycaster.engagement.unlike_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
                         : qsTrId("genycaster.engagement.like_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
        onClicked: root.likeInteractive ? root.likeRequested(!root.likedByMe) : root.authenticationRequested()
    }

    EngagementButton {
        objectName: "castQuoteButton"
        iconGlyph: Theme.iconQuote
        enabled: true
        Accessible.name: qsTrId("genycaster.composer.quote")
        onClicked: root.quoteRequested()
    }

    EngagementButton {
        objectName: "castViewCount"
        visible: root.viewCount !== null && root.viewCount !== undefined
                 && Number(root.viewCount) >= 0 && isFinite(Number(root.viewCount))
        iconGlyph: "eye"
        metricText: visible ? LocaleFormat.formatInteger(Number(root.viewCount)) : ""
        enabled: false
        Accessible.name: "Views: " + metricText
    }

    Item { Layout.fillWidth: true }
}
