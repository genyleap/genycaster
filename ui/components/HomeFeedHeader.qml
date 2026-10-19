import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string presentationMode: "for-you"
    property string visualFilter: "all"
    property string displayName: ""
    property string avatarUrl: ""
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
                                 : qsTrId("genycaster.home.greeting_night")).arg(greetingName)
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
            profiles: root.recentProfiles
            compact: root.compact
            onProfileRequested: profile => root.recentProfileRequested(profile)
            onOwnStoryRequested: root.composeRequested()
        }

        ContentTabs {
            Layout.fillWidth: true
            currentMode: root.presentationMode
            onModeRequested: mode => root.modeRequested(mode)
        }

    }
}
