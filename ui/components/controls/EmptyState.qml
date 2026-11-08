import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property bool runtimeReady: false
    property bool busy: false
    property bool offline: false
    property bool sourceUnavailable: false
    property string errorText: ""
    property string activeFeed: "following"
    property bool identityAvailable: false
    property bool feedStarted: false
    property bool filtered: false
    // A semantic feed is still allowed to complete in the background; this
    // indicates only that its first visible match exceeded the UI deadline.
    property bool timedOut: false
    property bool moreFilteredAvailable: false
    signal moreFilteredRequested()

    signal retryRequested()
    signal identityRequested()
    signal followingRequested()
    signal trendingRequested()

    Accessible.description: root.busy
                            ? qsTrId("genycaster.empty.loading_your_reading_surface")
                            : qsTrId("genycaster.empty.genycaster_checks_local_state_first_then_refreshes_fro")

    // A ListView footer measures the complete empty state before allocating
    // its scrollable height. This avoids centering a tall message in a zero/
    // negative-height overlay when the mobile feed header occupies the view.
    implicitHeight: stateContent.implicitHeight + Theme.space6 * 2

    readonly property bool setupFailure: !root.runtimeReady && root.errorText.length > 0
    readonly property bool idleHome: root.runtimeReady
                                     && !root.feedStarted
                                     && !root.busy
                                     && root.errorText.length === 0
    readonly property bool recoverableSourceFailure: root.runtimeReady && root.sourceUnavailable && !root.offline && !root.busy
    readonly property bool discoveryUnavailable: root.recoverableSourceFailure
                                               && (root.activeFeed === "trending" || root.activeFeed === "recent"
                                                   || root.activeFeed === "conversations")
    readonly property string stateIcon: root.setupFailure || root.sourceUnavailable || root.timedOut ? Theme.iconWarning
                                        : root.offline ? Theme.iconWifi
                                        : root.idleHome ? Theme.iconProfile
                                        : Theme.iconLeaf

    ColumnLayout {
        id: stateContent
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space6, 400)
        spacing: Theme.space2

        Superellipse {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            fillColor: root.setupFailure || root.offline || root.sourceUnavailable || root.timedOut
                       ? Theme.warningSurface : Theme.primarySurface
            curveDepth: Theme.curveControl

            Icon {
                anchors.centerIn: parent
                text: root.stateIcon
                color: root.setupFailure || root.offline || root.sourceUnavailable || root.timedOut
                    ? Theme.warning : Theme.accent
                preferredSize: Theme.typeTitle3Size
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.setupFailure
                  ? qsTrId("genycaster.empty.live_reading_is_not_configured")
                  : root.idleHome
                    ? qsTrId("genycaster.empty.open_your_farcaster_home")
                    : root.offline
                      ? qsTrId("genycaster.empty.no_saved_posts_for_offline_reading")
                    : root.sourceUnavailable || root.timedOut
                      ? (root.activeFeed === "trending"
                         ? qsTrId("genycaster.empty.trending_is_unavailable_from_this_source")
                         : qsTrId("genycaster.empty.the_live_source_did_not_return_this_feed"))
                      : root.errorText.length > 0
                        ? qsTrId("genycaster.empty.this_feed_could_not_be_loaded")
                        : root.filtered ? qsTrId("genycaster.feed.filtered_empty")
                                        : qsTrId("genycaster.empty.nothing_here_yet")
            color: Theme.ink
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            text: root.setupFailure
                  ? root.errorText
                  : root.idleHome
                    ? (root.identityAvailable
                       ? qsTrId("genycaster.empty.open_home_for_this_identity_or_use_explore_when_you_wa")
                       : qsTrId("genycaster.empty.open_an_identity_for_your_home_feed_or_use_explore_for"))
                    : root.offline
                      ? qsTrId("genycaster.empty.reconnect_and_refresh_once_to_populate_the_local_cache")
                    : root.discoveryUnavailable
                      ? (root.identityAvailable
                         ? qsTrId("genycaster.empty.explore_is_temporarily_unavailable_your_home_feed_rema")
                         : qsTrId("genycaster.empty.explore_is_temporarily_unavailable_open_an_identity_fo"))
                      : root.errorText.length > 0
                        ? root.errorText
                        : root.sourceUnavailable || root.timedOut
                          ? qsTrId("genycaster.empty.your_device_may_still_be_online_retry_or_use_another_a")
                          : root.filtered ? qsTrId("genycaster.feed.filtered_description")
                                          : qsTrId("genycaster.empty.open_an_identity_choose_another_feed_or_refresh_this_s")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeMetaSize
            lineHeight: Theme.typeMetaLineHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        RowLayout {
            visible: root.runtimeReady
                     && (root.idleHome || root.sourceUnavailable || root.timedOut
                         || root.offline || root.errorText.length > 0)
            layoutDirection: Theme.uiDirection
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space2
            spacing: Theme.space2

            ActionButton {
                visible: root.idleHome || (root.discoveryUnavailable && root.identityAvailable)
                text: root.identityAvailable ? qsTrId("genycaster.empty.open_home") : qsTrId("genycaster.empty.open_identity")
                prominent: true
                onClicked: root.identityAvailable ? root.followingRequested() : root.identityRequested()
            }

            ActionButton {
                visible: root.idleHome
                text: qsTrId("genycaster.empty.browse_explore")
                quiet: true
                onClicked: root.trendingRequested()
            }

            ActionButton {
                visible: !root.idleHome
                text: root.offline ? qsTrId("genycaster.empty.try_again") : qsTrId("genycaster.empty.retry")
                selected: root.discoveryUnavailable
                quiet: !root.discoveryUnavailable
                onClicked: root.retryRequested()
            }
        }
        ActionButton {
            visible: root.filtered && root.moreFilteredAvailable && !root.sourceUnavailable
                     && !root.timedOut && !root.offline && root.errorText.length === 0
            Layout.alignment: Qt.AlignHCenter
            text: qsTrId("genycaster.feed.more_filtered")
            enabled: !root.busy
            onClicked: root.moreFilteredRequested()
        }
    }
}
