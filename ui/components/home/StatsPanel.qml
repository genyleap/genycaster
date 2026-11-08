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
        if (section === "market" && tabActive) marketController.refresh(false)
    }
    onTabActiveChanged: {
        if (tabActive && section === "market") marketController.refresh(false)
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
            busy: root.section === "market" ? marketController.state === "loading" : root.loading
            spinIconWhileBusy: !Theme.reduceMotion
            enabled: !busy
            onClicked: {
                if (root.section === "market") marketController.refresh(true)
                else root.refreshRequested()
            }
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
                model: [
                    {days: 7, title: "7D"},
                    {days: 30, title: "30D"},
                    {days: 90, title: "90D"},
                    {days: 365, title: "1Y"}
                ]
                delegate: GenyTabButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    minimumWidth: 0
                    dense: true
                    selected: root.periodDays === modelData.days
                    text: modelData.title
                    Accessible.name: qsTrId("genycaster.stats.last_n_days").arg(modelData.days)
                    onClicked: if (root.periodDays !== modelData.days)
                                   root.periodRequested(modelData.days)
                }
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: root.section === "social" && root.loading
        text: qsTrId("genycaster.stats.loading").arg(statsController.loadedCount)
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
                text: statsController.updatedAt
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
