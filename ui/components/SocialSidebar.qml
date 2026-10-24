import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string mode: "feed"
    property bool authenticated: false
    property bool busy: false
    property bool offline: false
    property bool stale: false
    property int notificationUnreadCount: 0
    property double accountFid: 0
    property string accountName: ""
    property string accountUsername: ""
    property string accountAvatarUrl: ""
    property var accountProfile: ({})
    property var currentProfile: ({})
    property var feedRows: []

    signal searchRequested()
    signal accountRequested()
    signal ownProfileRequested()
    signal profileRequested(double fid)
    signal notificationsRequested()
    signal exploreRequested()

    readonly property var shownProfile: root.mode === "profile" && Number(root.currentProfile.fid || 0) > 0
                                        ? root.currentProfile : root.accountProfile
    // Keep the social rail intentionally single-column. A context sidebar is a
    // vertical scanning surface, not a dashboard grid; widening it should improve
    // text/media breathing room without changing information hierarchy.
    readonly property var recentProfiles: root.collectRecentProfiles(4)

    function collectRecentProfiles(limit) {
        const result = []
        const seen = ({})
        const source = root.feedRows || []
        const currentFid = Number(root.shownProfile.fid || 0)
        for (let index = 0; index < source.length && result.length < limit; ++index) {
            const row = source[index] || ({})
            const fid = Number(row.authorFid || row.fid || 0)
            const username = String(row.author || row.username || "")
            const key = fid > 0 ? "fid:" + fid : "user:" + username.toLowerCase()
            if (key === "user:" || seen[key] || (currentFid > 0 && fid === currentFid))
                continue
            seen[key] = true
            result.push({
                fid: fid,
                username: username,
                displayName: String(row.displayName || username || qsTrId("genycaster.side_nav.profile")),
                avatarUrl: String(row.avatarUrl || "")
            })
        }
        return result
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ScrollView {
        id: socialSidebarScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar {}

        ColumnLayout {
            width: socialSidebarScroll.availableWidth
            spacing: Theme.space2

            Item {
                Layout.preferredHeight: Theme.space1
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                implicitHeight: 44
                fillColor: searchHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: searchHover.hovered ? Theme.borderDefault : Theme.borderSubtle
                curveDepth: Theme.curveControl

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Icon {
                        text: Theme.iconSearch
                        color: Theme.textTertiary
                        preferredSize: Theme.iconSizeSmall
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.social.search_placeholder")
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }

                HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.searchRequested() }
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                implicitHeight: profileColumn.implicitHeight + Theme.space3 * 2
                Layout.alignment: Qt.AlignTop
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: profileColumn
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        Avatar {
                            avatarSize: 44
                            sourceUrl: root.mode === "profile"
                                       ? String(root.shownProfile.avatarUrl || root.shownProfile.pfpUrl || "")
                                       : root.accountAvatarUrl
                            displayName: root.mode === "profile"
                                         ? String(root.shownProfile.displayName || root.shownProfile.username || qsTrId("genycaster.side_nav.profile"))
                                         : (root.accountName.length > 0 ? root.accountName : qsTrId("genycaster.social.your_profile"))
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.mode === "profile"
                                      ? String(root.shownProfile.displayName || root.shownProfile.username || qsTrId("genycaster.side_nav.profile"))
                                      : (root.accountName.length > 0 ? root.accountName : qsTrId("genycaster.social.your_profile"))
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.mode === "profile"
                                      ? (String(root.shownProfile.username || "").length > 0
                                         ? "@" + String(root.shownProfile.username || "") : "")
                                      : (root.accountUsername.length > 0 ? "@" + root.accountUsername : qsTrId("genycaster.social.sign_in_to_personalize"))
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        ActionButton {
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconNotification
                            badgeText: root.notificationUnreadCount > 0
                                       ? (root.notificationUnreadCount > 99
                                          ? LocaleFormat.formatInteger(99) + "+"
                                          : LocaleFormat.formatInteger(root.notificationUnreadCount))
                                       : ""
                            text: qsTrId("genycaster.side_nav.notifications")
                            onClicked: root.notificationsRequested()
                        }
                    }

                    DirectionAwareLabel {
                        visible: root.mode === "profile" && String(root.shownProfile.bio || "").trim().length > 0
                        Layout.fillWidth: true
                        text: String(root.shownProfile.bio || "").replace(/\s+/g, " ").trim()
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        visible: root.mode === "profile" && Number(root.shownProfile.fid || 0) > 0
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection

                        DirectionAwareLabel {
                            text: qsTrId("genycaster.social.followers_count").arg(root.shownProfile.socialCountsKnown === false
                                                   ? "—"
                                                   : LocaleFormat.formatCompactNumber(Number(root.shownProfile.followerCount || 0)))
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            text: qsTrId("genycaster.social.following_count").arg(root.shownProfile.socialCountsKnown === false
                                                   ? "—"
                                                   : LocaleFormat.formatCompactNumber(Number(root.shownProfile.followingCount || 0)))
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.mode === "profile"
                                 && Number(root.shownProfile.fid || 0) > 0
                                 && root.shownProfile.socialCountsKnown === true
                                 && providerProfiles.activeReadContract === "snapchain-v1"
                        text: qsTrId("genycaster.nodes.counts_external").arg(providerProfiles.activeReadProviderLabel)
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        compact: true
                        prominent: root.authenticated
                        iconGlyph: Theme.iconProfile
                        text: root.mode === "profile" ? qsTrId("genycaster.social.open_profile")
                              : root.authenticated ? qsTrId("genycaster.social.your_profile") : qsTrId("genycaster.notifications.sign_in")
                        onClicked: {
                            if (root.mode === "profile" && Number(root.shownProfile.fid || 0) > 0)
                                root.profileRequested(Number(root.shownProfile.fid || 0))
                            else if (root.authenticated)
                                root.ownProfileRequested()
                            else
                                root.accountRequested()
                        }
                    }
                }
            }

            FeedPulseCard {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                rows: root.feedRows
                scopeMode: root.mode === "profile" ? "profile" : "feed"
                busy: root.busy
                stale: root.stale
                offline: root.offline
            }

            Superellipse {
                visible: root.recentProfiles.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                implicitHeight: recentColumn.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: recentColumn
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.social.people_in_your_orbit")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightBold
                            elide: Text.ElideRight
                        }
                        ActionButton {
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconExplore
                            text: qsTrId("genycaster.feed_header.explore")
                            onClicked: root.exploreRequested()
                        }
                    }

                    Repeater {
                        model: root.recentProfiles

                        delegate: Item {
                            id: personRow
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 46

                            RowLayout {
                                anchors.fill: parent
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection

                                Avatar {
                                    avatarSize: 34
                                    sourceUrl: String(personRow.modelData.avatarUrl || "")
                                    displayName: String(personRow.modelData.displayName || "")
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: String(personRow.modelData.displayName || "")
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.typeMetaSize
                                        font.weight: Theme.weightDemiBold
                                        elide: Text.ElideRight
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: String(personRow.modelData.username || "").length > 0
                                              ? "@" + String(personRow.modelData.username || "") : ""
                                        color: Theme.textTertiary
                                        font.pixelSize: Theme.typeCaptionSize
                                        elide: Text.ElideRight
                                    }
                                }
                                Icon {
                                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                    color: Theme.textTertiary
                                    preferredSize: Theme.typeCaptionSize
                                }
                            }

                            HoverHandler { id: personHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    const fid = Number(personRow.modelData.fid || 0)
                                    if (fid > 0)
                                        root.profileRequested(fid)
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: Theme.space2
            }
        }
    }
}
