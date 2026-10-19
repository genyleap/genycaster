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
    property string castId: ""
    property double authorFid: 0
    property string castText: ""
    readonly property var resolvedViewCount: {
        if (root.viewCount !== null && root.viewCount !== undefined
                && isFinite(Number(root.viewCount)) && Number(root.viewCount) >= 0)
            return root.viewCount
        return CastViews.countFor(root.castId)
    }

    function requestViews() {
        if (root.castId.length > 0 && root.authorFid > 0)
            CastViews.ensure(root.castId, root.authorFid, root.castText)
    }

    Component.onCompleted: requestViews()
    onCastIdChanged: requestViews()
    onAuthorFidChanged: requestViews()
    onCastTextChanged: requestViews()
    property bool replyInteractive: false
    property bool recastInteractive: false
    property bool likeInteractive: false
    property bool recastedByMe: false
    property bool likedByMe: false
    // Context stays available for future protocol-specific reactions, but the
    // primary Like action always uses one recognisable heart across all feeds.
    property string contextText: ""
    readonly property string displayedLikeIcon: Theme.iconHeart

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
        objectName: "castQuoteButton"
        iconGlyph: Theme.iconQuote
        enabled: true
        Accessible.name: qsTrId("genycaster.composer.quote")
        onClicked: root.quoteRequested()
    }

    EngagementButton {
        iconGlyph: Theme.iconRepost
        metricText: root.recastCount > 0 ? LocaleFormat.formatInteger(root.recastCount) : ""
        active: root.recastedByMe
        activeColor: Theme.positive
        enabled: true
        Accessible.name: root.recastedByMe
                         ? qsTrId("genycaster.engagement.undo_recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
                         : qsTrId("genycaster.engagement.recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
        onClicked: root.recastInteractive ? root.recastRequested(!root.recastedByMe) : root.authenticationRequested()
    }

    EngagementButton {
        iconGlyph: root.displayedLikeIcon
        iconFilled: root.likedByMe
        activeColor: Theme.negative
        metricText: root.likeCount > 0 ? LocaleFormat.formatInteger(root.likeCount) : ""
        active: root.likedByMe
        enabled: true
        Accessible.name: root.likedByMe
                         ? qsTrId("genycaster.engagement.unlike_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
                         : qsTrId("genycaster.engagement.like_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
        onClicked: root.likeInteractive ? root.likeRequested(!root.likedByMe) : root.authenticationRequested()
    }

    EngagementButton {
        objectName: "castViewCount"
        visible: root.resolvedViewCount !== null && root.resolvedViewCount !== undefined
                 && Number(root.resolvedViewCount) >= 0 && isFinite(Number(root.resolvedViewCount))
        iconGlyph: "eye"
        metricText: visible ? LocaleFormat.formatCompactNumber(Number(root.resolvedViewCount)) : ""
        enabled: false
        Accessible.name: "Views: " + metricText
    }

    Item { Layout.fillWidth: true }
}
