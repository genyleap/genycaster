import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string activeFeed: "trending"
    readonly property bool discoveryFeed: root.activeFeed === "trending" || root.activeFeed === "recent"
                                          || root.activeFeed === "conversations"
    property var channelInfo: ({})
    property bool identityAvailable: false
    property bool compact: false
    property bool busy: false
    property bool feedStarted: false
    property bool hasContent: false
    property bool sourceUnavailable: false
    property string errorText: ""

    signal trendingRequested()
    signal followingRequested()
    signal searchRequested(string query)
    signal refreshRequested()

    implicitHeight: headerColumn.implicitHeight
    Accessible.description: root.activeFeed === "channel"
                            ? qsTrId("genycaster.feed_header.casts_scoped_to_the_channel_you_opened")
                            : root.discoveryFeed
                              ? qsTrId("genycaster.feed_header.a_network_wide_view_from_the_selected_live_source")
                              : qsTrId("genycaster.feed_header.recent_casts_from_the_people_this_identity_follows")

    function submitSearch() {
        const value = searchInput.text.trim()
        if (value.length === 0)
            return
        root.searchRequested(value)
    }

    ColumnLayout {
        id: headerColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            implicitHeight: root.discoveryFeed ? 52 : 44
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                visible: root.discoveryFeed
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveControl
                elevation: Theme.elevationControl
                shadowOpacityScale: 0.46

                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconExplore
                    color: Theme.accent
                    preferredSize: Theme.typeTitle3Size
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    text: root.activeFeed === "channel"
                          ? (String(root.channelInfo.name || root.channelInfo.id || "").length > 0
                             ? String(root.channelInfo.name || root.channelInfo.id)
                             : qsTrId("genycaster.feed_header.channel"))
                          : root.discoveryFeed ? qsTrId("genycaster.feed_header.explore")
                          : qsTrId("genycaster.feed_header.home")
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: root.discoveryFeed && !root.compact
                                    ? Theme.typeTitle1Size : Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    horizontalAlignment: Theme.textStartAlignment
                    elide: Text.ElideRight
                }

                Label {
                    visible: !root.compact
                    Layout.fillWidth: true
                    text: root.discoveryFeed
                          ? qsTrId("genycaster.feed_header.discover_people_channels_and_casts")
                          : root.activeFeed === "channel"
                            ? qsTrId("genycaster.feed_header.casts_scoped_to_the_channel_you_opened")
                            : qsTrId("genycaster.feed_header.recent_casts_from_the_people_this_identity_follows")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.textStartAlignment
                    elide: Text.ElideRight
                }
            }

            ActionButton {
                visible: root.feedStarted
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.feed_header.refresh")
                opacity: 1
                enabled: !root.busy
                onClicked: root.refreshRequested()
            }
        }

        RowLayout {
            visible: root.activeFeed === "channel" && String(root.channelInfo.id || "").length > 0
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            SuperellipseImage {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                source: String(root.channelInfo.imageUrl || "")
                mediaCategory: "photos"
                fillMode: Image.PreserveAspectCrop
                fillColor: Theme.primarySurface
                curveDepth: 13
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: "/" + String(root.channelInfo.id || "")
                    color: Theme.accent
                    font.pixelSize: Theme.typeMetaSize
                }
                DirectionAwareLabel {
                    visible: Number(root.channelInfo.followerCount || 0) > 0
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.profile.value_followers").arg(
                        LocaleFormat.formatInteger(Number(root.channelInfo.followerCount || 0)))
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    visible: String(root.channelInfo.description || "").length > 0
                    Layout.fillWidth: true
                    text: String(root.channelInfo.description || "")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                }
            }
        }

        Superellipse {
            visible: root.discoveryFeed || root.activeFeed === "channel"
            Layout.fillWidth: true
            implicitHeight: 48
            fillColor: searchInput.activeFocus ? Theme.surfaceElevated : Theme.surfaceRaised
            borderWidth: searchInput.activeFocus ? 2 : 1
            borderColor: searchInput.activeFocus ? Theme.focus : Theme.borderMuted
            curveDepth: Theme.curveControl
            elevation: searchInput.activeFocus ? Theme.elevationControl : Theme.elevationNone
            shadowColor: searchInput.activeFocus ? Theme.focusHalo : Theme.shadow
            shadowOpacityScale: 0.74

            RowLayout {
                anchors.fill: parent
                layoutDirection: Theme.uiDirection
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space2
                spacing: Theme.space2

                Icon {
                    text: Theme.iconSearch
                    color: searchInput.activeFocus ? Theme.accent : Theme.inkMuted
                    preferredSize: Theme.typeBodySize
                }

                DirectionAwareTextField {
                    id: searchInput
                    Layout.fillWidth: true
                    implicitHeight: 44
                    background: Item {}
                    placeholderText: qsTrId("genycaster.feed_header.search_people_channels_casts")
                    Accessible.name: qsTrId("genycaster.feed_header.universal_search")
                    selectByMouse: true
                    onAccepted: root.submitSearch()
                }

                ActionButton {
                    visible: searchInput.text.length > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.close")
                    onClicked: {
                        searchInput.clear()
                        searchInput.forceActiveFocus()
                    }
                }

                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    text: qsTrId("genycaster.feed_header.open")
                    enabled: !root.busy && searchInput.text.trim().length > 0
                    onClicked: root.submitSearch()
                }
            }
        }

        RowLayout {
            visible: root.discoveryFeed || root.activeFeed === "channel"
            layoutDirection: Theme.uiDirection
            Layout.fillWidth: true
            spacing: Theme.space2

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 2
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                text: root.compact
                      ? qsTrId("genycaster.feed_header.search_hint")
                      : qsTrId("genycaster.feed_header.a_network_wide_view_from_the_selected_live_source")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }

            RowLayout {
                visible: root.discoveryFeed
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection

                Superellipse {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    curveDepth: 3.5
                    exponent: 2.01
                    fillColor: root.sourceUnavailable || root.errorText.length > 0
                               ? Theme.warning
                               : root.hasContent ? Theme.success : Theme.inkSubtle
                }

                Label {
                    text: root.sourceUnavailable
                          ? qsTrId("genycaster.status.source_issue")
                          : root.errorText.length > 0
                            ? qsTrId("genycaster.status.data_issue")
                          : root.hasContent ? qsTrId("genycaster.status.live")
                                                        : qsTrId("genycaster.status.ready")
                    color: root.sourceUnavailable || root.errorText.length > 0
                           ? Theme.warning
                           : root.hasContent ? Theme.success : Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
            }
        }
    }
}
