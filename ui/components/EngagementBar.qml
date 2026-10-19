import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    objectName: "engagementBar"
    layoutDirection: Theme.uiDirection

    property bool compact: false
    property bool showViews: true
    property var castData: ({})
    property int replyCount: 0
    property int recastCount: 0
    property int likeCount: 0
    // Optional verified impressions; unavailable provider values stay absent.
    property var viewCount: null
    property string castId: ""
    property double authorFid: 0
    property string castText: ""
    property bool scrollMoving: false
    readonly property double viewerFid: Number(presentationBridge.accountFid || 0)
    readonly property bool browserLikeEvidence: Qt.platform.os === "wasm"
                                                && CastReactions.reacted(root.castId, root.viewerFid, "like")
    readonly property bool browserRecastEvidence: Qt.platform.os === "wasm"
                                                  && CastReactions.reacted(root.castId, root.viewerFid, "recast")
    readonly property var browserLikeOverride: Qt.platform.os === "wasm"
        ? CastReactions.localValue(root.castId, root.viewerFid, "like") : null
    readonly property var browserRecastOverride: Qt.platform.os === "wasm"
        ? CastReactions.localValue(root.castId, root.viewerFid, "recast") : null
    readonly property bool effectiveLikedByMe: root.browserLikeOverride !== null
        ? root.browserLikeOverride : root.likedByMe || root.browserLikeEvidence
    readonly property bool effectiveRecastedByMe: root.browserRecastOverride !== null
        ? root.browserRecastOverride : root.recastedByMe || root.browserRecastEvidence

    function requestBrowserReactions() {
        if (Qt.platform.os === "wasm" && !root.scrollMoving
                && presentationBridge.isAuthenticated && root.viewerFid > 0)
            CastReactions.ensure(root.castId, root.viewerFid)
    }
    onViewerFidChanged: requestBrowserReactions()
    onScrollMovingChanged: if (!root.scrollMoving) requestBrowserReactions()
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

    Component.onCompleted: {
        requestViews()
        requestBrowserReactions()
    }
    onCastIdChanged: {
        requestViews()
        requestBrowserReactions()
    }
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
    readonly property string normalizedCastId: CastViews.normalizeHash(root.castId)
    readonly property bool isBookmarked: presentationBridge.bookmarkedCastIds.indexOf(root.normalizedCastId) >= 0

    signal replyRequested()
    signal quoteRequested()
    signal recastRequested(bool active)
    signal likeRequested(bool active)
    signal authenticationRequested()

    // Six equally flexible slots prevent the large dead zone formerly left
    // between Like and Views, while preserving full touch hit targets.
    spacing: root.compact ? 0 : Theme.space1
    Accessible.description: qsTrId("genycaster.engagement.value_replies").arg(LocaleFormat.formatInteger(root.replyCount))

    EngagementButton {
        objectName: "castReplyButton"
        Layout.fillWidth: true
        iconGlyph: Theme.iconReply
        metricText: root.replyCount > 0 ? LocaleFormat.formatCompactNumber(root.replyCount) : ""
        enabled: true
        Accessible.name: qsTrId("genycaster.engagement.reply_value_replies").arg(LocaleFormat.formatInteger(root.replyCount))
        // Reply always enters the central composer action. It owns the
        // authentication/signer gate and provides a visible reason when the
        // canonical target is unavailable; the old fallback swallowed intent.
        onClicked: root.replyRequested()
    }

    EngagementButton {
        objectName: "castQuoteButton"
        Layout.fillWidth: true
        iconGlyph: Theme.iconQuote
        enabled: true
        Accessible.name: qsTrId("genycaster.composer.quote")
        onClicked: root.quoteRequested()
    }

    EngagementButton {
        Layout.fillWidth: true
        iconGlyph: Theme.iconRepost
        metricText: root.recastCount > 0 ? LocaleFormat.formatCompactNumber(root.recastCount) : ""
        active: root.effectiveRecastedByMe
        activeColor: Theme.positive
        enabled: true
        Accessible.name: root.effectiveRecastedByMe
                         ? qsTrId("genycaster.engagement.undo_recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
                         : qsTrId("genycaster.engagement.recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
        onClicked: {
            if (!root.recastInteractive) {
                root.authenticationRequested()
                return
            }
            const next = !root.effectiveRecastedByMe
            if (Qt.platform.os === "wasm" && root.viewerFid > 0)
                CastReactions.setLocal(root.castId, root.viewerFid, "recast", next)
            root.recastRequested(next)
        }
    }

    EngagementButton {
        Layout.fillWidth: true
        iconGlyph: root.displayedLikeIcon
        iconFilled: root.effectiveLikedByMe
        activeColor: Theme.negative
        metricText: root.likeCount > 0 ? LocaleFormat.formatCompactNumber(root.likeCount) : ""
        active: root.effectiveLikedByMe
        enabled: true
        Accessible.name: root.effectiveLikedByMe
                         ? qsTrId("genycaster.engagement.unlike_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
                         : qsTrId("genycaster.engagement.like_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
        onClicked: {
            if (!root.likeInteractive) {
                root.authenticationRequested()
                return
            }
            const next = !root.effectiveLikedByMe
            if (Qt.platform.os === "wasm" && root.viewerFid > 0)
                CastReactions.setLocal(root.castId, root.viewerFid, "like", next)
            root.likeRequested(next)
        }
    }

    // Views remain inside the same footer as actions for every cast,
    // including threaded replies. The metric itself is not clickable.
    RowLayout {
        id: castViewsLabel
        objectName: "castViewCount"
        visible: root.showViews && root.resolvedViewCount !== null && root.resolvedViewCount !== undefined
                 && Number(root.resolvedViewCount) >= 0 && isFinite(Number(root.resolvedViewCount))
        Layout.fillWidth: true
        Layout.minimumWidth: Theme.iconSizeSmall + Theme.space2
        spacing: root.compact ? 2 : Theme.space1
        Layout.alignment: Qt.AlignVCenter
        Accessible.role: Accessible.StaticText

        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
        Accessible.name: "Views: " + viewMetric.text

        Icon {
            text: Theme.iconPreview
            preferredSize: Theme.iconSizeSmall
            color: Theme.inkMuted
            Layout.preferredWidth: Theme.iconSizeSmall
            Layout.preferredHeight: Theme.iconSizeSmall
        }

        Label {
            id: viewMetric
            text: castViewsLabel.visible
                  ? LocaleFormat.formatCompactNumber(Number(root.resolvedViewCount)) : ""
            color: Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightMedium
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
    }

    EngagementButton {
        id: bookmarkButton
        objectName: "castBookmarkButton"
        Layout.fillWidth: true
        iconGlyph: Theme.iconBookmark
        iconFilled: root.isBookmarked
        active: root.isBookmarked
        activeColor: Theme.accent
        enabled: root.normalizedCastId.length > 0 && presentationBridge.isAuthenticated
        Accessible.name: root.isBookmarked ? "Remove bookmark" : "Bookmark cast"
        onClicked: presentationBridge.toggleBookmark(root.castData)
    }
}
