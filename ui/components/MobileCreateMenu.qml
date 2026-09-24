import QtQuick
import QtQuick.Layouts

GenyDrawer {
    id: root
    objectName: "mobileCreateMenu"

    signal castRequested()
    signal photoRequested()
    signal spaceRequested()

    bottomSheet: true
    dimBackground: true
    contentPadding: Theme.gutter(root.width)
    width: parent ? parent.width : 420
    height: parent ? Math.min(286, parent.height * 0.38) : 286

    contentItem: ColumnLayout {
        spacing: Theme.space2

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.bottom_nav.create")
            subtitle: qsTrId("genycaster.shell.whats_happening")
            onCloseRequested: root.close()
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 1
            uniformCellWidths: true
            uniformCellHeights: true
            layoutDirection: Theme.uiDirection
            rowSpacing: Theme.space2
            columnSpacing: Theme.space2

            ActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconGlyph: Theme.iconCompose
                text: qsTrId("genycaster.home.share_cast")
                onClicked: { root.close(); root.castRequested() }
            }
            ActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconGlyph: Theme.iconImage
                text: qsTrId("genycaster.home.post_photo")
                onClicked: { root.close(); root.photoRequested() }
            }
            ActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconGlyph: Theme.iconMessage
                text: qsTrId("genycaster.home.join_space")
                onClicked: { root.close(); root.spaceRequested() }
            }
        }
    }
}
