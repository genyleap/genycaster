import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

NeonSurface {
    id: root
    interactive: true
    ambientNeon: true
    neonBusy: root.loading
    neonStrength: Theme.neonCardStrength

    property var rows: []
    property bool busy: false
    property double accountFid: 0
    property bool accountOnly: false
    property bool stale: false
    property bool offline: false
    // Metrics are computed only from the currently materialized rows supplied by
    // Presentation. They never imply network-wide/community-wide totals.
    property string scopeMode: "feed"

    readonly property var sampleRows: root.filteredRows()
    readonly property bool loading: root.busy && root.sampleRows.length === 0
    readonly property int castCount: root.sampleRows.length
    readonly property int activeAuthors: root.uniqueAuthorCount()
    // A raw protocol CastAdd does not include aggregate reactions. Missing
    // counts must not appear as verified zero engagement in profile analytics.
    readonly property bool engagementAvailable: typeof providerProfiles === "undefined"
                                                || providerProfiles === null
                                                || providerProfiles.activeReadContract !== "snapchain-v1"
    readonly property int engagementTotal: root.totalEngagement()
    readonly property double averageEngagement: root.castCount > 0 ? root.engagementTotal / root.castCount : 0
    readonly property var activityBuckets: root.buildActivityBuckets()
    readonly property int bucketMaximum: root.maxBucket()
    readonly property double newestTimestamp: root.latestTimestamp()

    function filteredRows() {
        const result = []
        const source = root.rows || []
        for (let index = 0; index < source.length; ++index) {
            const row = source[index] || ({})
            if (root.accountOnly && root.accountFid > 0 && Number(row.authorFid || 0) !== Number(root.accountFid))
                continue
            result.push(row)
        }
        return result
    }

    function uniqueAuthorCount() {
        const seen = ({})
        for (let index = 0; index < root.sampleRows.length; ++index) {
            const row = root.sampleRows[index] || ({})
            const fid = Number(row.authorFid || 0)
            const author = String(row.author || row.username || "").toLowerCase()
            const key = fid > 0 ? "fid:" + fid : "u:" + author
            if (key !== "u:") seen[key] = true
        }
        return Object.keys(seen).length
    }

    function totalEngagement() {
        let total = 0
        for (let index = 0; index < root.sampleRows.length; ++index) {
            const row = root.sampleRows[index] || ({})
            total += Number(row.replyCount || 0) + Number(row.recastCount || 0) + Number(row.likeCount || 0)
        }
        return total
    }

    function latestTimestamp() {
        let latest = 0
        for (let index = 0; index < root.sampleRows.length; ++index)
            latest = Math.max(latest, Number((root.sampleRows[index] || {}).createdAtMs || 0))
        return latest
    }

    function buildActivityBuckets() {
        const result = [0, 0, 0, 0, 0, 0, 0]
        const now = new Date()
        const today = new Date(now.getFullYear(), now.getMonth(), now.getDate())
        const start = today.getTime() - 6 * 86400000
        for (let index = 0; index < root.sampleRows.length; ++index) {
            const timestamp = Number((root.sampleRows[index] || {}).createdAtMs || 0)
            if (timestamp < start || timestamp >= today.getTime() + 86400000)
                continue
            const bucket = Math.floor((timestamp - start) / 86400000)
            if (bucket >= 0 && bucket < 7) result[bucket] += 1
        }
        return result
    }

    function maxBucket() {
        let value = 0
        for (let index = 0; index < root.activityBuckets.length; ++index)
            value = Math.max(value, Number(root.activityBuckets[index] || 0))
        return value
    }

    function freshnessText() {
        const scope = root.scopeMode === "profile" ? qsTrId("genycaster.feed_pulse.loaded_profile_posts") : qsTrId("genycaster.feed_pulse.loaded_feed_snapshot")
        if (root.offline) return scope + " · " + qsTrId("genycaster.feed_pulse.offline")
        if (root.stale) return scope + " · " + qsTrId("genycaster.feed_pulse.cached")
        if (root.newestTimestamp <= 0) return scope + " · " + qsTrId("genycaster.feed_pulse.waiting_for_data")
        return scope
    }

    component PulseMetric: ColumnLayout {
        property string valueText: "—"
        property string labelText: ""
        Layout.fillWidth: true
        spacing: 0
        Label {
            Layout.fillWidth: true
            LayoutMirroring.enabled: false
            text: parent.valueText
            color: Theme.textPrimary
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightBold
            horizontalAlignment: Theme.rtl ? Text.AlignRight : Text.AlignLeft
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: parent.labelText
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }

    fillColor: Theme.contextRailCard
    borderWidth: Theme.strokeHairline
    borderColor: root.pointerActive ? Theme.borderDefault : Theme.borderMuted
    curveDepth: Theme.curveModal
    implicitHeight: 214

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            // Match ContextRail's trailing reorder handle on both sides.
            Layout.leftMargin: Theme.rtl ? 34 : 0
            Layout.rightMargin: Theme.rtl ? 0 : 34
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                fillColor: Theme.primarySurface
                curveDepth: 17
                exponent: 2.01
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconChart
                    color: Theme.accent
                    preferredSize: Theme.iconSize
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.scopeMode === "profile" ? qsTrId("genycaster.feed_pulse.profile_snapshot") : qsTrId("genycaster.feed_pulse.title")
                    alignToUi: true
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.freshnessText()
                    alignToUi: true
                    color: root.offline || root.stale ? Theme.warning : Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
            }

            GenySpinner {
                visible: root.loading
                running: visible
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            PulseMetric {
                valueText: root.loading ? "—"
                           : root.scopeMode === "profile"
                             ? LocaleFormat.formatCompactNumber(root.castCount)
                             : LocaleFormat.formatCompactNumber(root.activeAuthors)
                labelText: root.scopeMode === "profile" ? qsTrId("genycaster.feed_pulse.posts_loaded") : qsTrId("genycaster.feed_pulse.authors_loaded")
            }
            PulseMetric {
                valueText: root.loading || (root.scopeMode === "profile" && !root.engagementAvailable) ? "—"
                           : root.scopeMode === "profile"
                             ? LocaleFormat.formatCompactNumber(root.engagementTotal)
                             : LocaleFormat.formatCompactNumber(root.castCount)
                labelText: root.scopeMode === "profile" ? qsTrId("genycaster.feed_pulse.interactions") : qsTrId("genycaster.context.community_casts_loaded")
            }
            PulseMetric {
                valueText: root.loading || (root.scopeMode === "profile" && !root.engagementAvailable) ? "—"
                           : root.scopeMode === "profile"
                             ? LocaleFormat.formatNumber(root.averageEngagement, 1)
                             : LocaleFormat.formatCompactNumber(root.engagementTotal)
                labelText: root.scopeMode === "profile" ? qsTrId("genycaster.feed_pulse.avg_per_post") : qsTrId("genycaster.feed_pulse.interactions")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            // Weekly buckets follow the UI reading direction just like their
            // start/end labels; the latest bucket mirrors in Persian.
            layoutDirection: Theme.uiDirection
            spacing: Theme.space1

            Repeater {
                model: root.activityBuckets
                delegate: Item {
                    required property int modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Superellipse {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: Math.max(5, parent.height * (root.bucketMaximum > 0 ? Number(modelData) / root.bucketMaximum : 0.08))
                        fillColor: modelData > 0 ? Theme.accent : Theme.heatmapEmpty
                        opacity: modelData > 0 ? 0.34 + 0.66 * (Number(modelData) / Math.max(1, root.bucketMaximum)) : 1
                        curveDepth: Math.min(6, height / 2)
                        exponent: 2.6
                        Behavior on height { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.context.community_last_7_days")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
            DirectionAwareLabel {
                visible: root.newestTimestamp > 0
                text: root.newestTimestamp > 0 ? LocaleFormat.formatRelativeTime(root.newestTimestamp) : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
}
