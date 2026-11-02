import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string presentationMode: "for-you"
    property string visualFilter: "all"
    property string audience: "all"
    property bool filterAvailable: false
    signal filterRequested()
    signal audienceRequested(string audience)
    property string displayName: ""
    property string avatarUrl: ""
    // Forward the signed-in account photo to the self-story delegate.
    property string ownAvatarUrl: ""
    property var recentProfiles: []
    property bool authenticated: false
    property bool interactionsAvailable: false
    property bool compact: false
    property int localHour: new Date().getHours()

    signal modeRequested(string mode)
    signal visualFilterRequested(string filter)
    signal composeRequested()
    signal photoRequested()
    signal spaceRequested()
    signal recentProfileRequested(var profile)
    signal authenticationRequested()
    signal searchRequested()

    implicitHeight: column.implicitHeight + (root.compact ? Theme.space2 : Theme.space3)

    readonly property int gutter: Theme.gutter(root.width)

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: root.localHour = new Date().getHours()
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.gutter
        anchors.rightMargin: root.gutter
        anchors.topMargin: root.compact ? Theme.space3 : Theme.space4
        spacing: root.compact ? Theme.space2 : Theme.space3

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    readonly property string greetingName: root.displayName.length > 0
                                                               ? root.displayName : qsTrId("genycaster.app.genycaster")
                    text: (root.localHour < 5
                           ? qsTrId("genycaster.home.greeting_night")
                           : root.localHour < 12
                             ? qsTrId("genycaster.home.greeting_morning")
                             : root.localHour < 17
                               ? qsTrId("genycaster.home.greeting_afternoon")
                               : root.localHour < 21
                                 ? qsTrId("genycaster.home.greeting_evening")
                                 : qsTrId("genycaster.home.greeting_night")).arg(
                                     "\u2066" + greetingName + "\u2069")
                    color: Theme.textPrimary
                    font.pixelSize: root.compact ? Theme.typeTitle3Size : Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    maximumLineCount: root.compact ? 2 : 1
                    wrapMode: root.compact ? Text.Wrap : Text.NoWrap
                    elide: Text.ElideRight
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.home.catch_up")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                }
            }

            Superellipse {
                Layout.preferredWidth: root.compact ? 40 : 184
                Layout.preferredHeight: 44
                fillColor: root.compact ? "transparent"
                                        : (searchHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                borderWidth: root.compact ? 0 : 1
                borderColor: searchHover.hovered ? Theme.borderStrong : Theme.borderMuted
                curveDepth: height / 2
                elevation: root.compact ? Theme.elevationNone : Theme.elevationControl

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.compact ? Theme.space2 : Theme.space3
                    anchors.rightMargin: anchors.leftMargin
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Superellipse {
                        Layout.preferredWidth: root.compact ? 28 : 30
                        Layout.preferredHeight: Layout.preferredWidth
                        fillColor: root.compact ? "transparent" : Theme.primarySurface
                        curveDepth: width / 2
                        exponent: 2.01

                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconSearch
                            color: root.compact ? Theme.ink : Theme.accent
                            preferredSize: Theme.iconSize
                        }
                    }

                    DirectionAwareLabel {
                        visible: !root.compact
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.feed_header.universal_search")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightMedium
                        elide: Text.ElideRight
                    }
                }

                HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.searchRequested()
                }
            }
        }

        HomeComposer {
            Layout.fillWidth: true
            compact: root.compact
            avatarUrl: root.avatarUrl
            displayName: root.displayName
            interactionsAvailable: root.interactionsAvailable
            onComposeRequested: root.composeRequested()
            onPhotoRequested: root.photoRequested()
            onSpaceRequested: root.spaceRequested()
            onAuthenticationRequested: root.authenticationRequested()
        }

        StoryStrip {
            Layout.fillWidth: true
            profiles: root.authenticated ? root.recentProfiles : []
            followingOnly: true
            ownAvatarUrl: root.ownAvatarUrl
            compact: root.compact
            onProfileRequested: profile => root.recentProfileRequested(profile)
            onOwnStoryRequested: root.composeRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.home.scope_feed")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }

            Rectangle {
                id: audienceSelector
                objectName: "explorerAudienceSelector"
                Layout.preferredWidth: root.compact ? 205 : 228
                Layout.preferredHeight: 38
                radius: 13
                color: Theme.surfaceRaised
                border.width: Theme.strokeHairline
                border.color: Theme.borderSubtle

                // Keep one persistent selection pill. Moving it is smoother
                // than replacing two selected delegate backgrounds on clicks.
                Rectangle {
                    id: audienceSelectionPill
                    width: (audienceSelector.width - 8) / 2
                    height: audienceSelector.height - 6
                    y: 3
                    x: 3 + ((root.audience === "following") !== Theme.rtl
                            ? width + 2 : 0)
                    radius: 10
                    color: Theme.backgroundPrimary
                    border.width: Theme.strokeHairline
                    border.color: Theme.borderSubtle
                    Behavior on x {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 2
                    layoutDirection: Theme.uiDirection

                    Repeater {
                        model: [
                            { id: "all", title: qsTrId("genycaster.home.scope_everyone") },
                            { id: "following", title: qsTrId("genycaster.home.scope_following") }
                        ]

                        delegate: Button {
                            id: scopeOption
                            required property var modelData
                            readonly property bool selected: root.audience === String(modelData.id)
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            padding: 0
                            hoverEnabled: true
                            enabled: String(modelData.id) !== "following"
                                     || root.authenticated
                            background: Rectangle {
                                radius: 10
                                color: scopeOption.selected ? "transparent"
                                     : scopeOption.hovered ? Theme.surfaceHover : "transparent"
                                border.width: 0
                                border.color: Theme.borderSubtle
                            }
                            contentItem: DirectionAwareLabel {
                                text: String(scopeOption.modelData.title)
                                font.pixelSize: Theme.typeCaptionSize
                                // Selection must not change glyph metrics while
                                // the persistent pill is moving.
                                font.weight: Theme.weightDemiBold
                                color: scopeOption.enabled
                                    ? (scopeOption.selected ? Theme.ink : Theme.inkMuted)
                                    : Theme.inkMuted
                                opacity: scopeOption.enabled ? 1 : 0.5
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            onClicked: {
                                if (!scopeOption.selected)
                                    root.audienceRequested(String(modelData.id))
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: String(modelData.title)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection

            ContentTabs {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: 48
                currentMode: root.presentationMode
                onModeRequested: mode => root.modeRequested(mode)
            }
            ActionButton {
                objectName: "explorerFilterTrigger"
                visible: root.filterAvailable
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: "filter"
                text: qsTrId("genycaster.filters.explorer")
                onClicked: root.filterRequested()
            }
        }

    }
}
