import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var modelData
    property bool compact: false
    property bool interactionsAvailable: false
    property bool enrichmentActive: true
    property double accountFid: 0
    property var scrollHost: null
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)

    // Cast content owns its own bidi flow. The application shell may be RTL,
    // but a Latin cast must remain an LTR message and vice versa.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    signal openRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()

    function resolveScrollHost() {
        let candidate = root.parent
        while (candidate !== null) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined) {
                root.scrollHost = candidate
                return
            }
            candidate = candidate.parent
        }
        root.scrollHost = null
    }

    Component.onCompleted: {
        root.resolveScrollHost()
        if (root.enrichmentActive && root.castChannel.length > 0
                && root.channelImageUrl.length === 0)
            channelDirectory.ensureChannelImage(root.castChannel)
    }
    onParentChanged: root.resolveScrollHost()
    // Keep the supporting subtree alive across payload revisions and motion.
    // URL/identity changes flow into its existing preview; async Loaders still
    // spread initial construction across frames.

    readonly property bool isOwnCast: Number(root.modelData.authorFid || 0) > 0
                                      && Number(root.modelData.authorFid || 0) === Number(root.accountFid)
    readonly property bool canDelete: root.isOwnCast && Boolean(root.modelData.castId)
    readonly property bool canDeleteNow: root.canDelete && root.interactionsAvailable
    readonly property bool hasWriteState: root.modelData.writeState !== undefined && root.modelData.writeState !== ""
    readonly property string parentCastId: String(root.modelData.parentCastId || "")
    readonly property bool hasParentCast: root.parentCastId.length > 0
    readonly property bool hasParentUrl: Boolean(root.modelData.parentUrl)
    readonly property string parentUrlText: String(root.modelData.parentUrl || "")
    readonly property string parentTokenLocator: TokenSemantic.canonicalLocator(root.parentUrlText)
    readonly property bool hasProtocolTokenParent: root.parentTokenLocator.length > 0
    readonly property bool hasHumanReadableParentUrl: root.hasParentUrl && !root.hasProtocolTokenParent
                                                       && !root.parentUrlText.startsWith("chain://")
                                                       && !root.parentUrlText.startsWith("farcaster://")
    // Farcaster uses CAIP-19 parent URLs for token tickers. That association is
    // metadata for the cast, not a reply/thread parent and must not render the
    // reply-context banner.
    // parent_url identifies channel/root context for many Farcaster casts; it
    // is not a reply target. Only a canonical parent cast should render the
    // reply-parent card.
    readonly property bool hasContext: root.hasParentCast || Boolean(root.modelData.isReply)
    readonly property var parentPreview: root.modelData.parentPreview || ({})
    readonly property bool hasParentPreview: Boolean(root.parentPreview.resolved)
                                               || (Boolean(root.parentPreview.castId)
                                                   && (Boolean(root.parentPreview.text)
                                                       || Boolean(root.parentPreview.author)
                                                       || Boolean(root.parentPreview.displayName)
                                                       || Boolean(root.parentPreview.avatarUrl)
                                                       || Number(root.parentPreview.createdAtMs || 0) > 0
                                                       || (root.parentPreview.embeds || []).length > 0))
    property bool parentHydrationTimedOut: false
    onParentCastIdChanged: root.parentHydrationTimedOut = false
    readonly property string castChannel: {
        const supplied = String(root.modelData.channelId || root.modelData.castChannel || "").trim()
        const source = String(root.modelData.parentUrl || "")
        const match = source.match(/\/(?:~\/)?channel\/([a-z0-9][a-z0-9_-]{0,63})(?:[/?#]|$)/i)
        const candidate = supplied || (match ? match[1] : "")
        return /^[a-z0-9][a-z0-9_-]{0,63}$/i.test(candidate) ? candidate : ""
    }
    readonly property string channelDisplayName: channelDirectory.imageRevision >= 0
        ? String(root.modelData.channelName
                 || channelDirectory.nameForChannel(root.castChannel) || root.castChannel) : ""
    readonly property string channelImageUrl: channelDirectory.imageRevision >= 0
        ? String(root.modelData.channelImageUrl
                 || channelDirectory.imageUrlForChannel(root.castChannel) || "") : ""
    // Bound the on-demand lookup to visible/hydrated casts; a long feed must
    // not launch a request for every off-screen channel.
    onEnrichmentActiveChanged: {
        if (root.enrichmentActive && root.castChannel.length > 0
                && root.channelImageUrl.length === 0)
            channelDirectory.ensureChannelImage(root.castChannel)
    }
    readonly property string authorName: root.modelData.displayName || root.modelData.author || qsTrId("genycaster.cast.unknown")
    readonly property string directionSeed: String(root.modelData.text || root.authorName || "")
    readonly property bool contentRtl: localizationController.isRightToLeftText(root.directionSeed)

    // Canonical embed grouping. Only image media belongs in the image gallery;
    // playable video, audio, file, collectible, cast-reference and link embeds
    // are each dispatched by EmbedPreview through the single media classifier.
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
        if (root.embedFamily(embed) === "image")
            return true
        if (!Boolean(root.modelData && root.modelData._artChannelSource))
            return false
        const url = root.normalizedEmbedUrl(embed).toLowerCase()
        return url.indexOf("imagedelivery.net/") >= 0
                || url.indexOf("res.cloudinary.com/") >= 0
                || url.indexOf("i.imgur.com/") >= 0
                || url.indexOf("pbs.twimg.com/") >= 0
                || url.indexOf("arweave.net/") >= 0
                || url.indexOf("gateway.pinata.cloud/") >= 0
                || url.indexOf("nftstorage.link/") >= 0
                || url.indexOf("ipfs.io/") >= 0
                || url.indexOf("wrpcd.net/") >= 0
    }

    function normalizedEmbedUrl(embed) {
        return String(embed && (embed.url || embed.value) ? (embed.url || embed.value) : "").split("#")[0]
    }

    function previewMediaFromEmbeds(values) {
        const embeds = values || []
        for (let index = 0; index < embeds.length; ++index) {
            const value = embeds[index] || ({})
            const family = root.embedFamily(value)
            const source = String(value.imageSource || value.previewImage || value.artwork || "")
            if ((family === "image" || family === "video" || family === "collectible" || family === "nft")
                    && source.length > 0)
                return { source: source, family: family }
        }
        return ({ source: "", family: "" })
    }

    readonly property var parentMediaPreview: root.previewMediaFromEmbeds(root.parentPreview.embeds || [])

    function inferredTextToken() {
        if (typeof liveInsights === "undefined" || liveInsights === null)
            return null

        let locator = root.parentTokenLocator
        let targetUrl = ""
        const text = String(root.modelData.text || "")

        // A direct token URL in cast text is stronger evidence than a cashtag.
        // Preserve that URL on the token card so the external-link action still
        // opens the author's original destination.
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

        const source = root.modelData.embeds || []
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
        const text = String(root.modelData.text || "")
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
        const source = root.modelData.embeds || []
        for (let index = 0; index < source.length; ++index) {
            if (root.normalizedEmbedUrl(source[index]) === canonical)
                return null
        }
        return { kind: "url", url: candidate, value: candidate, inferred: true }
    }

    readonly property var effectiveEmbeds: {
        const source = root.modelData.embeds || []
        const result = []
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

    readonly property var embedPartition: {
        const values = root.effectiveEmbeds || []
        const visual = []
        const supporting = []
        // Classify each embed once. Both lists are consumed by the same cast
        // delegate, so two independent bindings only duplicate classifier work.
        for (let index = 0; index < values.length; ++index) {
            const value = values[index]
            if (root.isVisualImage(value))
                visual.push(value)
            else
                supporting.push(value)
        }
        return { visual: visual, supporting: supporting }
    }
    readonly property var visualEmbeds: root.embedPartition.visual
    readonly property var supportingEmbeds: root.embedPartition.supporting
    // A simple single-web-link Cast can place its short author text inside
    // the destination card instead of rendering a detached paragraph above it.
    // Keep rich mentions, long text, galleries, mint pages and other media
    // on the existing interactive Cast renderer.
    readonly property bool combinedLinkCast: {
        if (root.mediaFirst || root.supportingEmbeds.length !== 1
                || root.displayText.length === 0 || root.displayText.length > 180)
            return false
        const embed = root.supportingEmbeds[0]
        if (root.embedFamily(embed) !== "link" || /[@#][a-z0-9_]/i.test(root.displayText))
            return false
        const url = root.normalizedEmbedUrl(embed).toLowerCase()
        return /^https?:\/\//.test(url)
               && !/(?:opensea\.io|superrare\.com|zora\.co|foundation\.app|manifold\.xyz|rarible\.com|magiceden\.io|highlight\.xyz|objkt\.com|mint\.fun)/.test(url)
               && !/\/mint(?:[/?#]|$)/.test(url)
    }
    readonly property bool mediaFirst: root.visualEmbeds.length > 0
    // Keep the artwork, caption, author row and engagement rail on one
    // symmetric mobile grid. The outer cast gutter remains untouched.
    // messageRow is the sole mobile horizontal inset. Descendants must not add
    // another side margin or the trailing edge becomes visually wider.
    readonly property real mediaContentInset: 0

    readonly property string displayText: {
        const source = String(root.modelData.text || "")
        // URL embeds render as semantic preview cards. Keep the protocol text in
        // modelData for copy/share, but do not duplicate raw URLs in the cast body.
        return source.replace(/https?:\/\/[^\s<>]+/gi, " ")
                     .replace(/[ \t]+\n/g, "\n")
                     .replace(/\n[ \t]+/g, "\n")
                     .replace(/[ \t]{2,}/g, " ")
                     .replace(/\n{3,}/g, "\n\n")
                     .trim()
    }

    // Cast permalinks are protocol identities, never the first media/embed URL.
    // Keep the copy action consistent with the footer Share menu and its
    // platform-aware public GenyCaster deep-link destination.
    readonly property string castPermalink: presentationBridge.castShareUrl(
        CastViews.normalizeHash(root.modelData.castId || ""))

    readonly property string firstExternalUrl: {
        const values = root.effectiveEmbeds || []
        for (let index = 0; index < values.length; ++index) {
            const candidate = String(values[index].url || values[index].value || "")
            if (/^https?:\/\//i.test(candidate)) return candidate
        }
        const match = String(root.modelData.text || "").match(/https?:\/\/[^\s<>]+/i)
        return match && match.length ? String(match[0]).replace(/[.,!?;:]+$/, "") : ""
    }

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

    function ltrText(value) {
        const isolate = String.fromCharCode(0x2066)
        const pop = String.fromCharCode(0x2069)
        return isolate + String(value || "") + pop
    }

    function ltrHandle(value) {
        return root.ltrText("@" + String(value || ""))
    }

    function parentLabel() {
        if (root.hasParentPreview) {
            const handle = String(root.parentPreview.author || "")
            const name = String(root.parentPreview.displayName || handle)
            const target = handle.length > 0 ? "@" + handle : name
            if (target.length > 0)
                return qsTrId("genycaster.cast.replying_to_value").arg(target)
            return qsTrId("genycaster.cast.replying_to_another_cast")
        }
        if (root.hasParentCast)
            return qsTrId("genycaster.cast.replying_to_another_cast")
        return qsTrId("genycaster.cast.reply_in_a_conversation")
    }

    function openMoreMenu() {
        castMenuLoader.active = true
        if (castMenuLoader.item)
            castMenuLoader.item.openAt(moreButton, root.contentRtl)
    }

    readonly property string menuCastId: String(root.modelData.castId || "")
    onMenuCastIdChanged: if (castMenuLoader) castMenuLoader.active = false

    readonly property string formattedTime: {
        const localeRevision = localizationController.revision
        return Number(root.modelData.createdAtMs || 0) > 0
            ? LocaleFormat.formatTimestamp(Number(root.modelData.createdAtMs))
            : String(root.modelData.time || "")
    }

    Timer {
        id: parentHydrationDeadline
        interval: 4500
        repeat: false
        running: root.hasParentCast && !root.hasParentPreview && !root.parentHydrationTimedOut
        onTriggered: root.parentHydrationTimedOut = true
    }

    implicitHeight: messageRow.implicitHeight + Theme.space3
    Accessible.description: (root.modelData.castId !== undefined ? qsTrId("genycaster.cast.view_conversation") : "")
                            + (Number(root.modelData.embedCount || 0) > 1
                               ? " · " + qsTrId("genycaster.cast.value_embeds").arg(LocaleFormat.formatInteger(Number(root.modelData.embedCount || 0))) : "")

    RowLayout {
        id: messageRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.compact ? Theme.space2 : Theme.space3
        anchors.rightMargin: root.compact ? Theme.space2 : Theme.space3
        anchors.topMargin: Theme.space2
        anchors.bottomMargin: Theme.space1
        spacing: root.compact ? Theme.space1 : Theme.space3
        layoutDirection: Theme.uiDirection

        Item {
            visible: !root.compact
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: root.compact ? 0 : 42
            Layout.preferredHeight: root.compact ? 0 : 42
            Layout.topMargin: Theme.space1

            Avatar {
                anchors.fill: parent
                avatarSize: 42
                sourceUrl: root.modelData.avatarUrl || ""
                displayName: root.authorName
            }

            TapHandler {
                enabled: Number(root.modelData.authorFid || 0) > 0
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.profileRequested(Number(root.modelData.authorFid))
            }
        }

        Item {
            id: messageSurface
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: messageContent.implicitHeight + (root.compact ? Theme.space2 : Theme.space4) * 2

            // All feed families share one soft superellipse surface. Keeping
            // the wrapper outside media Loaders preserves delegate reuse.
            Superellipse {
                anchors.fill: parent
                fillColor: Theme.backgroundPrimary
                borderWidth: 0
                borderColor: "transparent"
                curveDepth: Theme.curveContainer
            }

            ColumnLayout {
                id: messageContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: root.compact ? Theme.space2 : Theme.space4
                anchors.rightMargin: root.compact ? Theme.space2 : Theme.space4
                anchors.topMargin: root.compact ? Theme.space2 : Theme.space4
                anchors.bottomMargin: root.compact ? Theme.space2 : Theme.space4
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    spacing: Theme.space1
                    layoutDirection: Theme.uiDirection

                    Item {
                        visible: root.compact
                        Layout.preferredWidth: visible ? 36 : 0
                        Layout.preferredHeight: visible ? 36 : 0
                        Layout.alignment: Qt.AlignVCenter

                        Avatar {
                            anchors.fill: parent
                            avatarSize: 36
                            sourceUrl: root.modelData.avatarUrl || ""
                            displayName: root.authorName
                        }

                        TapHandler {
                            enabled: Number(root.modelData.authorFid || 0) > 0
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.profileRequested(Number(root.modelData.authorFid))
                        }
                    }

                    // Two-line identity, with channel attribution on the NAME
                    // line and time alongside the handle. Mobile must not
                    // reserve a third line for channel metadata.
                    ColumnLayout {
                        id: castAuthorIdentity
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 1

                        RowLayout {
                            id: castNameAndChannel
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space1
                            layoutDirection: Theme.uiDirection

                            DirectionAwareLabel {
                                id: castAuthorName
                                Layout.minimumWidth: 0
                                // Never bind a child width to the arranging RowLayout width.
                                // This caused Qt 6 recursive rearrange warnings.
                                Layout.preferredWidth: Math.min(implicitWidth,
                                    root.castChannel.length > 0
                                        ? Math.max(78, root.width * 0.52)
                                        : Math.max(78, root.width * 0.76))
                                text: root.authorName
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight

                                TapHandler {
                                    enabled: Number(root.modelData.authorFid || 0) > 0
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.profileRequested(Number(root.modelData.authorFid))
                                }
                            }

                            RowLayout {
                                id: castInlineChannel
                                visible: root.castChannel.length > 0
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space1
                                layoutDirection: Theme.uiDirection

                                DirectionAwareLabel {
                                    text: qsTrId("genycaster.cast.in_channel")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                Avatar {
                                    Layout.preferredWidth: root.compact ? 18 : 20
                                    Layout.preferredHeight: Layout.preferredWidth
                                    avatarSize: Layout.preferredWidth
                                    sourceUrl: root.channelImageUrl
                                    forceAutomaticDownload: true
                                    displayName: root.castChannel
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    // Attribution uses the canonical channel slug, never its
                                    // mutable display title (e.g. /network-propaganda).
                                    forceLeftToRight: true
                                    text: "/" + root.castChannel
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                            }

                            Item {
                                visible: !castInlineChannel.visible
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                            }

                            // Mobile timestamp belongs on the same baseline as
                            // the author name, never below the handle or beside
                            // the bottom edge of the overflow action.
                            DirectionAwareLabel {
                                objectName: "compactCastAuthorMetadata"
                                visible: root.compact && Number(root.modelData.createdAtMs || 0) > 0
                                Layout.alignment: Qt.AlignVCenter
                                Layout.maximumWidth: Math.min(104, root.width * 0.25)
                                text: visible ? LocaleFormat.formatRelativeTime(
                                    Number(root.modelData.createdAtMs)) : ""
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                                forceLeftToRight: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Label {
                                visible: Boolean(root.modelData.author)
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                LayoutMirroring.enabled: false
                                text: root.ltrHandle(root.modelData.author || "")
                                font.family: Theme.latinFontFamily
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                horizontalAlignment: Theme.textStartAlignment
                                elide: Text.ElideRight
                                TapHandler {
                                    enabled: Number(root.modelData.authorFid || 0) > 0
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.profileRequested(Number(root.modelData.authorFid))
                                }
                            }

                            SourceClientBadge {
                                objectName: "castSourceClientBadge"
                                clientName: String(root.modelData.sourceClient || "")
                            }

                        }
                    }

                    ActionButton {
                        id: moreButton
                        Layout.alignment: Qt.AlignTop
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconMore
                        text: qsTrId("genycaster.cast.more_options")
                        onClicked: root.openMoreMenu()
                    }
                }

                RowLayout {
                    visible: root.hasWriteState
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                    Icon {
                        text: root.modelData.writeState === "confirmed" ? Theme.iconCheck
                              : root.modelData.writeState === "failed" ? Theme.iconWarning
                              : Theme.iconCloud
                        color: root.modelData.writeState === "confirmed" ? Theme.success
                               : root.modelData.writeState === "failed" ? Theme.warning : Theme.accent
                        preferredSize: Theme.typeCaptionSize
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.modelData.writeState === "confirmed"
                              ? (root.modelData.isReply ? qsTrId("genycaster.cast.reply_published") : qsTrId("genycaster.cast.published"))
                              : root.modelData.writeState === "failed"
                                ? (root.modelData.isReply ? qsTrId("genycaster.cast.reply_failed_saved_locally") : qsTrId("genycaster.cast.publish_failed_saved_locally"))
                                : root.modelData.writeState === "retry"
                                  ? qsTrId("genycaster.cast.queued_for_retry_saved_locally")
                                  : root.modelData.writeState === "blocked"
                                    ? qsTrId("genycaster.cast.waiting_for_write_authority_saved_locally")
                                    : (root.modelData.isReply ? qsTrId("genycaster.cast.replying_saved_locally") : qsTrId("genycaster.cast.publishing_saved_locally"))
                        color: root.modelData.writeState === "confirmed" ? Theme.success
                               : root.modelData.writeState === "failed" ? Theme.warning : Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightMedium
                        horizontalAlignment: root.contentRtl ? Text.AlignRight : Text.AlignLeft
                        elide: Text.ElideRight
                    }
                }

                Superellipse {
                    visible: root.hasContext
                    Layout.fillWidth: true
                    implicitHeight: contextLayout.implicitHeight + Theme.space2 * 2
                    // Muted inset with a narrow accent rail: reply context stays
                    // quieter than the cast body itself.
                    fillColor: Theme.surfaceMuted
                    borderWidth: 0
                    borderColor: "transparent"
                    curveDepth: Theme.curveCompact

                    RowLayout {
                        id: contextLayout
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        spacing: Theme.space2
                        layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                        Superellipse {
                            Layout.preferredWidth: 3
                            Layout.preferredHeight: Math.max(34, contextContent.implicitHeight)
                            fillColor: Theme.accent
                            curveDepth: 2
                        }

                        Avatar {
                            visible: root.hasParentPreview && Boolean(root.parentPreview.displayName || root.parentPreview.author || root.parentPreview.avatarUrl)
                            avatarSize: visible ? 28 : 0
                            sourceUrl: root.parentPreview.avatarUrl || ""
                            displayName: root.parentPreview.displayName || root.parentPreview.author || ""
                        }

                        ColumnLayout {
                            id: contextContent
                            Layout.fillWidth: true
                            spacing: 1

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.parentLabel()
                                color: Theme.accent
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }

                            DirectionAwareLabel {
                                visible: root.hasParentPreview && String(root.parentPreview.text || "").replace(/https?:\/\/[^\s<>]+/gi, " ").trim().length > 0
                                Layout.fillWidth: true
                                text: String(root.parentPreview.text || "").replace(/https?:\/\/[^\s<>]+/gi, " ").trim()
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                visible: !root.hasParentPreview
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                                GenySpinner {
                                    visible: root.hasParentCast && !root.parentHydrationTimedOut
                                    Layout.preferredWidth: visible ? 14 : 0
                                    Layout.preferredHeight: visible ? 14 : 0
                                    running: visible
                                    strokeWidth: 1.6
                                    color: Theme.inkMuted
                                }

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: root.hasParentCast
                                          ? (root.parentHydrationTimedOut
                                             ? qsTrId("genycaster.cast.open_original_cast")
                                             : qsTrId("genycaster.cast.loading_original_cast"))
                                          : qsTrId("genycaster.cast.open_conversation_context")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeMetaSize
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                visible: root.hasParentPreview
                                         && (Number(root.parentPreview.replyCount || 0) > 0
                                             || Number(root.parentPreview.recastCount || 0) > 0
                                             || Number(root.parentPreview.likeCount || 0) > 0)
                                Layout.fillWidth: true
                                spacing: Theme.space3
                                layoutDirection: Qt.LeftToRight

                                RowLayout {
                                    visible: Number(root.parentPreview.replyCount || 0) > 0
                                    spacing: 3
                                    layoutDirection: Qt.LeftToRight
                                    Icon { text: Theme.iconReply; color: Theme.inkSubtle; preferredSize: 12 }
                                    Label {
                                        text: LocaleFormat.formatInteger(Number(root.parentPreview.replyCount || 0))
                                        color: Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                                RowLayout {
                                    visible: Number(root.parentPreview.recastCount || 0) > 0
                                    spacing: 3
                                    layoutDirection: Qt.LeftToRight
                                    Icon { text: Theme.iconRepost; color: Theme.inkSubtle; preferredSize: 12 }
                                    Label {
                                        text: LocaleFormat.formatInteger(Number(root.parentPreview.recastCount || 0))
                                        color: Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                                RowLayout {
                                    visible: Number(root.parentPreview.likeCount || 0) > 0
                                    spacing: 3
                                    layoutDirection: Qt.LeftToRight
                                    Icon { text: Theme.iconHeart; color: Theme.inkSubtle; preferredSize: 12 }
                                    Label {
                                        text: LocaleFormat.formatInteger(Number(root.parentPreview.likeCount || 0))
                                        color: Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        Item {
                            visible: String(root.parentMediaPreview.source || "").length > 0
                            Layout.preferredWidth: visible ? (root.compact ? 64 : 78) : 0
                            Layout.preferredHeight: visible ? (root.compact ? 48 : 58) : 0
                            Layout.alignment: Qt.AlignVCenter

                            SuperellipseImage {
                                anchors.fill: parent
                                source: root.parentMediaPreview.source || ""
                                mediaCategory: "photos"
                                fillMode: Image.PreserveAspectCrop
                                fillColor: Theme.backgroundSecondary
                                borderWidth: 0
                                curveDepth: Theme.shapeMedia
                                showLoadingIndicator: false
                                showFailureIndicator: false
                            }

                            Superellipse {
                                visible: root.parentMediaPreview.family === "video"
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                fillColor: Qt.rgba(0, 0, 0, 0.58)
                                curveDepth: 12
                                Icon {
                                    anchors.centerIn: parent
                                    text: Theme.iconPlay
                                    color: "white"
                                    preferredSize: 12
                                }
                            }
                        }

                        Icon {
                            visible: root.hasParentCast
                            Layout.alignment: Qt.AlignVCenter
                            text: root.contentRtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.textTertiary
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    TapHandler {
                        enabled: root.hasParentCast
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.openRequested(String(root.parentCastId || root.parentPreview.castId || ""))
                    }
                }

                Loader {
                    id: castTextBody
                    objectName: "castTextBody"
                    active: root.displayText.length > 0 && !root.combinedLinkCast
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true

                    // Inline text owns its pointer events. A broad parent
                    // TapHandler also fires for @mentions/URL anchors and
                    // races the requested profile/link against opening the
                    // enclosing cast. Plain text still opens the conversation
                    // via RichInlineTextItem.plainTextActivated.
                    sourceComponent: Component {
                        RichCastRenderer {
                            castId: String(root.modelData.castId || "")
                            plainText: root.displayText
                            compact: root.compact
                            suppressedEmbedUrls: root.protocolEmbedUrls
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                            onBodyActivated: {
                                const castId = String(root.modelData.castId || "")
                                if (!root.scrollMoving && castId.length > 0)
                                    root.openRequested(castId)
                            }
                            onCastRequested: castId => root.openRequested(castId)
                        }
                    }
                }

                // Protocol cast order: author/parent, text, embedded media,
                // supporting cards and engagement. Never put images before the
                // author's text simply because the cast contains image embeds.
                Loader {
                    objectName: "castVisualMedia"
                    active: root.mediaFirst
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    sourceComponent: Component {
                        CastEmbedsPanel {
                            visualEmbeds: root.visualEmbeds
                            supportingEmbeds: []
                            compact: root.compact
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                            onCastRequested: castId => root.openRequested(castId)
                        }
                    }
                }

                RowLayout {
                    visible: root.mediaFirst
                    Layout.fillWidth: true
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    spacing: Theme.space2
                    layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                    EngagementBar {
                        Layout.fillWidth: true
                        compact: root.compact
                        scrollMoving: root.scrollMoving
                        enrichmentActive: root.enrichmentActive
                        showViews: true
                        replyCount: Number(root.modelData.replyCount || 0)
                        recastCount: Number(root.modelData.recastCount || 0)
                        likeCount: Number(root.modelData.likeCount || 0)
                        viewCount: root.modelData.viewCount === undefined ? null : root.modelData.viewCount
                        castId: String(root.modelData.castId || "")
                        castData: root.modelData
                        authorFid: Number(root.modelData.authorFid || 0)
                        castText: String(root.modelData.text || "")
                        contextText: String(root.modelData.text || "")
                        replyInteractive: root.interactionsAvailable
                                          && root.modelData.castId !== undefined
                                          && String(root.modelData.castId).length > 0
                        recastInteractive: replyInteractive
                        likeInteractive: replyInteractive
                        recastedByMe: Boolean(root.modelData.recastedByMe || false)
                        likedByMe: Boolean(root.modelData.likedByMe || false)
                        onReplyRequested: root.replyRequested(root.modelData)
                        onQuoteRequested: root.quoteRequested(root.modelData)
                        onRecastRequested: active => root.reactionRequested("recast", root.modelData, active)
                        onLikeRequested: active => root.reactionRequested("like", root.modelData, active)
                        onAuthenticationRequested: root.authenticationRequested()
                    }

                    DirectionAwareLabel {
                        visible: !root.compact && root.formattedTime.length > 0
                        text: root.formattedTime
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightRegular
                        elide: Text.ElideRight
                    }
                }

                Loader {
                    active: root.mediaFirst && root.supportingEmbeds.length > 0
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    sourceComponent: Component {
                        CastEmbedsPanel {
                            visualEmbeds: []
                            supportingEmbeds: root.supportingEmbeds
                            castCaption: root.combinedLinkCast ? root.displayText : ""
                            compact: root.compact
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                            onCastRequested: castId => root.openRequested(castId)
                        }
                    }
                }

                Loader {
                    active: !root.mediaFirst && root.supportingEmbeds.length > 0
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    sourceComponent: Component {
                        CastEmbedsPanel {
                            visualEmbeds: []
                            supportingEmbeds: root.supportingEmbeds
                            castCaption: root.combinedLinkCast ? root.displayText : ""
                            compact: root.compact
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                            onCastRequested: castId => root.openRequested(castId)
                        }
                    }
                }

                RowLayout {
                    visible: !root.mediaFirst
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                    EngagementBar {
                        Layout.fillWidth: true
                        compact: root.compact
                        scrollMoving: root.scrollMoving
                        enrichmentActive: root.enrichmentActive
                        showViews: true
                        replyCount: Number(root.modelData.replyCount || 0)
                        recastCount: Number(root.modelData.recastCount || 0)
                        likeCount: Number(root.modelData.likeCount || 0)
                        viewCount: root.modelData.viewCount === undefined ? null : root.modelData.viewCount
                        castId: String(root.modelData.castId || "")
                        castData: root.modelData
                        authorFid: Number(root.modelData.authorFid || 0)
                        castText: String(root.modelData.text || "")
                        contextText: String(root.modelData.text || "")
                        replyInteractive: root.interactionsAvailable
                                          && root.modelData.castId !== undefined
                                          && String(root.modelData.castId).length > 0
                        recastInteractive: replyInteractive
                        likeInteractive: replyInteractive
                        recastedByMe: Boolean(root.modelData.recastedByMe || false)
                        likedByMe: Boolean(root.modelData.likedByMe || false)
                        onReplyRequested: root.replyRequested(root.modelData)
                        onQuoteRequested: root.quoteRequested(root.modelData)
                        onRecastRequested: active => root.reactionRequested("recast", root.modelData, active)
                        onLikeRequested: active => root.reactionRequested("like", root.modelData, active)
                        onAuthenticationRequested: root.authenticationRequested()
                    }

                    DirectionAwareLabel {
                        visible: !root.compact && root.formattedTime.length > 0
                        text: root.formattedTime
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightRegular
                        elide: Text.ElideRight
                    }
                }
            }

            // Right-click is for selecting/copying content. Only the explicit
            // three-dot button opens Cast actions.
        }
    }

    // Cards own their outlines; double timeline dividers only add clutter.

    // Keep contextual actions uninstantiated until requested by the user.
    Loader {
        id: castMenuLoader
        active: false
        sourceComponent: Component {
            CastActionsMenu {
                castData: root.modelData
                accountFid: root.accountFid
                interactionsAvailable: root.interactionsAvailable
                onOpenRequested: castId => root.openRequested(castId)
                onProfileRequested: fid => root.profileRequested(fid)
                onRemoveRequested: cast => root.removeRequested(cast)
            }
        }
    }

}
