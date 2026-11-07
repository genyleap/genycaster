import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    objectName: "engagementBar"
    // Mirror the interaction clusters as a whole for RTL. Keep the
    // individual action's icon/count cluster controlled by EngagementButton.
    layoutDirection: Theme.uiDirection

    property bool compact: false
    function closeShareMenu() {
        if (castShareMenu.item)
            castShareMenu.item.close()
    }
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
    // The owning viewport/delegate controls secondary network interest.
    property bool enrichmentActive: true
    readonly property var viewDescriptor: root.castData && typeof root.castData === "object"
        ? root.castData : ({})
    readonly property string descriptorCastId:
        CastViews.normalizeHash(root.viewDescriptor.castId || "")
    readonly property string reactionCastId:
        CastViews.normalizeHash(root.viewDescriptor.castId || root.castId)
    readonly property double viewerFid: Number(presentationBridge.accountFid || 0)
    readonly property bool browserLikeEvidence: Qt.platform.os === "wasm"
                                                && CastReactions.reacted(root.reactionCastId, root.viewerFid, "like")
    readonly property bool browserRecastEvidence: Qt.platform.os === "wasm"
                                                  && CastReactions.reacted(root.reactionCastId, root.viewerFid, "recast")
    readonly property var browserLikeOverride: Qt.platform.os === "wasm"
        ? CastReactions.localValue(root.reactionCastId, root.viewerFid, "like") : null
    readonly property var browserRecastOverride: Qt.platform.os === "wasm"
        ? CastReactions.localValue(root.reactionCastId, root.viewerFid, "recast") : null
    readonly property bool effectiveLikedByMe: root.browserLikeOverride !== null
        ? root.browserLikeOverride : root.likedByMe || root.browserLikeEvidence
    readonly property bool effectiveRecastedByMe: root.browserRecastOverride !== null
        ? root.browserRecastOverride : root.recastedByMe || root.browserRecastEvidence

    // Browser positive evidence has the same explicit viewport lifetime as
    // native Views. A pooled row releases its queued demand immediately.
    readonly property string browserInterestKey: Qt.platform.os === "wasm"
        && root.enrichmentActive && root.visible && !root.scrollMoving
        && presentationBridge.isAuthenticated && root.viewerFid > 0
        && root.reactionCastId.length > 0
        ? String(root.viewerFid) + ":" + root.reactionCastId : ""
    property string retainedBrowserInterestKey: ""
    // Desktop/mobile use the provider-routed native acquisition, while WASM
    // retains its existing browser reaction adapter. Never request both.
    readonly property string nativeInterestKey: Qt.platform.os !== "wasm"
        && root.enrichmentActive && root.visible && !root.scrollMoving
        && presentationBridge.isAuthenticated && root.viewerFid > 0
        && root.reactionCastId.length > 0
        && Number(root.viewDescriptor.authorFid || root.authorFid) > 0
        ? String(root.viewerFid) + ":" + root.reactionCastId : ""
    property string retainedNativeInterestKey: ""

    function syncNativeInterest() {
        const next = root.nativeInterestKey
        if (root.retainedNativeInterestKey === next)
            return
        const previous = root.retainedNativeInterestKey
        if (previous.length > 0) {
            const separator = previous.indexOf(":")
            presentationBridge.releaseViewerReactionInterest(
                previous.slice(separator + 1), Number(previous.slice(0, separator)))
        }
        root.retainedNativeInterestKey = next
        if (next.length > 0) {
            presentationBridge.retainViewerReactionInterest(
                root.reactionCastId, Number(root.viewDescriptor.authorFid || root.authorFid),
                root.viewerFid)
        }
    }
    onNativeInterestKeyChanged: syncNativeInterest()

    function syncBrowserInterest() {
        const next = root.browserInterestKey
        if (next === root.retainedBrowserInterestKey)
            return
        const previous = root.retainedBrowserInterestKey
        if (previous.length > 0) {
            const separator = previous.indexOf(":")
            CastReactions.release(previous.slice(separator + 1),
                                  Number(previous.slice(0, separator)))
        }
        root.retainedBrowserInterestKey = next
        if (next.length > 0)
            CastReactions.retain(root.reactionCastId, root.viewerFid)
    }

    function requestBrowserReactions() {
        syncBrowserInterest()
        if (root.browserInterestKey.length > 0)
            CastReactions.ensure(root.reactionCastId, root.viewerFid)
    }
    onViewerFidChanged: requestBrowserReactions()
    onBrowserInterestKeyChanged: requestBrowserReactions()
    // Every visible/near-visible consumer retains just its own cast. The C++
    // controller removes queued work when the last delegate releases interest.
    readonly property bool hasCanonicalViewCount:
        root.viewCount !== null && root.viewCount !== undefined
        && isFinite(Number(root.viewCount)) && Number(root.viewCount) >= 0
    readonly property bool wantsDerivedViews: root.showViews && root.enrichmentActive
        && root.visible && !root.hasCanonicalViewCount
        && Number(root.viewDescriptor.authorFid || 0) > 0
        && root.descriptorCastId.length > 0
    readonly property var castViewEntry: root.wantsDerivedViews
        ? (castViewController ? castViewController.entryFor(root.descriptorCastId) : null) : null
    property string retainedViewsCastId: ""

    readonly property var resolvedViewCount: root.hasCanonicalViewCount
        ? root.viewCount : root.castViewEntry ? root.castViewEntry.count : null

    function syncViewInterest() {
        const next = root.wantsDerivedViews ? root.descriptorCastId : ""
        if (root.retainedViewsCastId === next)
            return
        if (root.retainedViewsCastId.length > 0 && castViewController)
            castViewController.release(root.retainedViewsCastId)
        root.retainedViewsCastId = next
        if (next.length > 0 && castViewController)
            castViewController.retain(next)
    }

    function requestViews() {
        // One atomic castData map supplies identity, author and text. Separate
        // QML property bindings can otherwise transiently cross cast identities.
        if (root.wantsDerivedViews && !root.scrollMoving && castViewController)
            castViewController.ensure(root.descriptorCastId,
                                      Number(root.viewDescriptor.authorFid || 0),
                                      String(root.viewDescriptor.text || ""))
    }

    Component.onCompleted: {
        syncViewInterest()
        requestViews()
        requestBrowserReactions()
        syncNativeInterest()
    }
    Component.onDestruction: {
        if (root.retainedViewsCastId.length > 0 && castViewController)
            castViewController.release(root.retainedViewsCastId)
        if (root.retainedBrowserInterestKey.length > 0) {
            const previous = root.retainedBrowserInterestKey
            const separator = previous.indexOf(":")
            CastReactions.release(previous.slice(separator + 1),
                                  Number(previous.slice(0, separator)))
        }
        if (root.retainedNativeInterestKey.length > 0) {
            const previous = root.retainedNativeInterestKey
            const separator = previous.indexOf(":")
            presentationBridge.releaseViewerReactionInterest(
                previous.slice(separator + 1), Number(previous.slice(0, separator)))
        }
    }
    onCastDataChanged: {
        syncViewInterest()
        requestViews()
        requestBrowserReactions()
        syncNativeInterest()
    }
    onEnrichmentActiveChanged: {
        syncViewInterest()
        requestViews()
        requestBrowserReactions()
        syncNativeInterest()
    }
    onVisibleChanged: {
        syncViewInterest()
        requestViews()
        requestBrowserReactions()
        syncNativeInterest()
    }
    onWantsDerivedViewsChanged: {
        syncViewInterest()
        requestViews()
    }
    // The descriptor is read atomically from castData, not from these
    // independently updating compatibility properties.
    onScrollMovingChanged: {
        requestBrowserReactions()
        syncNativeInterest()
        if (!root.scrollMoving)
            requestViews()
    }
    property bool replyInteractive: false
    property bool recastInteractive: false
    property bool likeInteractive: false
    property bool recastedByMe: false
    property bool likedByMe: false
    // Context stays available for future protocol-specific reactions, but the
    // primary Like action always uses one recognisable heart across all feeds.
    property string contextText: ""
    readonly property string displayedLikeIcon: Theme.iconHeart
    readonly property string normalizedCastId: CastViews.normalizeHash(root.viewDescriptor.castId || root.castId)
    readonly property bool isBookmarked: presentationBridge.bookmarkedCastIds.indexOf(root.normalizedCastId) >= 0

    signal replyRequested()
    signal quoteRequested()
    signal recastRequested(bool active)
    signal likeRequested(bool active)
    signal authenticationRequested()

    // Four tight reaction actions on the left, with Bookmark and Share pinned
    // to the right. Every tappable target remains at least 44px.
    spacing: root.compact ? 0 : Theme.space1
    Accessible.description: qsTrId("genycaster.engagement.value_replies").arg(LocaleFormat.formatInteger(root.replyCount))

    RowLayout {
        id: reactionActions
        objectName: "castReactionActions"
        Layout.alignment: Qt.AlignVCenter
        layoutDirection: Theme.uiDirection
        spacing: root.compact ? 0 : Theme.space1

    EngagementButton {
        objectName: "castReplyButton"
        Layout.minimumWidth: Theme.touchTarget
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
        Layout.minimumWidth: Theme.touchTarget
        iconGlyph: Theme.iconQuote
        enabled: true
        Accessible.name: qsTrId("genycaster.composer.quote")
        onClicked: root.quoteRequested()
    }

    EngagementButton {
        Layout.minimumWidth: Theme.touchTarget
        iconGlyph: Theme.iconRepost
        metricText: root.recastCount > 0 ? LocaleFormat.formatCompactNumber(root.recastCount) : ""
        active: root.effectiveRecastedByMe
        activeColor: Theme.positive
        enabled: !presentationBridge.isAuthenticated || !presentationBridge.writeBusy
        Accessible.name: root.effectiveRecastedByMe
                         ? qsTrId("genycaster.engagement.undo_recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
                         : qsTrId("genycaster.engagement.recast_value_recasts").arg(LocaleFormat.formatInteger(root.recastCount))
        onClicked: {
            if (!root.recastInteractive) {
                // An in-flight signed operation temporarily changes write
                // readiness. Never interpret that as revoked authorization.
                if (presentationBridge.isAuthenticated &&
                    (presentationBridge.writeBusy || presentationBridge.signerBusy
                     || presentationBridge.signerPhase === "checking"))
                    return
                root.authenticationRequested()
                return
            }
            const next = !root.effectiveRecastedByMe
            if (Qt.platform.os === "wasm" && root.viewerFid > 0)
                CastReactions.setLocal(root.reactionCastId, root.viewerFid, "recast", next)
            root.recastRequested(next)
        }
    }

    EngagementButton {
        Layout.minimumWidth: Theme.touchTarget
        iconGlyph: root.displayedLikeIcon
        iconFilled: root.effectiveLikedByMe
        activeColor: Theme.negative
        metricText: root.likeCount > 0 ? LocaleFormat.formatCompactNumber(root.likeCount) : ""
        active: root.effectiveLikedByMe
        enabled: !presentationBridge.isAuthenticated || !presentationBridge.writeBusy
        Accessible.name: root.effectiveLikedByMe
                         ? qsTrId("genycaster.engagement.unlike_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
                         : qsTrId("genycaster.engagement.like_value_likes").arg(LocaleFormat.formatInteger(root.likeCount))
        onClicked: {
            if (!root.likeInteractive) {
                if (presentationBridge.isAuthenticated &&
                    (presentationBridge.writeBusy || presentationBridge.signerBusy
                     || presentationBridge.signerPhase === "checking"))
                    return
                root.authenticationRequested()
                return
            }
            const next = !root.effectiveLikedByMe
            if (Qt.platform.os === "wasm" && root.viewerFid > 0)
                CastReactions.setLocal(root.reactionCastId, root.viewerFid, "like", next)
            root.likeRequested(next)
        }
    }

        RowLayout {
            id: castViewsLabel
            objectName: "castViewCount"
            visible: root.showViews && root.resolvedViewCount !== null && root.resolvedViewCount !== undefined
                     && Number(root.resolvedViewCount) >= 0 && isFinite(Number(root.resolvedViewCount))
            Layout.minimumWidth: Theme.iconSizeSmall + Theme.space2
            Layout.alignment: Qt.AlignVCenter
            spacing: root.compact ? 2 : Theme.space1
            Accessible.role: Accessible.StaticText

            Accessible.name: qsTrId("genycaster.engagement.views_accessible").arg(viewMetric.text)

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

        }

    }

    // Flexible space keeps utility actions at the physical trailing edge.
    Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

    RowLayout {
        id: utilityActions
        objectName: "castUtilityActions"
        Layout.alignment: Qt.AlignVCenter
        layoutDirection: Theme.uiDirection
        spacing: root.compact ? 0 : Theme.space1

    EngagementButton {
        id: bookmarkButton
        objectName: "castBookmarkButton"
        // Keep the old inline bookmark even on compact/mobile layouts.
        Layout.minimumWidth: Theme.touchTarget
        iconGlyph: Theme.iconBookmark
        iconFilled: root.isBookmarked
        active: root.isBookmarked
        activeColor: Theme.accent
        enabled: root.normalizedCastId.length > 0 && presentationBridge.isAuthenticated
        Accessible.name: root.isBookmarked ? "Remove bookmark" : "Bookmark cast"
        onClicked: {
            const adding = !root.isBookmarked
            presentationBridge.toggleBookmark(root.castData)
            if (adding)
                openProofConnections.requestBookmarkCollectionChoice(root.normalizedCastId)
        }
    }
    // A single lazy menu per expanded cast. Desktop uses an anchored
    // popover; a narrow viewport uses a bottom sheet tied to the overlay,
    // never coordinates projected from a recycled feed delegate.
    EngagementButton {
        id: castShareButton
        objectName: "castShareButton"
        Layout.minimumWidth: Theme.touchTarget
        iconGlyph: Theme.iconShare
        enabled: root.normalizedCastId.length > 0
        Accessible.name: qsTrId("genycaster.share.cast")
        onClicked: {
            const overlay = Overlay.overlay
            // Recycled cast delegates can outlive their window overlay during
            // teardown; never dereference a destroyed overlay.
            if (!overlay) return
            castShareMenu.active = true
            const menu = castShareMenu.item
            if (!menu) return
            if (overlay.width >= Theme.breakpointMobile) {
                const point = castShareButton.mapToItem(
                    overlay, 0, castShareButton.height + Theme.space1)
                menu.x = Math.max(Theme.space2, Math.min(
                    overlay.width - menu.width - Theme.space2,
                    point.x + castShareButton.width - menu.width))
                menu.y = Math.max(Theme.space2, Math.min(
                    overlay.height - menu.height - Theme.space2, point.y))
            }
            menu.open()
        }

        Loader {
            id: castShareMenu
            active: false
            // Cast compactness is per-card, not a window breakpoint.
            // Desktop Bookmarks render dense casts but still need a popup.
            sourceComponent: Overlay.overlay && Overlay.overlay.width > 0
                             && Overlay.overlay.width < Theme.breakpointMobile
                ? mobileCastShareSheet : desktopCastSharePopup
        }
    }

    Component {
        id: desktopCastSharePopup
        Popup {
            objectName: "castShareMenu"
            parent: Overlay.overlay
            width: Theme.scaleUi(248)
            implicitHeight: shareActionsLoader.implicitHeight + Theme.space2
            padding: Theme.space2
            modal: true
            focus: true
            dim: false
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            background: PopupSurface { curveDepth: Theme.curveContainer }
            contentItem: Loader {
                id: shareActionsLoader
                sourceComponent: castShareActionList
            }
            enter: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 0.6
                    to: 1.0
                    duration: Theme.reduceMotion ? 0 : 170
                    easing.type: Easing.OutCubic
                }
            }
            exit: Transition {
                NumberAnimation {
                    property: "opacity"
                    to: 0
                    duration: Theme.reduceMotion ? 0 : 135
                }
            }
        }
    }

    Component {
        id: mobileCastShareSheet
        GenyDrawer {
            objectName: "castShareMobileDrawer"
            bottomSheet: true
            showHandle: true
            dimBackground: true
            // Gesture-dismissal matches the exposed sheet handle.
            interactive: true
            width: parent ? parent.width : 390
            // Size the sheet to its actual actions. A fixed 352px panel left
            // a dead shelf below the last row and a large gap after the title.
            height: parent
                ? Math.min(parent.height * 0.82,
                           Math.max(Theme.scaleUi(226),
                                    shareSheetContents.implicitHeight + topPadding + bottomPadding))
                : Theme.scaleUi(286)
            contentPadding: Theme.space2
            topPadding: Theme.space5
            bottomPadding: Theme.space3
            contentItem: ColumnLayout {
                id: shareSheetContents
                spacing: Theme.space1

                ModalHeader {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.scaleUi(34)
                    title: qsTrId("genycaster.share.cast")
                    sheetPresentation: true
                    onCloseRequested: root.closeShareMenu()
                }
                // Quiet section boundary. The action rows begin immediately
                // below this line, not after another empty header-sized gap.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.strokeHairline
                    color: Theme.borderSubtle
                }
                Loader {
                    Layout.fillWidth: true
                    sourceComponent: castShareActionList
                }
            }
        }
    }

    Component {
        id: castShareActionList
        ColumnLayout {
            spacing: Theme.space1

            ActionButton {
                Layout.fillWidth: true
                compact: root.compact
                quiet: true
                alignStart: true
                iconGlyph: Theme.iconShare
                text: qsTrId("genycaster.share.share_action")
                onClicked: {
                    root.closeShareMenu()
                    presentationBridge.shareUrl(presentationBridge.castShareUrl(
                        root.normalizedCastId))
                }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: root.compact
                quiet: true
                alignStart: true
                iconGlyph: Theme.iconCopy
                text: qsTrId("genycaster.share.copy_link")
                onClicked: {
                    presentationBridge.copyText(
                        presentationBridge.castShareUrl(root.normalizedCastId))
                    root.closeShareMenu()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: root.compact
                quiet: true
                alignStart: true
                iconGlyph: Theme.iconCopy
                text: qsTrId("genycaster.share.copy_farcaster_link")
                onClicked: {
                    presentationBridge.copyText(
                        presentationBridge.farcasterWebCastUrl(
                            root.normalizedCastId,
                            String(root.castData.author || "")))
                    root.closeShareMenu()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: root.compact
                quiet: true
                alignStart: true
                iconGlyph: Theme.iconArrowUpRight
                text: qsTrId("genycaster.share.open_quorum")
                onClicked: {
                    root.closeShareMenu()
                    presentationBridge.openQuorumCast(root.normalizedCastId)
                }
            }
            ActionButton {
                visible: presentationBridge.farcasterWebCastUrl(
                    root.normalizedCastId,
                    String(root.castData.author || "")).length > 0
                Layout.fillWidth: true
                compact: root.compact
                quiet: true
                alignStart: true
                iconGlyph: Theme.iconArrowUpRight
                text: qsTrId("genycaster.share.open_farcaster")
                onClicked: {
                    root.closeShareMenu()
                    presentationBridge.openFarcasterCast(
                        root.normalizedCastId,
                        String(root.castData.author || ""))
                }
            }
        }
    }
    }

}
