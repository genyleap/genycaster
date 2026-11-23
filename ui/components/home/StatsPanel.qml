import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Stats is a presentation surface. All provider requests and calculations
// remain in StatsController; no fake deltas, impression counts or snapshot history.
ColumnLayout {
    id: root
    objectName: "homeStatsPanel"
    spacing: Theme.space3

    property int periodDays: 30
    property bool compact: false
    // Entitlements are verified by the account controller and scoped to FID
    // by HomeDashboard. Unknown is never promoted to an unlocked account.
    property bool proUnlocked: false
    readonly property var proMembership: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool proStatusKnown: proMembership.status === "verified"
    property string section: "social"
    property real sectionArrival: 1

    // The section's content arrives with the same restrained shift and fade
    // used for navigation. The shared tabs/header themselves remain stable.
    NumberAnimation {
        id: sectionEntrance
        target: root
        property: "sectionArrival"
        from: 0
        to: 1
        duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
        easing.type: Easing.OutCubic
    }

    onSectionChanged: {
        sectionEntrance.stop()
        if (!tabActive || Theme.reduceMotion)
            sectionArrival = 1
        else {
            sectionArrival = 0
            sectionEntrance.start()
        }
        if (tabActive) {
            if (section === "market" && root.proUnlocked) marketController.refresh(false)
        }
    }
    onTabActiveChanged: {
        if (tabActive) {
            if (section === "market" && root.proUnlocked) marketController.refresh(false)
        }
    }
    property bool tabActive: false
    opacity: tabActive ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis; easing.type: Easing.OutCubic }
    }
    transform: Translate {
        y: root.tabActive ? 0 : 7
        Behavior on y {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis; easing.type: Easing.OutCubic }
        }
    }
    readonly property string fetchState: statsController.state
    readonly property bool loading: fetchState === "loading"
    readonly property bool loaded: fetchState === "ready" || fetchState === "partial"
    readonly property bool partial: fetchState === "partial"
    readonly property var metrics: statsController.summary
    readonly property var rangeDays: [7, 30, 90, 365]

    function rangeLabel(days) {
        if (days === 7) return qsTrId("genycaster.stats.range_week")
        if (days === 30) return qsTrId("genycaster.stats.range_month")
        if (days === 90) return qsTrId("genycaster.stats.range_quarter")
        return qsTrId("genycaster.stats.range_year")
    }
    function rangeIndex(days) {
        return Math.max(0, rangeDays.indexOf(days))
    }
    // A concise, locale-aware report from the selected account and period.
    // This uses measured data only: the period filters publication dates,
    // while engagement reflects interactions observed to date. The fixed
    // English footer is a brand signature, not a second language setting.
    // Single line breaks keep the entire report in ONE rich paragraph.
    // Blank-line Markdown paragraphs created multiple full-height editors
    // with huge gaps between statistics in the visual Composer.
    function buildShareText() {
        if (section !== "social" || !loaded) return ""
        const integer = value => LocaleFormat.formatInteger(Number(value || 0))
        const heading = qsTrId("genycaster.stats.share_report_title").arg(rangeLabel(periodDays))
        const paragraphs = ["**" + heading + "**"]
        // Only the heading and lead metric use inline Markdown emphasis.
        // They remain in a single paragraph rather than becoming separate
        // full-height editable cards. Published plain text stays readable.
        if (Boolean(metrics.hasEngagement))
            paragraphs.push("**" + qsTrId("genycaster.stats.share_engagement_line")
                .arg(integer(metrics.engagement)) + "**")
        paragraphs.push(qsTrId("genycaster.stats.share_publishing_line")
            .arg(integer(metrics.casts)).arg(integer(metrics.replies)))
        if (Boolean(metrics.hasEngagement)) {
            paragraphs.push(
                qsTrId("genycaster.stats.share_reactions_line")
                    .arg(integer(metrics.likes))
                    .arg(integer(metrics.recasts))
                    .arg(integer(metrics.receivedReplies)),
                qsTrId("genycaster.stats.share_scope_note")
            )
        }
        paragraphs.push(
            qsTrId("genycaster.stats.share_prepared")
                .arg(LocaleFormat.formatDateTime(Date.now())),
            "Powered by Genycaster"
        )
        return paragraphs.join("\n")
    }

    property bool manualMarketRefresh: false
    property string marketRefreshFeedback: ""
    readonly property bool marketRequestBusy: marketController.state === "loading"
                                                   || manualMarketRefresh

    // A network/cache hit can finish in one frame. Keep the busy state visible
    // long enough for the user to perceive the refresh and its final outcome.
    function settleMarketRefresh() {
        if (!manualMarketRefresh || minimumRefreshDisplay.running
                || marketController.state === "loading")
            return
        manualMarketRefresh = false
        marketRefreshFeedback = marketController.state === "ready"
            ? qsTrId("genycaster.market.checked")
            : marketController.state === "partial"
              ? qsTrId("genycaster.market.partial")
              : qsTrId("genycaster.market.refresh_failed")
        dismissRefreshFeedback.restart()
    }

    function requestMarketRefresh() {
        if (!root.proUnlocked) return
        dismissRefreshFeedback.stop()
        const accepted = marketController.refresh(true)
        if (!accepted) {
            marketRefreshFeedback = qsTrId("genycaster.market.refresh_unavailable")
            dismissRefreshFeedback.restart()
            return
        }
        manualMarketRefresh = true
        marketRefreshFeedback = qsTrId("genycaster.market.checking")
        minimumRefreshDisplay.restart()
    }

    Timer {
        id: minimumRefreshDisplay
        interval: Theme.reduceMotion ? 200 : 850
        repeat: false
        onTriggered: root.settleMarketRefresh()
    }
    Timer {
        id: dismissRefreshFeedback
        interval: 2800
        repeat: false
        onTriggered: root.marketRefreshFeedback = ""
    }
    Timer {
        // Poll only when the user is actually viewing Market; the first-party
        // aggregator remains responsible for shared TTL/rate limiting.
        interval: 300000
        repeat: true
        running: root.tabActive && root.proUnlocked
            && root.section === "market" && root.visible
        onTriggered: marketController.refresh(false)
    }
    Connections {
        target: marketController
        function onChanged() { root.settleMarketRefresh() }
    }

    signal periodRequested(int days)
    signal refreshRequested()
    signal shareRequested(var payload)

    // Snapshot is frozen before the preview opens: later Stats refreshes must
    // not silently change a report already being prepared for publication.
    property var shareSnapshot: ({})
    property double shareCapturedAt: 0
    property string shareCardStyle: "neon"
    property string accountDisplayName: ""
    property string accountUsername: ""
    property string accountFid: ""
    property string accountAvatarUrl: ""
    onAccountFidChanged: {
        if (statsShareModal.opened) statsShareModal.close()
    }
    onProUnlockedChanged: {
        if (!proUnlocked && statsShareModal.opened) statsShareModal.close()
    }
    property string shareAccountName: ""
    property int sharePeriodDays: 30
    property string shareAccountHandle: ""
    property string shareAccountFid: ""
    property string shareAccountAvatarUrl: ""
    property bool shareCaptureBusy: false
    property string shareCaptureError: ""
    property string shareGeneratedCardUrl: ""
    property bool shareCardHandedOff: false

    // An ordinary CPU-raster PNG is generated BEFORE the preview opens.
    // The displayed Image uses that exact file; Qt Quick's asynchronous
    // grabToImage callback is never part of the share/publish critical path.
    function regenerateShareImage() {
        if (root.shareCaptureBusy) return false
        if (mediaUpload.busy) {
            root.shareCaptureError = qsTrId("genycaster.media.upload_unavailable")
            return false
        }
        root.shareCaptureBusy = true
        try {
            const created = mediaUpload.renderStatsCard(
                root.shareSnapshot,
                root.shareAccountName,
                root.shareAccountHandle,
                root.shareAccountAvatarUrl,
                root.sharePeriodDays,
                root.shareCardStyle,
                root.shareCapturedAt)
            const next = String(created.imageFileUrl || "")
            if (!next) {
                root.shareCaptureError = qsTrId("genycaster.stats.card_capture_failed")
                return false
            }
            const previous = root.shareGeneratedCardUrl
            root.shareGeneratedCardUrl = next
            root.shareCaptureError = ""
            if (previous.length > 0 && previous !== next)
                mediaUpload.discardStatsCardImage(previous)
            return true
        } catch (error) {
            root.shareCaptureError = qsTrId("genycaster.stats.card_capture_failed")
            return false
        } finally {
            root.shareCaptureBusy = false
        }
    }

    // The actual portrait arrives asynchronously. Regenerate the same CPU
    // PNG once it is decoded so published media and preview remain identical.
    Connections {
        target: mediaUpload
        function onStatsAvatarChanged() {
            if (statsShareModal.opened && !root.shareCaptureBusy
                    && !mediaUpload.statsAvatarLoading && !root.shareCardHandedOff
                    && root.shareAccountFid === root.accountFid)
                root.regenerateShareImage()
        }
    }

    function selectShareStyle(nextStyle) {
        if (nextStyle === root.shareCardStyle) return
        const previousStyle = root.shareCardStyle
        root.shareCardStyle = nextStyle
        if (!root.regenerateShareImage())
            root.shareCardStyle = previousStyle
    }

    function openStatsShare(name, handle, fid) {
        if (root.section !== "social" || !root.proUnlocked || !root.loaded)
            return
        root.shareSnapshot = Object.assign({}, root.metrics)
        root.shareCapturedAt = Date.now()
        root.sharePeriodDays = root.periodDays
        root.shareAccountName = String(name || "")
        root.shareAccountHandle = String(handle || "")
        root.shareAccountFid = String(fid || "")
        root.shareAccountAvatarUrl = String(root.accountAvatarUrl || "")
        mediaUpload.prepareStatsAvatar(root.shareAccountAvatarUrl)
        root.shareCardStyle = "neon"
        root.shareCaptureError = ""
        root.shareCaptureBusy = false
        root.shareCardHandedOff = false
        // If image generation fails, leave the preview open with an explicit
        // error and a selectable text-only fallback; never hang on Preparing.
        root.regenerateShareImage()
        statsShareModal.open()
    }

    function compactShareText() {
        const metric = value => LocaleFormat.formatInteger(Math.max(0, Number(value || 0)))
        const lines = ["Genycaster Stats · " + root.rangeLabel(root.sharePeriodDays)]
        const stats = root.shareSnapshot
        if (stats.hasEngagement === true)
            lines.push(qsTrId("genycaster.stats.share_engagement_line").arg(metric(stats.engagement)))
        lines.push(qsTrId("genycaster.stats.share_publishing_line")
            .arg(metric(stats.casts)).arg(metric(stats.replies)))
        if (stats.hasEngagement === true)
            lines.push(qsTrId("genycaster.stats.share_reactions_line")
                .arg(metric(stats.likes)).arg(metric(stats.recasts))
                .arg(metric(stats.receivedReplies)))
        lines.push("", "Powered by Genycaster")
        return lines.join("\n")
    }

    function captureStatsCard() {
        if (root.shareCaptureBusy || !statsShareModal.opened
                || !root.proUnlocked || !root.loaded || mediaUpload.busy
                || mediaUpload.statsAvatarLoading
                || root.shareAccountFid !== root.accountFid)
            return
        if (!root.shareGeneratedCardUrl.length || !shareCardPreview.ready) {
            root.shareCaptureError = qsTrId("genycaster.stats.card_capture_failed")
            return
        }
        const fileUrl = root.shareGeneratedCardUrl
        root.shareCardHandedOff = true
        statsShareModal.close()
        root.shareRequested({
            schema: "genycaster.stats/v1",
            text: root.compactShareText(),
            imageFileUrl: fileUrl
        })
    }

    GenyModal {
        id: statsShareModal
        objectName: "statsShareRichCastPreview"
        parent: Overlay.overlay
        onClosed: {
            const unusedImage = root.shareGeneratedCardUrl
            root.shareGeneratedCardUrl = ""
            if (!root.shareCardHandedOff && unusedImage.length > 0)
                mediaUpload.discardStatsCardImage(unusedImage)
            root.shareCardHandedOff = false
            root.shareCaptureBusy = false
        }
        // Neon is a square recap; Minimal is a rounded 2:1 cover. Both
        // previews use the actual aspect of the exported raster.
        padding: Theme.space3
        width: parent ? Math.min(460, parent.width - Theme.space3 * 2) : 460
        height: parent ? Math.min(parent.height - Theme.space3 * 2,
            Math.max(310, (width - Theme.space3 * 2)
                * (root.shareCardStyle === "minimal" ? 0.5 : 1) + 178)) : 600
        contentItem: Flickable {
            id: shareModalScroll
            clip: true
            contentWidth: width
            contentHeight: shareModalBody.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ColumnLayout {
                id: shareModalBody
                width: shareModalScroll.width
                spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stats.card_title")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                ActionButton {
                    iconOnly: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.pro.not_now")
                    onClicked: statsShareModal.close()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    compact: true
                    text: qsTrId("genycaster.stats.card_theme_neon")
                    prominent: root.shareCardStyle === "neon"
                    onClicked: root.selectShareStyle("neon")
                }
                ActionButton {
                    compact: true
                    text: qsTrId("genycaster.stats.card_theme_minimal")
                    prominent: root.shareCardStyle === "minimal"
                    onClicked: root.selectShareStyle("minimal")
                }
                Item { Layout.fillWidth: true }
            }
            Item {
                id: statsSharePreviewViewport
                Layout.fillWidth: true
                Layout.preferredHeight: width * (root.shareCardStyle === "minimal" ? 0.5 : 1)
                clip: false
                StatsShareCard {
                    id: shareCardPreview
                    variantStyle: root.shareCardStyle
                    sourceUrl: root.shareGeneratedCardUrl
                    width: statsSharePreviewViewport.width
                    height: statsSharePreviewViewport.height
                    // The actual source is 1920×1920 (Neon) or 1920×960
                    // (Minimal); no forced cropping or stretched preview.
                    onFailedChanged: {
                        if (failed && root.shareGeneratedCardUrl.length > 0)
                            root.shareCaptureError = qsTrId("genycaster.stats.card_capture_failed")
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: root.shareCaptureError.length > 0
                text: root.shareCaptureError
                color: Theme.danger
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.shareAccountAvatarUrl.length > 0
                    && !mediaUpload.statsAvatarLoading && !mediaUpload.statsAvatarReady
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stats.avatar_unavailable")
                    color: Theme.warning
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.stats.avatar_retry")
                    onClicked: mediaUpload.prepareStatsAvatar(root.shareAccountAvatarUrl)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.stats.card_text_only")
                    enabled: !root.shareCaptureBusy
                    onClicked: {
                        statsShareModal.close()
                        root.shareRequested({
                            schema: "genycaster.stats/v1",
                            text: root.compactShareText(),
                            imageFileUrl: ""
                        })
                    }
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    prominent: true
                    Accessible.description: qsTrId("genycaster.stats.card_compat_note")
                    text: root.shareCaptureBusy || mediaUpload.statsAvatarLoading
                        ? qsTrId("genycaster.stats.card_preparing")
                        : qsTrId("genycaster.stats.card_publish")
                    enabled: !root.shareCaptureBusy && !mediaUpload.statsAvatarLoading
                             && !mediaUpload.busy
                             && shareCardPreview.ready && root.shareGeneratedCardUrl.length > 0
                    onClicked: root.captureStatsCard()
                }
            }
            }
        }
    }
    signal castRequested(string hash)
    signal proRequested()

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space1

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.stats.your_stats")
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
                color: Theme.textPrimary
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.section === "market" ? qsTrId("genycaster.market.subtitle")
                    : qsTrId("genycaster.stats.subtitle_short")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
                wrapMode: Text.Wrap
            }
        }

        ActionButton {
            objectName: "statsShareResultsButton"
            visible: root.section === "social"
            enabled: root.proUnlocked && root.loaded && !root.loading
            Layout.preferredWidth: iconOnly ? 42 : implicitWidth
            Layout.preferredHeight: 42
            Layout.alignment: Qt.AlignVCenter
            compact: true
            quiet: true
            elevated: false
            iconOnly: root.compact || root.width < 750
            iconSize: Theme.iconSizeLarge
            iconGlyph: Theme.iconShare
            text: qsTrId("genycaster.stats.share_results")
            onClicked: root.openStatsShare(root.accountDisplayName,
                                            root.accountUsername,
                                            root.accountFid)
        }

        ActionButton {
            id: refreshButton
            objectName: "statsRefreshButton"
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            Layout.alignment: Qt.AlignVCenter
            compact: true
            quiet: true
            elevated: false
            iconOnly: true
            iconSize: Theme.iconSizeLarge
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.stats.refresh")
            busy: root.section === "market" ? root.marketRequestBusy : root.loading
            spinIconWhileBusy: !Theme.reduceMotion
            enabled: root.proUnlocked && !busy
            onClicked: {
                if (root.section === "market") root.requestMarketRefresh()
                else root.refreshRequested()
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: root.proUnlocked && root.section === "market"
            && root.marketRefreshFeedback.length > 0
        text: root.marketRefreshFeedback
        color: root.manualMarketRefresh ? Theme.accent : Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
        Accessible.role: Accessible.StaticText
        opacity: visible ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection
        GenyTabButton {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            selected: root.section === "social"
            text: qsTrId("genycaster.market.social")
            onClicked: root.section = "social"
        }
        GenyTabButton {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            selected: root.section === "market"
            text: qsTrId("genycaster.market.market")
            onClicked: root.section = "market"
        }
    }

    // A true locked state, not a decorative blur over account data. The
    // server-side subscription remains authoritative; this surface neither
    // fetches premium metrics nor accepts refresh/share while locked.
    NeonSurface {
        id: premiumStatsGate
        interactive: true
        ambientNeon: true
        neonStrength: Theme.neonCardStrength
        objectName: "statsProUpgradeGate"
        visible: !root.proUnlocked
        Layout.fillWidth: true
        implicitHeight: premiumGateBody.implicitHeight + Theme.space4 * 2
        // Unverified is not Basic. Treat an unavailable entitlement service
        // as a neutral status panel, not a paid-upgrade advertisement.
        fillColor: root.proStatusKnown ? Theme.proSurface : Theme.surfaceRaised
        borderColor: root.proStatusKnown ? Theme.proBorder : Theme.borderMuted
        borderWidth: Theme.strokeHairline
        curveDepth: Theme.curveContainer

        ColumnLayout {
            id: premiumGateBody
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.space4
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Icon {
                    text: Theme.iconSpark
                    color: Theme.proAccent
                    preferredSize: Theme.iconSizeLarge
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.proStatusKnown
                    ? qsTrId("genycaster.pro.feature_market_desc")
                    : qsTrId("genycaster.pro.verification_unavailable")
                font.pixelSize: Theme.typeBodySize
                color: Theme.textSecondary
                wrapMode: Text.Wrap
            }
            ActionButton {
                objectName: "statsUnlockProButton"
                Layout.fillWidth: root.compact
                Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                compact: false
                prominent: root.proStatusKnown
                quiet: !root.proStatusKnown
                iconGlyph: root.proStatusKnown ? Theme.iconSpark : Theme.iconRefresh
                text: root.proStatusKnown
                    ? qsTrId("genycaster.pro.get_pro")
                    : qsTrId("genycaster.pro.check_status")
                onClicked: root.proRequested()
            }
        }
    }

    // Keep the context and range actions on the same compact line. The
    // period menu is local to this surface, not a second navigation strip.
    RowLayout {
        objectName: "statsPeriodContextRow"
        visible: root.proUnlocked && root.section === "social"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2
        opacity: root.sectionArrival
        transform: Translate { y: (1 - root.sectionArrival) * 6 }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            alignToUi: true
            text: qsTrId("genycaster.stats.period_note")
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        DirectionAwareLabel {
            text: root.rangeLabel(root.periodDays)
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightDemiBold
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignVCenter
        }

        ActionButton {
            id: periodMenuButton
            objectName: "statsPeriodMenuButton"
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            compact: true
            quiet: true
            elevated: false
            iconOnly: true
            iconGlyph: Theme.iconMore
            text: qsTrId("genycaster.stats.choose_period")
            onClicked: {
                if (periodMenu.visible)
                    periodMenu.close()
                else
                    periodMenu.open()
            }

            Popup {
                id: periodMenu
                objectName: "statsPeriodMenu"
                parent: periodMenuButton
                x: Theme.rtl ? 0 : parent.width - width
                y: parent.height + Theme.space1
                width: Math.min(208, Math.max(152, root.width - Theme.space4))
                // Match other contextual menus: keep selected/hover paint
                // inset from the popover's rounded border on every edge.
                padding: Theme.space2
                implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
                modal: false
                focus: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                background: PopupSurface {
                    fillColor: Theme.surfaceElevated
                    borderColor: Theme.borderDefault
                    curveDepth: Theme.curveContainer
                    elevation: Theme.elevationPopover
                }
                contentItem: Column {
                    spacing: 1
                    Repeater {
                        model: root.rangeDays
                        delegate: ItemDelegate {
                            id: periodChoice
                            required property var modelData
                            width: periodMenu ? periodMenu.width - periodMenu.leftPadding
                                   - periodMenu.rightPadding : 0
                            height: 38
                            hoverEnabled: true
                            onClicked: {
                                periodMenu.close()
                                if (root.periodDays !== modelData)
                                    root.periodRequested(modelData)
                            }
                            contentItem: RowLayout {
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: root.rangeLabel(periodChoice.modelData)
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: root.periodDays === periodChoice.modelData
                                                 ? Theme.weightDemiBold : Font.Normal
                                    color: Theme.textPrimary
                                }
                                Icon {
                                    visible: root.periodDays === periodChoice.modelData
                                    text: Theme.iconCheck
                                    color: Theme.accent
                                    preferredSize: Theme.iconSizeSmall
                                    Layout.preferredWidth: Theme.iconSizeSmall
                                    Layout.preferredHeight: Theme.iconSizeSmall
                                }
                            }
                            background: Superellipse {
                                fillColor: periodChoice.hovered
                                           ? Theme.surfaceHover
                                           : periodChoice.modelData === root.periodDays
                                             ? Theme.primarySurface : "transparent"
                                borderWidth: 0
                                curveDepth: Theme.curveCompact
                            }
                        }
                    }
                }
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: root.proUnlocked && root.section === "social" && root.loading
        text: qsTrId("genycaster.stats.loading").arg(LocaleFormat.formatInteger(statsController.loadedCount))
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
    }

    StatsSkeleton {
        visible: root.proUnlocked && root.section === "social" && root.loading
        Layout.fillWidth: true
    }

    NeonSurface {
        interactive: true
        ambientNeon: true
        neonStrength: Theme.neonPanelStrength
        visible: root.proUnlocked && root.section === "social"
            && (root.fetchState === "error" || root.fetchState === "idle")
        Layout.fillWidth: true
        Layout.preferredHeight: errorLabel.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surface
        borderColor: Theme.borderMuted
        borderWidth: 1
        curveDepth: Theme.curveContainer

        DirectionAwareLabel {
            id: errorLabel
            anchors.fill: parent
            anchors.margins: Theme.space4
            text: root.fetchState === "error"
                  ? qsTrId("genycaster.stats.node_unavailable")
                    .arg(String(root.metrics.error || ""))
                  : qsTrId("genycaster.stats.select_to_load")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeMetaSize
            wrapMode: Text.Wrap
        }
    }

    // Loaded content follows the section entrance progress. Refresh still
    // uses the skeleton and never exposes the previous account's statistics.
    ColumnLayout {
        id: content
        Layout.fillWidth: true
        visible: root.proUnlocked && root.section === "social" && root.loaded
        spacing: Theme.space3
        opacity: root.loaded ? root.sectionArrival : 0
        transform: Translate { y: (1 - root.sectionArrival) * 8 }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.partial
            text: qsTrId("genycaster.stats.partial")
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        GridLayout {
            id: statsKpiGrid
            layoutDirection: Theme.uiDirection
            Layout.fillWidth: true
            columns: root.compact || root.width < 660 ? 2 : 4
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2

            HoverHandler {
                id: statsKpiPointer
                enabled: Theme.neonEnabled && !root.compact
                blocking: false
            }

            StatsKpi {
                id: castsKpi
                proximityPointerActive: statsKpiPointer.hovered
                proximityPointerX: statsKpiPointer.point.position.x - castsKpi.x
                proximityPointerY: statsKpiPointer.point.position.y - castsKpi.y
                title: qsTrId("genycaster.stats.casts")
                iconName: Theme.iconCompose
                amount: Number(root.metrics.casts || 0)
                footnote: qsTrId("genycaster.stats.published_posts")
            }
            StatsKpi {
                id: repliesKpi
                proximityPointerActive: statsKpiPointer.hovered
                proximityPointerX: statsKpiPointer.point.position.x - repliesKpi.x
                proximityPointerY: statsKpiPointer.point.position.y - repliesKpi.y
                title: qsTrId("genycaster.stats.replies")
                iconName: Theme.iconReply
                amount: Number(root.metrics.replies || 0)
                footnote: qsTrId("genycaster.stats.written_replies")
            }
            StatsKpi {
                id: engagementKpi
                proximityPointerActive: statsKpiPointer.hovered
                proximityPointerX: statsKpiPointer.point.position.x - engagementKpi.x
                proximityPointerY: statsKpiPointer.point.position.y - engagementKpi.y
                title: qsTrId("genycaster.stats.engagement")
                iconName: Theme.iconHeart
                amount: Number(root.metrics.engagement || 0)
                available: Boolean(root.metrics.hasEngagement)
                footnote: qsTrId("genycaster.stats.received_on_posts")
            }
            StatsKpi {
                id: followersKpi
                proximityPointerActive: statsKpiPointer.hovered
                proximityPointerX: statsKpiPointer.point.position.x - followersKpi.x
                proximityPointerY: statsKpiPointer.point.position.y - followersKpi.y
                title: qsTrId("genycaster.stats.followers")
                iconName: Theme.iconProfile
                amount: Number(root.metrics.followers || 0)
                available: Boolean(root.metrics.hasAudience)
                footnote: qsTrId("genycaster.stats.current_total")
            }
        }

        StatsBreakdown {
            visible: Boolean(root.metrics.hasEngagement)
            Layout.fillWidth: true
            compact: root.compact
            metrics: root.metrics
            // Same provider-scoped date buckets as the publishing chart.
            // Engagement in a bucket is the current response count on
            // posts authored then, never a fabricated interaction timeline.
            activity: statsController.activity
        }

        StatsChart {
            Layout.fillWidth: true
            compact: root.compact
            bars: statsController.activity
            periodDays: root.periodDays
        }

        StatsTopCasts {
            visible: Boolean(root.metrics.hasEngagement)
            Layout.fillWidth: true
            casts: statsController.topCasts
            onCastRequested: hash => root.castRequested(hash)
        }

        RowLayout {
            layoutDirection: Theme.uiDirection
            Layout.fillWidth: true
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.metrics.hasEngagement
                    ? qsTrId("genycaster.stats.engagement_scope")
                    : qsTrId("genycaster.stats.engagement_unavailable")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
            DirectionAwareLabel {
                Layout.alignment: Qt.AlignBottom
                text: LocaleFormat.localizeDigits(statsController.updatedAt)
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
    MarketPanel {
        visible: root.proUnlocked && root.section === "market"
        Layout.fillWidth: true
        opacity: root.sectionArrival
        transform: Translate { y: (1 - root.sectionArrival) * 8 }
    }
}
