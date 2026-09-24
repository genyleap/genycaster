import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var modelData
    property bool compact: false
    property bool interactionsAvailable: false
    property double accountFid: 0

    signal openRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()

    implicitHeight: column.implicitHeight + Theme.space3

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Icon { text: Theme.iconCode; color: Theme.accent; preferredSize: Theme.typeMetaSize }
            Label {
                text: qsTrId("genycaster.home.builder_notes")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }
            Item { Layout.fillWidth: true }
        }

        CastItem {
            id: cast
            Layout.fillWidth: true
            modelData: root.modelData
            compact: root.compact
            interactionsAvailable: root.interactionsAvailable
            accountFid: root.accountFid
            onOpenRequested: castId => root.openRequested(castId)
            onProfileRequested: fid => root.profileRequested(fid)
            onReplyRequested: data => root.replyRequested(data)
            onReactionRequested: (kind, data, active) => root.reactionRequested(kind, data, active)
            onRemoveRequested: data => root.removeRequested(data)
            onExternalRequested: url => root.externalRequested(url)
            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
            onAuthenticationRequested: root.authenticationRequested()
        }
    }
}
