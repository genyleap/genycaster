import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Owner-scoped verified streaks. The parent Drawer owns the only screen title;
// this component owns just the summary, calendar and milestone progression.
Item {
    id: root
    objectName: "profileStreakCard"

    signal profileRequested(double fid)
    signal signerAuthorizationRequested()

    property var snapshot: ({})
    property bool loading: false
    property string errorText: ""
    property bool compact: false
    property string activeTrack: "activity"
    property bool showAllBadges: false
    // Three peer pages: two private streak views and a standalone public leaderboard.
    property string activePage: "activity"
    property string leaderboardTrack: "activity"
    signal pageChanged()

    property int inspectedMilestone: 0
    readonly property bool flameMotion: root.activePage !== "leaderboard"
        && root.current > 0 && root.visible
        && !Theme.reduceMotion && !Theme.powerSave
    readonly property var streakPreferences: openProofConnections
        ? (openProofConnections.streakPreferences || ({})) : ({})
    readonly property bool leaderboardVisible: streakPreferences.leaderboard_visible === true
    readonly property var leaderboardEntries: openProofConnections
        ? (openProofConnections.streakLeaderboard || []) : []
    readonly property bool leaderboardBusy: openProofConnections
        ? Boolean(openProofConnections.streakLeaderboardBusy) : false
    readonly property bool leaderboardCanJoin: openProofConnections
        ? Boolean(openProofConnections.cloudAuthorizationReady) : false
    // A missing or unapproved HyperSnap signer needs its own custody consent.
    // Repeating a cloud cookie request cannot register a network signer.
    readonly property bool signerAuthorizationNeeded: presentationBridge
        && presentationBridge.isAuthenticated
        && presentationBridge.nativeCustodyAccount
        && !presentationBridge.writeReady
        && !presentationBridge.signerBusy
        && (presentationBridge.signerPhase === "missing"
            || presentationBridge.signerPhase === "authorization-required"
            || presentationBridge.signerPhase === "expired"
            || presentationBridge.signerPhase === "revoked")

    readonly property var track: activeTrack === "creator"
        ? (snapshot.creator || ({})) : (snapshot.activity || ({}))
    readonly property bool available: snapshot.time_zone === "UTC"
    readonly property var badges: track.badges || []
    // Legacy snapshots remain readable. New levels come from the server,
    // not a second client-owned milestone catalogue.
    readonly property var milestoneItems: {
        const catalog=track.milestone_definitions || []
        if(Array.isArray(catalog) && catalog.length>0)
            return catalog.filter(b=>Number(b.threshold_days)>0)
                .slice(0, 48)
        return [1,3,7,14,30,100,365].map(days=>({
            id: "streak-"+days, threshold_days: days,
            unlocked: root.badges.indexOf(days)>=0,
            progress_days: Math.min(root.record, days),
            icon: "flame", reward_kind: "none"
        }))
    }
    readonly property var milestones: milestoneItems.map(b=>Number(b.threshold_days))
    // Translations are static IDs (also for cached older snapshots that lack
    // name_id). Unknown future milestones degrade gracefully to their duration.
    function milestoneName(badge) {
        const threshold = Number(badge?.threshold_days || 0)
        const nameId = String(badge?.name_id || "")
        switch (nameId || "genycaster.streak.badge_" + ({
            1:"first_spark",3:"ignition",7:"momentum",14:"on_fire",
            30:"blaze",60:"wildfire",90:"inferno",100:"firestorm",
            180:"phoenix",365:"eternal_flame"
        }[threshold] || "")) {
        case "genycaster.streak.badge_first_spark": return qsTrId("genycaster.streak.badge_first_spark")
        case "genycaster.streak.badge_ignition": return qsTrId("genycaster.streak.badge_ignition")
        case "genycaster.streak.badge_momentum": return qsTrId("genycaster.streak.badge_momentum")
        case "genycaster.streak.badge_on_fire": return qsTrId("genycaster.streak.badge_on_fire")
        case "genycaster.streak.badge_blaze": return qsTrId("genycaster.streak.badge_blaze")
        case "genycaster.streak.badge_wildfire": return qsTrId("genycaster.streak.badge_wildfire")
        case "genycaster.streak.badge_inferno": return qsTrId("genycaster.streak.badge_inferno")
        case "genycaster.streak.badge_firestorm": return qsTrId("genycaster.streak.badge_firestorm")
        case "genycaster.streak.badge_phoenix": return qsTrId("genycaster.streak.badge_phoenix")
        case "genycaster.streak.badge_eternal_flame": return qsTrId("genycaster.streak.badge_eternal_flame")
        default: return milestoneDuration(threshold)
        }
    }
    function milestoneDuration(days) {
        return (Number(days) === 1
            ? qsTrId("genycaster.streak.badge_one_day")
            : qsTrId("genycaster.streak.badge_many_days"))
            .arg(LocaleFormat.formatInteger(Number(days || 0)))
    }
    readonly property int current: Number(track.current || 0)
    readonly property int record: Number(track.longest || 0)
    readonly property int nextMilestone: Number(track.next_milestone || 0)
    readonly property bool saturdayWeekAvailable: Theme.rtl
        && Array.isArray(track.week_saturday) && track.week_saturday.length === 7
    readonly property var displayWeek: saturdayWeekAvailable
        ? track.week_saturday : (track.week || [])
    readonly property var weekdayLabels: saturdayWeekAvailable
        ? [qsTrId("genycaster.streak.sat"), qsTrId("genycaster.streak.sun"),
           qsTrId("genycaster.streak.mon"), qsTrId("genycaster.streak.tue"),
           qsTrId("genycaster.streak.wed"), qsTrId("genycaster.streak.thu"),
           qsTrId("genycaster.streak.fri")]
        : [qsTrId("genycaster.streak.mon"), qsTrId("genycaster.streak.tue"),
           qsTrId("genycaster.streak.wed"), qsTrId("genycaster.streak.thu"),
           qsTrId("genycaster.streak.fri"), qsTrId("genycaster.streak.sat"),
           qsTrId("genycaster.streak.sun")]

    implicitHeight: body.implicitHeight

    Component {
        id: milestoneBadgeTile

        Rectangle {
            id: badgeTile
            required property var modelData
            readonly property int threshold: Number(modelData.threshold_days || 0)
            readonly property bool earned: modelData.unlocked === true
            readonly property real completed: Math.max(0, Math.min(1,
                Number(modelData.progress_days || 0) / Math.max(1, threshold)))
            readonly property int tileHeight: Theme.scaleUi(root.compact ? 148 : 156)
            implicitWidth: Theme.scaleUi(root.compact ? 118 : 126)
            implicitHeight: tileHeight
            width: implicitWidth
            height: tileHeight
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: tileHeight
            radius: Theme.curveControl
            color: root.inspectedMilestone === threshold || badgeTile.earned
                ? Theme.streakSurface : Theme.surfaceMuted
            border.color: root.inspectedMilestone === threshold
                ? Theme.streak : (badgeTile.earned ? Theme.streakTrack : Theme.borderMuted)
            border.width: root.inspectedMilestone === threshold ? 1.5
                : (badgeTile.earned ? 1 : 0)

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                anchors.topMargin: Theme.space3
                anchors.bottomMargin: Theme.space3
                spacing: Theme.space1

                Item {
                    id: badgeMedallion
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Theme.scaleUi(48)
                    Layout.preferredHeight: Theme.scaleUi(48)

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: badgeTile.earned
                            ? Theme.streakSurface : Theme.surfaceElevated
                        border.color: badgeTile.earned
                            ? Theme.streak : Theme.borderMuted
                        border.width: badgeTile.earned ? 1.5 : 1
                    }
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconStreakFlame
                        preferredSize: Theme.scaleUi(23)
                        color: badgeTile.earned ? Theme.streak : Theme.inkMuted
                        opacity: badgeTile.earned ? 1 : 0.5
                    }
                    Rectangle {
                        visible: !badgeTile.earned
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: Theme.scaleUi(22)
                        height: width
                        radius: width / 2
                        color: Theme.surfaceMuted
                        border.color: Theme.borderMuted
                        border.width: 1
                        Icon {
                            anchors.centerIn: parent
                            text: "system/lock"
                            preferredSize: Theme.scaleUi(12)
                            color: Theme.inkMuted
                        }
                    }
                }
                Label {
                    objectName: "streakAchievementName"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.scaleUi(34)
                    text: root.milestoneName(badgeTile.modelData)
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    color: badgeTile.earned ? Theme.streak : Theme.ink
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: root.milestoneDuration(badgeTile.threshold)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                }
                Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.scaleUi(4)
                    radius: height / 2
                    color: Theme.streakTrack
                    Rectangle {
                        x: Theme.rtl ? parent.width - width : 0
                        width: parent.width * badgeTile.completed
                        height: parent.height
                        radius: parent.radius
                        color: Theme.streak
                    }
                }
            }
            Accessible.role: Accessible.Button
            Accessible.name: root.milestoneName(badgeTile.modelData) + " · "
                + root.milestoneDuration(badgeTile.threshold)
            MouseArea {
                anchors.fill: parent
                onClicked: root.inspectedMilestone =
                    root.inspectedMilestone===badgeTile.threshold ? 0 : badgeTile.threshold
            }
        }
    }

    ColumnLayout {
        id: body
        width: parent.width
        spacing: Theme.space3
        LayoutMirroring.enabled: false

        GenyTabStrip {
            Layout.fillWidth: true
            currentId: root.activePage
            distribute: true
            segmented: true
            minimumTabWidth: 70
            tabs: [
                {id: "activity", label: qsTrId("genycaster.streak.activity"),
                    icon: Theme.iconStreakFlame},
                {id: "creator", label: qsTrId("genycaster.streak.creator"),
                    icon: Theme.iconCompose},
                {id: "leaderboard", label: qsTrId("genycaster.streak.leaderboard"),
                    icon: Theme.iconChart}
            ]
            onActivated: id => {
                root.activePage = id
                if(id === "leaderboard")
                    if (openProofConnections) openProofConnections.fetchStreakLeaderboard(root.leaderboardTrack)
                else
                    root.activeTrack = id
                root.pageChanged()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.activePage !== "leaderboard" && !root.available
            text: root.loading ? qsTrId("genycaster.streak.loading")
                : root.errorText.length > 0 ? root.errorText
                : qsTrId("genycaster.streak.unavailable")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Theme.textStartAlignment
        }

        // Explain why a retry failed instead of leaving the same inert
        // prompt on screen after the session API rejects a signed proof.
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.activePage !== "leaderboard" && !root.available
                && !root.signerAuthorizationNeeded && openProofConnections
                && String(openProofConnections.accountProofError || "").length > 0
            text: qsTrId("genycaster.pro.secure_session_unavailable")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
        }

        // A session failure must have an explicit recovery action. The
        // server independently validates the same FID's registered signer;
        // this button never fabricates activity or an authenticated account.
        ActionButton {
            objectName: "streakRestoreVerifiedSession"
            visible: root.activePage !== "leaderboard" && !root.available
                && openProofConnections && !openProofConnections.cloudAuthorizationReady
            enabled: openProofConnections && !openProofConnections.cloudSessionBusy
                && !openProofConnections.accountProofBusy
            busy: openProofConnections && (openProofConnections.cloudSessionBusy
                || openProofConnections.accountProofBusy)
            text: root.signerAuthorizationNeeded
                ? qsTrId("genycaster.shell.enable_interactions")
                : qsTrId("genycaster.pro.restore_secure_session")
            compact: true
            quiet: true
            onClicked: {
                if (root.signerAuthorizationNeeded)
                    root.signerAuthorizationRequested()
                else
                    openProofConnections.retryAccountSession()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.activePage !== "leaderboard" && root.available
            spacing: Theme.space3

            // Reference-inspired centered achievement hero; one focal point
            // rather than competing statistics across the sheet.
            ColumnLayout {
                objectName: "streakCenteredHero"
                Layout.fillWidth: true
                spacing: Theme.space1

                // Restrained achievement orb: two independent
                // soft pulses, a stable core and a flame. No particles, GPU
                // loops or attention-grabbing flashes; reduced-motion safe.
                Item {
                    id: streakFlameOrb
                    objectName: "streakFlameOrb"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Theme.scaleUi(root.compact ? 106 : 120)
                    Layout.preferredHeight: Layout.preferredWidth

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.94
                        height: width
                        radius: width / 2
                        color: Theme.streakGlow
                        opacity: root.current > 0 ? (Theme.dark ? 0.11 : 0.13) : 0.025
                        scale: 1
                        SequentialAnimation on scale {
                            running: root.flameMotion
                            loops: Animation.Infinite
                            NumberAnimation { from: 0.98; to: 1.07; duration: 1900; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 1.07; to: 0.98; duration: 1900; easing.type: Easing.InOutSine }
                        }
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.79
                        height: width
                        radius: width / 2
                        color: Theme.streakGlow
                        opacity: root.current > 0 ? (Theme.dark ? 0.13 : 0.16) : 0.025
                        scale: 1
                        SequentialAnimation on scale {
                            running: root.flameMotion
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.06; to: 0.96; duration: 1420; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.96; to: 1.06; duration: 1420; easing.type: Easing.InOutSine }
                        }
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.69
                        height: width
                        radius: width / 2
                        color: Theme.streakSurface
                        border.color: Theme.streak
                        border.width: 1
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconStreakFlame
                            preferredSize: Theme.scaleUi(root.compact ? 34 : 39)
                            color: Theme.streak
                        }
                    }
                }
                Label {
                    objectName: "streakCurrentCount"
                    Layout.fillWidth: true
                    text: LocaleFormat.formatInteger(root.current)
                    color: Theme.streak
                    font.pixelSize: Theme.scaleUi(root.compact ? 72 : 78)
                    font.weight: Theme.weightBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: root.current === 1
                        ? qsTrId("genycaster.streak.day_in_row")
                        : qsTrId("genycaster.streak.days_in_row")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.streak.best")
                        .arg(LocaleFormat.formatInteger(root.record))
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.nextMilestone
                    ? qsTrId("genycaster.streak.next")
                        .arg(LocaleFormat.formatInteger(root.nextMilestone))
                    : qsTrId("genycaster.streak.legendary")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Theme.textStartAlignment
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.nextMilestone > 0
                height: Theme.scaleUi(4)
                radius: height / 2
                color: Theme.streakTrack
                Rectangle {
                    // Advance the milestone bar from the reading start.
                    x: Theme.rtl ? parent.width - width : 0
                    height: parent.height
                    width: parent.width * Math.min(1,
                        Math.max(0, root.current / Math.max(1, root.nextMilestone)))
                    radius: parent.radius
                    color: Theme.streak
                }
            }

            RowLayout {
                objectName: "streakWeeklyCalendar"
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1

                Repeater {
                    model: root.displayWeek
                    delegate: ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.weekdayLabels[index]
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: Theme.scaleUi(root.compact ? 32 : 35)
                            height: width
                            radius: width / 2
                            color: modelData.completed
                                ? Theme.streakSurface : Theme.surfaceMuted
                            border.color: modelData.completed
                                ? Theme.streak : Theme.borderMuted
                            border.width: modelData.completed ? 2 : 1
                            Icon {
                                anchors.centerIn: parent
                                text: modelData.completed ? Theme.iconCheck : ""
                                preferredSize: 15
                                color: Theme.streak
                            }
                            Rectangle {
                                visible: !modelData.completed
                                anchors.centerIn: parent
                                width: Theme.scaleUi(8)
                                height: width
                                radius: width / 2
                                color: modelData.future ? Theme.inkSubtle
                                    : Theme.inkMuted
                                opacity: modelData.future ? 0.24 : 0.4
                            }
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.activeTrack === "creator"
                    ? qsTrId("genycaster.streak.creator_rule")
                    : qsTrId("genycaster.streak.activity_rule")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Theme.textStartAlignment
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.divider
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.streak.badges")
                        .arg(LocaleFormat.formatInteger(root.badges.length))
                        .arg(LocaleFormat.formatInteger(root.milestones.length))
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                    horizontalAlignment: Theme.textStartAlignment
                }
                ActionButton {
                    text: root.showAllBadges ? qsTrId("genycaster.streak.show_less")
                        : qsTrId("genycaster.streak.see_all")
                    quiet: true
                    compact: true
                    onClicked: root.showAllBadges = !root.showAllBadges
                }
            }

            // Fill the inspector with a compact responsive milestone grid,
            // not a horizontally clipped carousel with empty space underneath.
            // Six previews form two complete desktop rows; See all reveals the
            // full server-authored catalogue without changing tile geometry.
            GridLayout {
                id: streakBadgeGrid
                objectName: "streakBadgeGrid"
                Layout.fillWidth: true
                columns: root.width < Theme.scaleUi(root.compact ? 380 : 410) ? 2 : 3
                columnSpacing: Theme.space2
                rowSpacing: Theme.space2
                layoutDirection: Theme.uiDirection
                LayoutMirroring.enabled: false
                Repeater {
                    model: root.showAllBadges
                        ? root.milestoneItems : root.milestoneItems.slice(0, 6)
                    delegate: milestoneBadgeTile
                }
            }
            RowLayout {
                layoutDirection: Theme.uiDirection
                objectName: "streakBadgeProgress"
                Layout.fillWidth: true
                visible: root.inspectedMilestone > 0
                spacing: Theme.space2
                Label {
                    Layout.fillWidth: true
                    text: root.milestoneName(
                        root.milestoneItems.find(item =>
                            Number(item.threshold_days) === root.inspectedMilestone))
                    color: Theme.streak
                    font.pixelSize: Theme.typeCaptionSize
                }
                Label {
                    text: LocaleFormat.formatInteger(Math.min(root.record, root.inspectedMilestone))
                        + " / " + LocaleFormat.formatInteger(root.inspectedMilestone)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }

        }

        ColumnLayout {
            id: leaderboardPage
            objectName: "streakLeaderboardPage"
            Layout.fillWidth: true
            visible: root.activePage === "leaderboard"
            spacing: Theme.space3

            GenyTabStrip {
                objectName: "streakLeaderboardTrackFilter"
                Layout.fillWidth: true
                currentId: root.leaderboardTrack
                segmented: true
                distribute: true
                minimumTabWidth: 76
                tabs: [
                    {id: "activity", label: qsTrId("genycaster.streak.activity"),
                        icon: Theme.iconStreakFlame},
                    {id: "creator", label: qsTrId("genycaster.streak.creator"),
                        icon: Theme.iconCompose}
                ]
                onActivated: id => {
                    root.leaderboardTrack = id
                    if (openProofConnections) openProofConnections.fetchStreakLeaderboard(id)
                }
            }
            // A three-person preview is visible even without joining.
            // Expanding reveals all 100 in ONE parent Flickable; a nested
            // ListView used to swallow wheel/touch scrolling at the bottom.
            ColumnLayout {
                objectName: "streakLeaderboardContent"
                Layout.fillWidth: true
                spacing: Theme.space2

                Label {
                    Layout.fillWidth: true
                    visible: true
                    text: qsTrId("genycaster.streak.leaderboard_description")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Theme.textStartAlignment
                }
                ActionButton {
                    Layout.fillWidth: true
                    visible: root.leaderboardCanJoin
                    enabled: !root.leaderboardBusy
                    compact: true
                    quiet: !root.leaderboardVisible
                    selected: root.leaderboardVisible
                    text: root.leaderboardVisible
                        ? qsTrId("genycaster.streak.leave_leaderboard")
                        : qsTrId("genycaster.streak.join_leaderboard")
                    onClicked: {
                        // Membership affects the user's OWN rank, never the
                        // ability to read everyone else's public ranking.
                        if (openProofConnections) openProofConnections.setStreakPreferences(
                            String(root.streakPreferences.activity_source || "casts"),
                            !root.leaderboardVisible)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: !root.leaderboardBusy
                        && root.leaderboardEntries.length === 0
                    text: qsTrId("genycaster.streak.leaderboard_empty")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                GenySpinner {
                    Layout.alignment: Qt.AlignHCenter
                    running: visible
                    visible: root.leaderboardBusy
                }
                ColumnLayout {
                    objectName: "streakTop100"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Repeater {
                        model: root.leaderboardEntries
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.scaleUi(root.compact ? 62 : 68)
                            radius: Theme.curveCompact
                            color: Theme.surfaceMuted
                            border.color: Number(modelData.rank) === 1
                                ? Theme.streak : Theme.borderMuted
                            border.width: Number(modelData.rank) === 1 ? 1 : 0

                            RowLayout {
                                objectName: "streakLeaderboardIdentityRow"
                                anchors.fill: parent
                                anchors.leftMargin: Theme.space3
                                anchors.rightMargin: Theme.space3
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection
                                LayoutMirroring.enabled: false
                                Label {
                                    text: LocaleFormat.formatInteger(Number(modelData.rank))
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightBold
                                    color: Number(modelData.rank) <= 3
                                        ? Theme.streak : Theme.inkMuted
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: Theme.scaleUi(22)
                                }
                                Avatar {
                                    avatarSize: Theme.scaleUi(root.compact ? 34 : 38)
                                    Layout.preferredWidth: avatarSize
                                    Layout.preferredHeight: avatarSize
                                    sourceUrl: String(modelData.avatar_url || "")
                                    displayName: String(modelData.display_name || modelData.username || "")
                                    forceAutomaticDownload: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 0
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        alignToUi: true
                                        text: String(modelData.display_name
                                            || (/^fid:[0-9]+$/i.test(String(modelData.username || ""))
                                                ? "" : modelData.username)
                                            || "") || qsTrId("genycaster.streak.fid")
                                                .arg(LocaleFormat.formatInteger(Number(modelData.fid)))
                                        font.pixelSize: Theme.typeBodySize
                                        font.weight: Theme.weightDemiBold
                                        color: Theme.ink
                                        elide: Text.ElideRight
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: Theme.space1
                                        layoutDirection: Theme.uiDirection
                                        LayoutMirroring.enabled: false
                                        DirectionAwareLabel {
                                            readonly property string actualHandle: String(modelData.username || "").trim().replace(/^@/, "")
                                            visible: actualHandle.length > 0 && !/^fid:[0-9]+$/i.test(actualHandle)
                                            text: visible ? "@" + actualHandle : ""
                                            forceLeftToRight: true
                                            font.pixelSize: Theme.typeCaptionSize
                                            color: Theme.inkMuted
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: Theme.scaleUi(120)
                                        }
                                        Label {
                                            visible: String(modelData.username || "").trim().length > 0
                                                && !/^fid:[0-9]+$/i.test(String(modelData.username || "").trim())
                                            text: "·"
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeCaptionSize
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            alignToUi: true
                                            forceRightToLeft: Theme.rtl
                                            text: qsTrId("genycaster.streak.fid")
                                                .arg(LocaleFormat.formatInteger(Number(modelData.fid)))
                                            font.pixelSize: Theme.typeCaptionSize
                                            color: Theme.inkMuted
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                ColumnLayout {
                                    spacing: 1
                                    Layout.alignment: Qt.AlignVCenter
                                    DirectionAwareLabel {
                                        Layout.alignment: Theme.rtl ? Qt.AlignLeft : Qt.AlignRight
                                        horizontalAlignment: Theme.rtl ? Text.AlignLeft : Text.AlignRight
                                        forceRightToLeft: Theme.rtl
                                        text: qsTrId("genycaster.streak.leader_days")
                                            .arg(LocaleFormat.formatInteger(Number(modelData.current)))
                                        color: Theme.streak
                                        font.weight: Theme.weightDemiBold
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                    DirectionAwareLabel {
                                        Layout.alignment: Theme.rtl ? Qt.AlignLeft : Qt.AlignRight
                                        horizontalAlignment: Theme.rtl ? Text.AlignLeft : Text.AlignRight
                                        forceRightToLeft: Theme.rtl
                                        text: qsTrId("genycaster.streak.best")
                                            .arg(LocaleFormat.formatInteger(Number(modelData.longest)))
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: String(modelData.display_name || modelData.username || modelData.fid)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.profileRequested(Number(modelData.fid))
                            }
                        }
                    }
                }
            }
        }
    }
}
