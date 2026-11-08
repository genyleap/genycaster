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
            if (section === "market") marketController.refresh(false)
            else refreshGithub()
        }
    }
    onTabActiveChanged: {
        if (tabActive) {
            if (section === "market") marketController.refresh(false)
            else refreshGithub()
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

    // Only the FID-bound OpenProof connection list can establish GitHub ownership.
    // Bio links and manually entered profile fields are not identity proof.
    readonly property string verifiedGithubLogin: {
        if (typeof openProofConnections === "undefined" || !openProofConnections
                || !openProofConnections.cloudAuthorizationReady)
            return ""
        const entries = openProofConnections.connections || []
        for (let i = 0; i < entries.length && i < 50; ++i) {
            const entry = entries[i] || ({})
            if (String(entry.provider || "").toLowerCase() !== "github")
                continue
            const names = [entry.username, entry.preferred_username, entry.login, entry.handle]
            const profile = String(entry.profileUrl || entry.provider_profile_url || "")
            const match = /^https:\/\/github\.com\/([A-Za-z0-9-]{1,39})\/?$/.exec(profile)
            if (match) names.push(match[1])
            for (const name of names) {
                const login = String(name || "").replace(/^@/, "").trim()
                if (/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(login))
                    return login
            }
        }
        return ""
    }
    readonly property int githubYear: new Date().getUTCFullYear()
    readonly property var githubCalendar: genyPulseActivity.githubContributions || ({})
    readonly property bool githubSnapshotMatches: Number(githubCalendar.year) === githubYear
        && verifiedGithubLogin.length > 0
        && String(githubCalendar.username || "").toLowerCase() === verifiedGithubLogin.toLowerCase()
    readonly property string githubStatus: githubSnapshotMatches
        ? String(githubCalendar.status || "idle") : "idle"
    readonly property bool githubReady: githubStatus === "ok"
    readonly property int githubWeekCount: width >= 760 ? 18 : width >= 460 ? 12 : 7
    property int hoveredGithubCell: -1
    readonly property var githubDayMap: {
        const map = ({})
        if (!githubReady) return map
        const days = githubCalendar.days || []
        for (let i = 0; i < days.length && i < 366; ++i) {
            const day = days[i] || ({})
            const date = String(day.date || "")
            if (/^\d{4}-\d{2}-\d{2}$/.test(date))
                map[date] = day
        }
        return map
    }
    function githubDateAt(index) {
        const now = new Date()
        const today = Date.UTC(now.getUTCFullYear(), now.getUTCMonth(),
                               now.getUTCDate()) / 86400000
        return new Date((today - githubWeekCount * 7 + index + 1) * 86400000)
            .toISOString().slice(0, 10)
    }
    function githubColor(level) {
        const value = Math.max(0, Math.min(4, Number(level || 0)))
        if (value === 0) return Theme.surfaceMuted
        // Palette derives from the user's current accent and light/dark
        // scheme; levels come from the verified GitHub daily calendar.
        return Qt.lighter(Theme.accent, Theme.dark
            ? [1, 1.95, 1.55, 1.25, 1][value]
            : [1, 2.25, 1.7, 1.28, 1][value])
    }

    function refreshGithub(force) {
        if (!tabActive || section !== "social" || verifiedGithubLogin.length === 0)
            return
        if (force)
            genyPulseActivity.refreshGitHubContributions(verifiedGithubLogin, githubYear)
        else
            genyPulseActivity.requestGitHubContributions(verifiedGithubLogin, githubYear)
    }
    onVerifiedGithubLoginChanged: refreshGithub()
    Component.onCompleted: refreshGithub()

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
        running: root.tabActive && root.section === "market" && root.visible
        onTriggered: marketController.refresh(false)
    }
    Connections {
        target: marketController
        function onChanged() { root.settleMarketRefresh() }
    }

    signal periodRequested(int days)
    signal refreshRequested()
    signal shareRequested(string text)
    signal castRequested(string hash)

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
            enabled: root.loaded && !root.loading
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
            onClicked: {
                const draft = root.buildShareText()
                if (draft.length > 0)
                    root.shareRequested(draft)
            }
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
            enabled: !busy
            onClicked: {
                if (root.section === "market") root.requestMarketRefresh()
                else {
                    root.refreshGithub(true)
                    root.refreshRequested()
                }
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: root.section === "market" && root.marketRefreshFeedback.length > 0
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

    // Keep the context and range actions on the same compact line. The
    // period menu is local to this surface, not a second navigation strip.
    RowLayout {
        objectName: "statsPeriodContextRow"
        visible: root.section === "social"
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
                            width: periodMenu.width - periodMenu.leftPadding
                                   - periodMenu.rightPadding
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
        visible: root.section === "social" && root.loading
        text: qsTrId("genycaster.stats.loading").arg(LocaleFormat.formatInteger(statsController.loadedCount))
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
    }

    StatsSkeleton {
        visible: root.section === "social" && root.loading
        Layout.fillWidth: true
    }

    Superellipse {
        visible: root.section === "social" && (root.fetchState === "error" || root.fetchState === "idle")
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
        visible: root.section === "social" && root.loaded
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
            Layout.fillWidth: true
            columns: root.compact || root.width < 660 ? 2 : 4
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2

            StatsKpi {
                title: qsTrId("genycaster.stats.casts")
                iconName: Theme.iconCompose
                amount: Number(root.metrics.casts || 0)
                footnote: qsTrId("genycaster.stats.published_posts")
            }
            StatsKpi {
                title: qsTrId("genycaster.stats.replies")
                iconName: Theme.iconReply
                amount: Number(root.metrics.replies || 0)
                footnote: qsTrId("genycaster.stats.written_replies")
            }
            StatsKpi {
                title: qsTrId("genycaster.stats.engagement")
                iconName: Theme.iconHeart
                amount: Number(root.metrics.engagement || 0)
                available: Boolean(root.metrics.hasEngagement)
                footnote: qsTrId("genycaster.stats.received_on_posts")
            }
            StatsKpi {
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

        // Optional, verified account-derived insight. An unrelated public profile
        // or a failed service response must never be displayed as this user's data.
        Superellipse {
            visible: root.verifiedGithubLogin.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: githubContent.implicitHeight + Theme.space4 * 2
            fillColor: Theme.surface
            borderColor: Theme.borderMuted
            borderWidth: 1
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: githubContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space4
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Image {
                        source: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/github.svg"
                        Layout.preferredWidth: Theme.iconSizeLarge
                        Layout.preferredHeight: Theme.iconSizeLarge
                        sourceSize.width: 48
                        sourceSize.height: 48
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: "GitHub"
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                    DirectionAwareLabel {
                        forceLeftToRight: true
                        text: "@" + root.verifiedGithubLogin
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideMiddle
                    }
                    ActionButton {
                        visible: !root.githubReady && root.githubStatus !== "loading"
                        compact: true
                        quiet: true
                        elevated: false
                        iconOnly: true
                        iconGlyph: Theme.iconRefresh
                        text: qsTrId("genycaster.stats.refresh")
                        onClicked: root.refreshGithub(true)
                    }
                }

                // On a wide statistics workspace, the year summary and
                // recent verified calendar occupy one balanced row. Compact
                // layouts stack them without shrinking the individual cells.
                GridLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    columns: root.width >= 740 ? 2 : 1
                    columnSpacing: Theme.space6
                    rowSpacing: Theme.space3

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 1
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            alignToUi: true
                            text: root.githubReady
                                ? qsTrId("genycaster.activity.contributions_count").arg(
                                    LocaleFormat.formatInteger(Number(root.githubCalendar.count || 0)))
                                : root.githubStatus === "idle" || root.githubStatus === "loading"
                                  ? qsTrId("genycaster.activity.loading")
                                  : root.githubStatus === "authorization-required"
                                    || root.githubStatus === "identity-unconfirmed"
                                    ? qsTrId("genycaster.activity.github_session_required")
                                  : root.githubStatus === "rate-limited"
                                    ? qsTrId("genycaster.activity.rate_limited")
                                    : qsTrId("genycaster.activity.fetch_error")
                            font.pixelSize: root.githubReady ? Theme.typeTitle2Size : Theme.typeCaptionSize
                            font.weight: root.githubReady ? Theme.weightBold : Font.Normal
                            color: root.githubReady ? Theme.textPrimary : Theme.textSecondary
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: root.githubReady
                            alignToUi: true
                            text: qsTrId("genycaster.stats.github_active_days").arg(
                                LocaleFormat.formatInteger(Number(root.githubCalendar.observedDays || 0)))
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textSecondary
                        }
                    }

                    // Verified server-side calendar, not inferred from public
                    // events or a guessed contribution count. This adds useful
                    // temporal context without inventing a comparison percentage.
                    ColumnLayout {
                        visible: root.githubReady && (root.githubCalendar.days || []).length > 0
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Grid {
                            id: githubHeatmap
                            Layout.alignment: Qt.AlignHCenter
                            rows: 7
                            columns: root.githubWeekCount
                            flow: Grid.TopToBottom
                            columnSpacing: 3
                            rowSpacing: 3
                            Repeater {
                                model: root.githubWeekCount * 7
                                delegate: Rectangle {
                                    required property int index
                                    readonly property string dayDate: root.githubDateAt(index)
                                    readonly property var dayData: root.githubDayMap[dayDate] || ({})
                                    width: 11
                                    height: 11
                                    radius: 3
                                    color: root.githubColor(dayData.level)
                                    antialiasing: true
                                    Accessible.role: Accessible.StaticText
                                    Accessible.name: LocaleFormat.localizeDigits(dayDate) + ": "
                                        + LocaleFormat.formatInteger(Number(dayData.count || 0))
                                    HoverHandler {
                                        onHoveredChanged: {
                                            if (hovered) root.hoveredGithubCell = index
                                            else if (root.hoveredGithubCell === index)
                                                root.hoveredGithubCell = -1
                                        }
                                    }
                                }
                            }
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            alignToUi: true
                            text: root.hoveredGithubCell >= 0
                                  ? LocaleFormat.localizeDigits(
                                        root.githubDateAt(root.hoveredGithubCell)) + " · "
                                      + LocaleFormat.formatInteger(Number(
                                          (root.githubDayMap[root.githubDateAt(
                                              root.hoveredGithubCell)] || {}).count || 0))
                                  : qsTrId("genycaster.stats.github_recent_weeks").arg(
                                        LocaleFormat.formatInteger(root.githubWeekCount))
                            color: Theme.textTertiary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

            }
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
        visible: root.section === "market"
        Layout.fillWidth: true
        opacity: root.sectionArrival
        transform: Translate { y: (1 - root.sectionArrival) * 8 }
    }
}
