import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var modelData
    property bool compact: false
    property bool interactionsAvailable: false
    property double accountFid: 0
    property var scrollHost: null
    property bool supportingAdmitted: false
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)

    // Cast content owns its own bidi flow. The application shell may be RTL,
    // but a Latin cast must remain an LTR message and vice versa.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    signal openRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
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

    function synchronizeSupportingAdmission() {
        if (root.supportingEmbeds.length === 0) {
            root.supportingAdmitted = false
            return
        }
        if (!root.scrollMoving)
            root.supportingAdmitted = true
    }

    Component.onCompleted: {
        root.resolveScrollHost()
        root.synchronizeSupportingAdmission()
    }
    onParentChanged: root.resolveScrollHost()
    onScrollMovingChanged: root.synchronizeSupportingAdmission()
    onSupportingEmbedsChanged: {
        // A recycled row must not retain the previous cast's rich/link subtree.
        // If reuse happens mid-flick, reserve geometry and defer the replacement
        // until movement ends instead of doing classification/Loader work now.
        root.supportingAdmitted = false
        root.synchronizeSupportingAdmission()
    }

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
        const menu = castMenuLoader.item
        if (!menu)
            return
        const point = moreButton.mapToItem(Overlay.overlay, 0, moreButton.height + Theme.space1)
        const preferredX = root.contentRtl ? point.x : point.x + moreButton.width - menu.width
        menu.x = Math.max(Theme.space2, Math.min(Overlay.overlay.width - menu.width - Theme.space2, preferredX))
        menu.y = Math.max(Theme.space2, Math.min(Overlay.overlay.height - menu.height - Theme.space2, point.y))
        menu.open()
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

    implicitHeight: messageRow.implicitHeight + Theme.space2
    Accessible.description: (root.modelData.castId !== undefined ? qsTrId("genycaster.cast.view_conversation") : "")
                            + (Number(root.modelData.embedCount || 0) > 1
                               ? " · " + qsTrId("genycaster.cast.value_embeds").arg(LocaleFormat.formatInteger(Number(root.modelData.embedCount || 0))) : "")

    RowLayout {
        id: messageRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.compact ? Theme.mobileGutter : Theme.space3
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space3
        anchors.topMargin: Theme.space2
        anchors.bottomMargin: Theme.space1
        spacing: root.compact ? Theme.space1 : Theme.space3
        layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

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
            implicitHeight: messageContent.implicitHeight + Theme.space2 * 2

            ColumnLayout {
                id: messageContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: root.compact ? 0 : Theme.space2
                anchors.rightMargin: root.compact ? 0 : Theme.space2
                anchors.topMargin: Theme.space1
                anchors.bottomMargin: Theme.space2
                spacing: Theme.space2
                layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    spacing: Theme.space1
                    layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight

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

                    DirectionAwareLabel {
                        // Never derive a Layout constraint from the RowLayout's own
                        // width. That creates a circular size dependency and causes
                        // Qt Quick Layouts to recursively rearrange the row.
                        Layout.maximumWidth: Math.max(100, messageSurface.width * 0.48)
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

                    Label {
                        visible: Boolean(root.modelData.author)
                        Layout.maximumWidth: 150
                        LayoutMirroring.enabled: false
                        text: root.ltrHandle(root.modelData.author)
                        font.family: Theme.latinFontFamily
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeMetaSize
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }


                    Item { Layout.fillWidth: true }

                    ActionButton {
                        id: moreButton
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconMore
                        text: qsTrId("genycaster.cast.view_conversation")
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
                            text: root.contentRtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.inkSubtle
                            preferredSize: Theme.typeCaptionSize
                        }
                    }

                    TapHandler {
                        enabled: root.hasParentCast
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.openRequested(String(root.parentCastId || root.parentPreview.castId || ""))
                    }
                }

                Loader {
                    objectName: "castTextBody"
                    active: root.displayText.length > 0
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    sourceComponent: Component {
                        RichCastRenderer {
                            castId: String(root.modelData.castId || "")
                            plainText: root.displayText
                            compact: root.compact
                            suppressedEmbedUrls: root.protocolEmbedUrls
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
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
                        replyCount: Number(root.modelData.replyCount || 0)
                        recastCount: Number(root.modelData.recastCount || 0)
                        likeCount: Number(root.modelData.likeCount || 0)
                        contextText: String(root.modelData.text || "")
                        replyInteractive: root.interactionsAvailable
                                          && root.modelData.castId !== undefined
                                          && String(root.modelData.castId).length > 0
                        recastInteractive: replyInteractive
                        likeInteractive: replyInteractive
                        recastedByMe: Boolean(root.modelData.recastedByMe || false)
                        likedByMe: Boolean(root.modelData.likedByMe || false)
                        onReplyRequested: root.replyRequested(root.modelData)
                        onRecastRequested: active => root.reactionRequested("recast", root.modelData, active)
                        onLikeRequested: active => root.reactionRequested("like", root.modelData, active)
                        onAuthenticationRequested: root.authenticationRequested()
                    }

                    DirectionAwareLabel {
                        visible: root.formattedTime.length > 0
                        text: root.formattedTime
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightRegular
                        elide: Text.ElideRight
                    }
                }

                Loader {
                    active: root.mediaFirst && root.supportingEmbeds.length > 0
                            && root.supportingAdmitted
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    Layout.leftMargin: root.mediaContentInset
                    Layout.rightMargin: root.mediaContentInset
                    sourceComponent: Component {
                        CastEmbedsPanel {
                            visualEmbeds: []
                            supportingEmbeds: root.supportingEmbeds
                            compact: root.compact
                            onExternalRequested: url => root.externalRequested(url)
                            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                            onCastRequested: castId => root.openRequested(castId)
                        }
                    }
                }

                Loader {
                    active: !root.mediaFirst && root.supportingEmbeds.length > 0
                            && root.supportingAdmitted
                    visible: active
                    asynchronous: true
                    Layout.fillWidth: true
                    sourceComponent: Component {
                        CastEmbedsPanel {
                            visualEmbeds: []
                            supportingEmbeds: root.supportingEmbeds
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
                        replyCount: Number(root.modelData.replyCount || 0)
                        recastCount: Number(root.modelData.recastCount || 0)
                        likeCount: Number(root.modelData.likeCount || 0)
                        contextText: String(root.modelData.text || "")
                        replyInteractive: root.interactionsAvailable
                                          && root.modelData.castId !== undefined
                                          && String(root.modelData.castId).length > 0
                        recastInteractive: replyInteractive
                        likeInteractive: replyInteractive
                        recastedByMe: Boolean(root.modelData.recastedByMe || false)
                        likedByMe: Boolean(root.modelData.likedByMe || false)
                        onReplyRequested: root.replyRequested(root.modelData)
                        onRecastRequested: active => root.reactionRequested("recast", root.modelData, active)
                        onLikeRequested: active => root.reactionRequested("like", root.modelData, active)
                        onAuthenticationRequested: root.authenticationRequested()
                    }

                    DirectionAwareLabel {
                        visible: root.formattedTime.length > 0
                        text: root.formattedTime
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightRegular
                        elide: Text.ElideRight
                    }
                }
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.openMoreMenu()
            }
        }
    }

    Rectangle {
        visible: root.compact
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        height: Theme.strokeHairline
        color: Theme.divider
    }

    Rectangle {
        visible: !root.compact
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.contentRtl ? Theme.space3 : 58
        anchors.rightMargin: root.contentRtl ? 58 : Theme.space3
        height: Theme.strokeHairline
        color: Theme.divider
    }

    // These controls are unrelated to rendering a feed row. Construct them only
    // on explicit interaction, and release them when that row is recycled.
    Loader {
        id: castMenuLoader
        active: false
        sourceComponent: Component {
            Popup {
                id: castMenu
                objectName: "castMenu"
                parent: Overlay.overlay
                width: 224
                implicitHeight: menuContent.implicitHeight + Theme.space2
                padding: Theme.space1
                // A contextual menu is an input boundary. Modeless popups let
                // touch events reach the Cast below the menu on mobile WASM.
                modal: true
                dim: false
                focus: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                background: PopupSurface { curveDepth: Theme.curveContainer }

                contentItem: ColumnLayout {
                    id: menuContent
                    spacing: 0

                    ActionButton {
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconComment
                        text: qsTrId("genycaster.cast.view_conversation")
                        onClicked: {
                            castMenu.close()
                            root.openRequested(String(root.modelData.castId || ""))
                        }
                    }

                    ActionButton {
                        visible: Number(root.modelData.authorFid || 0) > 0
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconUser
                        text: qsTrId("genycaster.cast.open_author_profile")
                        onClicked: {
                            castMenu.close()
                            root.profileRequested(Number(root.modelData.authorFid))
                        }
                    }

                    ActionButton {
                        visible: String(root.modelData.text || "").length > 0
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconCopy
                        text: qsTrId("genycaster.cast.copy_text")
                        onClicked: {
                            presentationBridge.copyText(String(root.modelData.text || ""))
                            castMenu.close()
                        }
                    }

                    ActionButton {
                        visible: root.firstExternalUrl.length > 0
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconLink
                        text: qsTrId("genycaster.cast.copy_link")
                        onClicked: {
                            presentationBridge.copyText(root.firstExternalUrl)
                            castMenu.close()
                        }
                    }

                    ActionButton {
                        visible: String(root.modelData.castId || "").length > 0
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        iconGlyph: Theme.iconCopy
                        text: qsTrId("genycaster.cast.copy_cast_id")
                        onClicked: {
                            presentationBridge.copyText(String(root.modelData.castId || ""))
                            castMenu.close()
                        }
                    }

                    Rectangle {
                        visible: root.canDelete
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.divider
                    }

                    ActionButton {
                        visible: root.canDelete
                        enabled: root.canDeleteNow
                        Layout.fillWidth: true
                        quiet: true
                        alignStart: true
                        destructive: true
                        iconGlyph: Theme.iconClose
                        text: qsTrId("genycaster.cast.delete")
                        onClicked: {
                            castMenu.close()
                            root.removeRequested(root.modelData)
                        }
                    }
                }
            }
        }
    }
}
