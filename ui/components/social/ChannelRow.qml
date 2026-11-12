import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One stable, reusable public channel row. No membership mutations.
Item {
    id: root
    property var channelData: ({})
    property bool compact: false
    property bool allowed: true
    property bool scrolling: false
    readonly property string channelId: String(channelData.id || "")
    readonly property bool joined: Boolean(channelData.isMember)
    readonly property bool displayed: channelId.length > 0 && allowed
    signal openRequested(var channelInfo)

    implicitHeight: surface.implicitHeight
    height: displayed ? implicitHeight + Theme.space2 : 0
    visible: displayed
    activeFocusOnTab: displayed
    Accessible.role: Accessible.Button
    Accessible.name: String(channelData.name || channelId)
    Accessible.description: joined ? qsTrId("genycaster.composer.joined_channel") : ""
    Accessible.onPressAction: root.openRequested(root.channelData)
    Keys.onReturnPressed: root.openRequested(root.channelData)
    Keys.onSpacePressed: root.openRequested(root.channelData)

    Superellipse {
        id: surface
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        implicitHeight: content.implicitHeight + Theme.space3 * 2
        height: implicitHeight
        fillColor: root.activeFocus ? Theme.surfaceSelected
                   : rowHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: root.activeFocus ? Theme.accent : Theme.borderSubtle
        curveDepth: Theme.curveContainer

        RowLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space3
            layoutDirection: Theme.uiDirection
            spacing: Theme.space3

            Avatar {
                Layout.alignment: Qt.AlignTop
                avatarSize: root.compact ? 44 : 52
                sourceUrl: String(root.channelData.imageUrl || "")
                displayName: String(root.channelData.name || root.channelId)
                forceAutomaticDownload: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: String(root.channelData.name || root.channelId)
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: "/" + root.channelId
                    forceLeftToRight: true
                    color: Theme.accent
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    visible: String(root.channelData.description || "").length > 0
                    Layout.fillWidth: true
                    text: String(root.channelData.description || "")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    readonly property int followers: Number(root.channelData.followerCount ?? -1)
                    readonly property int members: Number(root.channelData.memberCount ?? -1)
                    visible: followers >= 0 || members >= 0
                    Layout.fillWidth: true
                    text: followers >= 0
                          ? qsTrId("genycaster.composer.channel_follower_count")
                              .arg(LocaleFormat.formatInteger(followers))
                          : qsTrId("genycaster.composer.channel_member_count")
                              .arg(LocaleFormat.formatInteger(members))
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space2
                DirectionAwareLabel {
                    visible: root.joined
                    text: qsTrId("genycaster.composer.joined_channel")
                    color: Theme.accent
                    font.pixelSize: Theme.typeCaptionSize
                }
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    color: Theme.inkMuted
                    preferredSize: Theme.iconSizeSmall
                }
            }
        }
    }
    HoverHandler {
        id: rowHover
        enabled: root.displayed && !root.scrolling
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        enabled: root.displayed
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.openRequested(root.channelData)
    }
}
