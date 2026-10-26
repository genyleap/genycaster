import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ListView {
    id: root

    property string activeView: ""
    property var detail
    property bool compact: false
    property bool busy: false
    property bool interactionsAvailable: false
    property double accountFid: 0
    property var confirmedReply: ({})
    function normalizedCastHash(value) {
        const hash = String(value || "").trim().toLowerCase()
        return hash.startsWith("0x") ? hash.slice(2) : hash
    }
    readonly property bool hasConfirmedReply: root.activeView === "cast"
        && root.normalizedCastHash(root.detail.castId)
            === root.normalizedCastHash(root.confirmedReply.parentCastId)
        && String(root.confirmedReply.castId || "").length > 0
    readonly property var prioritizedReplies: {
        const source = root.detail.threadReplies || []
        if (!root.hasConfirmedReply) return source
        const pinnedId = root.normalizedCastHash(root.confirmedReply.castId)
        const ordered = []
        let found = false
        for (let index = 0; index < source.length; ++index) {
            const reply = source[index] || ({})
            if (root.normalizedCastHash(reply.castId) === pinnedId) {
                ordered.unshift(reply)
                found = true
            } else {
                ordered.push(reply)
            }
        }
        if (!found) ordered.unshift(root.confirmedReply)
        return ordered
    }
    function focusConfirmedReply() {
        if (!root.hasConfirmedReply) return
        Qt.callLater(function() {
            if (!root.hasConfirmedReply || root.prioritizedReplies.length === 0) return
            // Confirmed local replies are pinned at the beginning of the
            // flattened virtual list and can be focused without instantiation.
            root.positionViewAtIndex(0, ListView.Beginning)
        })
    }
    onHasConfirmedReplyChanged: if (hasConfirmedReply) focusConfirmedReply()
    onContentHeightChanged: if (hasConfirmedReply && root.contentY === 0) focusConfirmedReply()

    readonly property string directionSeed: String(root.detail.text || root.detail.bio || root.detail.displayName || root.detail.author || "")
    readonly property bool contentRtl: localizationController.isRightToLeftText(root.directionSeed)
    readonly property string castChannel: {
        const source = String(root.detail.parentUrl || "")
        const match = source.match(/\/(?:~\/)?channel\/([a-z0-9][a-z0-9_-]{0,63})(?:[/?#]|$)/i)
        const candidate = String(root.detail.channelId || root.detail.castChannel || (match ? match[1] : ""))
        return /^[a-z0-9][a-z0-9_-]{0,63}$/i.test(candidate) ? candidate : ""
    }
    readonly property string resolvedChannelImage: channelDirectory.imageRevision >= 0
        ? String(root.detail.channelImageUrl
            || channelDirectory.imageUrlForChannel(root.castChannel) || "") : ""
    readonly property string resolvedChannelName: channelDirectory.imageRevision >= 0
        ? String(root.detail.channelName
            || channelDirectory.nameForChannel(root.castChannel) || root.castChannel) : ""
    onCastChannelChanged: {
        if (root.castChannel.length > 0 && root.resolvedChannelImage.length === 0)
            channelDirectory.ensureChannelImage(root.castChannel)
    }
    readonly property string parentTokenLocator: TokenSemantic.canonicalLocator(String(root.detail.parentUrl || ""))
    readonly property bool hasProtocolTokenParent: root.parentTokenLocator.length > 0
    readonly property var effectiveEmbeds: root.activeView === "cast" ? root.buildEffectiveEmbeds() : []
    readonly property var visualEmbeds: {
        const result = []
        for (let index = 0; index < root.effectiveEmbeds.length; ++index) {
            if (root.isVisualImage(root.effectiveEmbeds[index]))
                result.push(root.effectiveEmbeds[index])
        }
        return result
    }
    readonly property var supportingEmbeds: {
        const result = []
        for (let index = 0; index < root.effectiveEmbeds.length; ++index) {
            if (!root.isVisualImage(root.effectiveEmbeds[index]))
                result.push(root.effectiveEmbeds[index])
        }
        return result
    }

    // Canonical embed grouping (shared rule base with CastItem). Only image
    // media belongs in the image gallery; playable video, audio, file,
    // collectible, cast-reference and link embeds go through EmbedPreview.
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

    function normalizedEmbedUrl(embed) {
        return String(embed && (embed.url || embed.value) ? (embed.url || embed.value) : "").split("#")[0]
    }

    function inferredTextToken() {
        if (typeof liveInsights === "undefined" || liveInsights === null)
            return null

        let locator = root.parentTokenLocator
        let targetUrl = ""
        const text = String(root.detail.text || "")
        if (locator.length === 0) {
            const urlMatches = text.match(/https?:\/\/[^\s<>]+/gi) || []
            for (let index = 0; index < urlMatches.length; ++index) {
                let candidate = String(urlMatches[index] || "")
                while (candidate.length > 0) {
                    const suffix = candidate[candidate.length - 1]
                    if (!/[.,!?;:]/.test(suffix) && suffix !== String.fromCharCode(41))
                        break
                    candidate = candidate.slice(0, -1)
                }
                const candidateLocator = TokenSemantic.locatorFromUrl(candidate)
                if (candidateLocator.length > 0) {
                    locator = candidateLocator
                    targetUrl = candidate
                    break
                }
            }
        }
        if (locator.length === 0)
            locator = TokenSemantic.canonicalLocator(
                        String(liveInsights.tokenLocatorForText(text) || ""))
        if (locator.length === 0)
            return null

        const source = root.detail.embeds || []
        for (let index = 0; index < source.length; ++index) {
            if (TokenSemantic.sameLocator(TokenSemantic.locatorFromEmbed(source[index]), locator))
                return null
        }
        return {
            kind: "token-address",
            tokenLocator: locator,
            url: targetUrl,
            value: targetUrl,
            inferred: true
        }
    }

    function inferredTextLink() {
        const text = String(root.detail.text || "")
        const match = text.match(/https?:\/\/[^\s<>]+/i)
        if (!match || match.length === 0)
            return null
        let candidate = String(match[0])
        while (candidate.length > 0) {
            const suffix = candidate[candidate.length - 1]
            if (suffix !== "." && suffix !== "," && suffix !== "!" && suffix !== "?"
                    && suffix !== ";" && suffix !== ":" && suffix !== String.fromCharCode(41))
                break
            candidate = candidate.slice(0, -1)
        }
        if (candidate.length === 0)
            return null
        if (TokenSemantic.locatorFromUrl(candidate).length > 0)
            return null
        const canonical = candidate.split("#")[0]
        const embeds = root.detail.embeds || []
        for (let index = 0; index < embeds.length; ++index) {
            if (root.normalizedEmbedUrl(embeds[index]) === canonical)
                return null
        }
        return { kind: "url", url: candidate, value: candidate, inferred: true }
    }

    function buildEffectiveEmbeds() {
        const result = []
        const source = root.detail.embeds || []
        for (let index = 0; index < source.length; ++index)
            result.push(source[index])
        const inferredToken = root.inferredTextToken()
        if (inferredToken !== null)
            result.push(inferredToken)
        const inferred = root.inferredTextLink()
        if (inferred !== null)
            result.push(inferred)
        return result
    }

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    signal backRequested()
    signal followersRequested()
    signal followingRequested()
    signal refreshRequested()
    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal followingMutationRequested(double targetFid, bool following)
    signal removeCastRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()

    readonly property var protocolEmbedUrls: {
        const values = root.effectiveEmbeds
        const result = []
        for (let index = 0; index < values.length; ++index) {
            const url = String(values[index].url || values[index].value || "")
            if (url.length > 0)
                result.push(url)
        }
        return result
    }

    readonly property string visibleCastText: {
        const original = String(root.detail.text || "")
        if (!original.length) return qsTrId("genycaster.detail.no_detail_cached_yet")
        const embedded = root.protocolEmbedUrls || []
        if (!embedded.length) return original
        // Canonical Cast text remains unchanged for quoting and publishing.
        // A URL is removed only if its destination already has an embed card.
        return original.replace(/https?:\/\/[^\s<>]+/gi, token => {
            const normalized = token.replace(/[.,!?;:)]+$/, "").split("#")[0]
            for (let i = 0; i < embedded.length; ++i) {
                if (String(embedded[i]).split("#")[0] === normalized)
                    return token.slice(normalized.length)
            }
            return token
        }).replace(/[ \t]+\n/g, "\n").replace(/\n[ \t]+/g, "\n")
          .replace(/\n{3,}/g, "\n\n").trim()
    }

    function ltrText(value) {
        const isolate = String.fromCharCode(0x2066)
        const pop = String.fromCharCode(0x2069)
        return isolate + String(value || "") + pop
    }

    function ltrHandle(value) {
        return root.ltrText(String.fromCharCode(0x40) + String(value || ""))
    }

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    spacing: Theme.space2
    reuseItems: true
    cacheBuffer: root.compact ? 340 : 650
    model: root.activeView === "cast" ? root.prioritizedReplies : []

    header: ColumnLayout {
        id: detailColumn
        // Cast threads may spread to the wide detail bound on desktop; only the
        // reading-style profile view keeps the narrower centered column. Compact
        // (mobile) layouts always fill the viewport minus the gutter.
        width: Math.min(root.width - (root.compact ? Theme.space2 * 2 : Theme.contentGutter * 2),
                        root.activeView === "cast"
                            ? (root.compact ? Theme.detailWidth : Theme.detailWidthWide)
                            : Theme.readingWidth)
        x: Math.max(root.compact ? Theme.space2 : Theme.contentGutter, (root.width - width) / 2)
        y: root.compact ? Theme.space3 : Theme.space4
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

            ActionButton {
                compact: true
                quiet: true
                iconGlyph: Theme.iconBack
                text: qsTrId("genycaster.detail.back_back_to_feed")
                onClicked: root.backRequested()
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: root.busy ? qsTrId("genycaster.detail.refreshing") : qsTrId("genycaster.detail.refresh")
                enabled: !root.busy
                onClicked: root.refreshRequested()
            }
        }

        Superellipse {
            Layout.fillWidth: true
            implicitHeight: detailBody.implicitHeight + (root.compact ? Theme.space2 : Theme.space4) * 2
            fillColor: root.activeView === "cast" ? Theme.backgroundPrimary : Theme.surfaceRaised
            borderWidth: root.activeView === "cast" && root.compact ? 0 : Theme.strokeHairline
            borderColor: root.activeView === "cast" && root.compact ? "transparent" : Theme.borderSubtle
            curveDepth: Theme.curveModal

            ColumnLayout {
                id: detailBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.compact ? Theme.space2 : Theme.space5
                spacing: root.compact ? Theme.space3 : Theme.space4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                    Item {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: root.activeView === "profile" ? 68 : (root.compact ? 46 : 52)
                        Layout.preferredHeight: Layout.preferredWidth

                        Avatar {
                            anchors.fill: parent
                            avatarSize: parent.width
                            sourceUrl: root.detail.avatarUrl || ""
                            displayName: root.detail.displayName || root.detail.author || ""
                        }

                        TapHandler {
                            enabled: root.activeView === "cast" && Number(root.detail.authorFid || 0) > 0
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.profileRequested(Number(root.detail.authorFid))
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        RowLayout {
                            id: detailNameAndChannel
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space1
                            layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                            DirectionAwareLabel {
                                Layout.minimumWidth: 0
                                Layout.preferredWidth: Math.min(implicitWidth,
                                    root.activeView === "cast" && root.castChannel.length > 0
                                        ? Math.max(84, detailNameAndChannel.width * 0.62)
                                        : detailNameAndChannel.width)
                                Layout.maximumWidth: Layout.preferredWidth
                                text: root.detail.displayName || root.detail.author || qsTrId("genycaster.detail.canonical_detail")
                                color: authorNameHover.hovered ? Theme.accent : Theme.ink
                                font.pixelSize: root.activeView === "profile" ? Theme.typeTitle2Size : Theme.typeTitle3Size
                                font.weight: Theme.weightDemiBold
                                wrapMode: root.activeView === "profile" ? Text.Wrap : Text.NoWrap
                                elide: root.activeView === "cast" ? Text.ElideRight : Text.ElideNone

                                HoverHandler { id: authorNameHover }
                                TapHandler {
                                    enabled: root.activeView === "cast" && Number(root.detail.authorFid || 0) > 0
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.profileRequested(Number(root.detail.authorFid))
                                }
                            }

                            RowLayout {
                                visible: root.activeView === "cast" && root.castChannel.length > 0
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space1
                                layoutDirection: Qt.LeftToRight
                                DirectionAwareLabel {
                                    text: qsTr("in")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                Avatar {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    avatarSize: 20
                                    sourceUrl: root.resolvedChannelImage
                                    displayName: root.resolvedChannelName
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: root.resolvedChannelName
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                            }
                            Item {
                                visible: root.activeView !== "cast" || root.castChannel.length === 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                            Label {
                                visible: Boolean(root.detail.username)
                                LayoutMirroring.enabled: false
                                text: root.ltrHandle(root.detail.username)
                                font.family: Theme.latinFontFamily
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                            }

                            Label {
                                visible: root.activeView === "profile" && Number(root.detail.fid || 0) > 0
                                LayoutMirroring.enabled: false
                                text: qsTrId("genycaster.detail.fid_value").arg(LocaleFormat.formatIdentifier(Number(root.detail.fid)))
                                font.family: Theme.latinFontFamily
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                horizontalAlignment: Text.AlignLeft
                            }

                            // Timestamp belongs with the author identity, as
                            // it does for replies. Show relative on narrow
                            // screens and localized full datetime on desktop.
                            DirectionAwareLabel {
                                visible: root.activeView === "cast"
                                         && Number(root.detail.createdAtMs || 0) > 0
                                Layout.fillWidth: true
                                text: {
                                    const milliseconds = Number(root.detail.createdAtMs || 0)
                                    return milliseconds > 0
                                           ? "· " + (root.compact
                                              ? LocaleFormat.formatRelativeTime(milliseconds)
                                              : LocaleFormat.formatDateTime(milliseconds))
                                           : ""
                                }
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }

                            Item {
                                Layout.fillWidth: root.activeView === "profile"
                            }
                        }
                    }
                }

                Superellipse {
                    visible: root.activeView === "cast"
                             && Boolean(root.detail.parentCastId)
                    Layout.fillWidth: true
                    implicitHeight: parentContextLayout.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceMuted
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveCompact

                    RowLayout {
                        id: parentContextLayout
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                        Superellipse {
                            Layout.preferredWidth: 3
                            Layout.preferredHeight: Math.max(34, parentContextContent.implicitHeight)
                            fillColor: Theme.accent
                            curveDepth: 2
                        }

                        Avatar {
                            readonly property var parentCast: root.detail.parentPreview || ({})
                            visible: Boolean(parentCast.resolved && (parentCast.avatarUrl
                                              || parentCast.displayName || parentCast.author))
                            avatarSize: visible ? 29 : 0
                            sourceUrl: String(parentCast.avatarUrl || "")
                            displayName: String(parentCast.displayName || parentCast.author || "")
                        }

                        ColumnLayout {
                            id: parentContextContent
                            readonly property var parentCast: root.detail.parentPreview || ({})
                            Layout.fillWidth: true
                            spacing: Theme.space1

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(parentContextContent.parentCast.displayName
                                             || parentContextContent.parentCast.author
                                             || qsTrId("genycaster.cast.replying_to_another_cast"))
                                color: Theme.accent
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(parentContextContent.parentCast.text
                                             || qsTrId("genycaster.cast.open_original_cast"))
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                maximumLineCount: 3
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }
                        }

                        Icon {
                            visible: Boolean(root.detail.parentCastId)
                            text: root.contentRtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.inkSubtle
                            preferredSize: Theme.typeCaptionSize
                        }
                    }

                    TapHandler {
                        enabled: Boolean(root.detail.parentCastId)
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.castRequested(String(root.detail.parentCastId || ""))
                    }
                }

                RichCastRenderer {
                    emojiSurface: 2
                    visible: root.activeView === "cast"
                    Layout.fillWidth: true
                    castId: String(root.detail.castId || "")
                    plainText: root.visibleCastText
                    compact: root.compact
                    suppressedEmbedUrls: root.protocolEmbedUrls
                    onExternalRequested: url => root.externalRequested(url)
                    onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                    onCastRequested: castId => root.castRequested(castId)
                }

                RichText {
                    visible: root.activeView !== "cast"
                    Layout.fillWidth: true
                    sourceText: root.detail.bio || qsTrId("genycaster.detail.no_detail_cached_yet")
                    textSize: Theme.typeBodySize
                    textLineHeight: Theme.typeBodyLineHeight
                    onExternalRequested: url => root.externalRequested(url)
                }

                CastEmbedsPanel {
                    visible: root.activeView === "cast"
                             && (root.visualEmbeds.length > 0 || root.supportingEmbeds.length > 0)
                    Layout.fillWidth: true
                    visualEmbeds: root.visualEmbeds
                    supportingEmbeds: root.supportingEmbeds
                    compact: root.compact
                    onExternalRequested: url => root.externalRequested(url)
                    onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                    onCastRequested: castId => root.castRequested(castId)
                }

                Rectangle {
                    visible: root.activeView === "cast"
                    Layout.fillWidth: true
                    height: Theme.strokeHairline
                    color: Theme.divider
                }

                EngagementBar {
                    compact: root.compact
                    visible: root.activeView === "cast"
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space1
                    Layout.rightMargin: Theme.space1
                    replyCount: Number(root.detail.replyCount || 0)
                    recastCount: Number(root.detail.recastCount || 0)
                    likeCount: Number(root.detail.likeCount || 0)
                    viewCount: root.detail.viewCount === undefined ? null : root.detail.viewCount
                    castId: String(root.detail.castId || "")
                    castData: root.detail
                    showViews: true
                    scrollMoving: root.moving
                    enrichmentActive: root.activeView === "cast" && root.visible
                        && root.contentY < (root.headerItem ? root.headerItem.height : 700) + 320
                    authorFid: Number(root.detail.authorFid || 0)
                    castText: String(root.detail.text || "")
                    contextText: String(root.detail.text || "")
                    replyInteractive: root.interactionsAvailable && Boolean(root.detail.castId)
                    recastInteractive: replyInteractive
                    likeInteractive: replyInteractive
                    recastedByMe: Boolean(root.detail.recastedByMe || false)
                    likedByMe: Boolean(root.detail.likedByMe || false)
                    onReplyRequested: root.replyRequested(root.detail)
                    onQuoteRequested: root.quoteRequested(root.detail)
                    onRecastRequested: active => root.reactionRequested("recast", root.detail, active)
                    onLikeRequested: active => root.reactionRequested("like", root.detail, active)
                    onAuthenticationRequested: root.authenticationRequested()
                }



                RowLayout {
                    visible: root.activeView === "cast"
                             && root.interactionsAvailable
                             && Number(root.detail.authorFid || 0) === Number(root.accountFid)
                             && Boolean(root.detail.castId)
                    Layout.fillWidth: true
                    layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        quiet: true
                        destructive: true
                        compact: true
                        text: qsTrId("genycaster.detail.delete_cast")
                        onClicked: root.removeCastRequested(root.detail)
                    }
                }

                RowLayout {
                    visible: root.activeView === "profile"
                    layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    ActionButton {
                        visible: root.interactionsAvailable
                                 && Boolean(root.detail.followingKnown)
                                 && Number(root.detail.fid || 0) > 0
                                 && Number(root.detail.fid || 0) !== Number(root.accountFid)
                        iconGlyph: Boolean(root.detail.followingByMe) ? Theme.iconCheck : ""
                        text: Boolean(root.detail.followingByMe) ? qsTrId("genycaster.detail.followed") : qsTrId("genycaster.detail.follow")
                        selected: Boolean(root.detail.followingByMe)
                        prominent: !Boolean(root.detail.followingByMe)
                        Accessible.description: Boolean(root.detail.followingByMe)
                                                ? qsTrId("genycaster.detail.unfollow") : text
                        onClicked: root.followingMutationRequested(
                            Number(root.detail.fid), !Boolean(root.detail.followingByMe))
                    }

                    ActionButton {
                        quiet: true
                        text: qsTrId("genycaster.detail.followers")
                        onClicked: root.followersRequested()
                    }

                    ActionButton {
                        quiet: true
                        text: qsTrId("genycaster.detail.following")
                        onClicked: root.followingRequested()
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        ColumnLayout {
            id: repliesContainer
            visible: root.activeView === "cast"
                     && root.prioritizedReplies.length > 0
            Layout.fillWidth: true
            // Keep the reply header connected to the parent cast without a
            // doubled layout gap before the first conversation rail.
            Layout.topMargin: Theme.space3
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                DirectionAwareLabel {
                    text: qsTrId("genycaster.profile.replies")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }

                Superellipse {
                    // A quiet count, not a button competing with the Replies title.
                    Layout.preferredWidth: Math.max(26, replyCountLabel.implicitWidth + Theme.space3)
                    Layout.preferredHeight: 26
                    fillColor: Theme.surfaceSelected
                    borderWidth: 0
                    borderColor: Theme.borderMuted
                    curveDepth: height / 2
                    DirectionAwareLabel {
                        id: replyCountLabel
                        anchors.centerIn: parent
                        text: LocaleFormat.formatInteger(root.prioritizedReplies.length)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightBold
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // The header is inside ListView.header while replies are delegates.
            // An explicit trailing gap prevents the count badge from touching
            // the next card's top stroke across font scales and languages.
            Item { Layout.preferredHeight: Theme.space4 }
        }
    }

    delegate: ThreadReplyItem {
        required property var modelData
        width: Math.min(root.width - (root.compact ? Theme.space2 * 2 : Theme.contentGutter * 2),
                        root.compact ? Theme.detailWidth : Theme.detailWidthWide)
        x: Math.max(root.compact ? Theme.space2 : Theme.contentGutter,
                    (root.width - width) / 2)
        reply: modelData
        enrichmentActive: root.activeView === "cast" && root.visible
            && y + height >= root.contentY - 320
            && y <= root.contentY + root.height + 320
        scrollMoving: root.moving
        highlighted: root.hasConfirmedReply
                     && root.normalizedCastHash(modelData.castId)
                        === root.normalizedCastHash(root.confirmedReply.castId)
        depth: Number(modelData.depth || 1)
        compact: root.compact
        interactionsAvailable: root.interactionsAvailable
        accountFid: root.accountFid
        parentAuthoredByMe: Number(root.detail.authorFid || 0) > 0
                             && Number(root.detail.authorFid || 0) === Number(root.accountFid)
        onCastRequested: castId => root.castRequested(castId)
        onProfileRequested: fid => root.profileRequested(fid)
        onReplyRequested: cast => root.replyRequested(cast)
        onQuoteRequested: cast => root.quoteRequested(cast)
        onReactionRequested: (kind, cast, active) => root.reactionRequested(kind, cast, active)
        onExternalRequested: url => root.externalRequested(url)
        onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
        onAuthenticationRequested: root.authenticationRequested()
        onRemoveCastRequested: cast => root.removeCastRequested(cast)
    }

    ScrollToTopButton {
        // ListView's default child parenting is contentItem; explicitly
        // parent this overlay to the viewport so it does not scroll away.
        parent: root
        flickable: root
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space5
        anchors.bottomMargin: root.compact ? Theme.space2 + 12 : Theme.space4
    }
    ContentRefreshIndicator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 50
        active: root.busy && root.activeView === "cast"
    }

}
