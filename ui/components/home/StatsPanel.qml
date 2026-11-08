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
    onSectionChanged: {
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

    Superellipse {
        visible: root.section === "social"
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        fillColor: Theme.surfaceMuted
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveControl

        RowLayout {
            anchors.fill: parent
            anchors.margins: 3
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [7, 30, 90, 365]
                delegate: GenyTabButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    minimumWidth: 0
                    dense: true
                    selected: root.periodDays === modelData
                    text: modelData === 7 ? qsTrId("genycaster.stats.range_week")
                        : modelData === 30 ? qsTrId("genycaster.stats.range_month")
                        : modelData === 90 ? qsTrId("genycaster.stats.range_quarter")
                                           : qsTrId("genycaster.stats.range_year")
                    Accessible.name: qsTrId("genycaster.stats.last_n_days").arg(LocaleFormat.formatInteger(modelData))
                    onClicked: if (root.periodDays !== modelData)
                                   root.periodRequested(modelData)
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

    // Loaded components fade in once per response. Refresh falls back to a
    // lightweight skeleton; no old FID's statistics can leak into a new FID.
    ColumnLayout {
        id: content
        Layout.fillWidth: true
        visible: root.section === "social" && root.loaded
        spacing: Theme.space3
        opacity: root.loaded ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.partial
            text: qsTrId("genycaster.stats.partial")
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.stats.period_note")
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
    }
}
