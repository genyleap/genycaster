import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyDrawer {
    id: root
    objectName: "mobileCreateMenu"

    signal castRequested()
    signal photoRequested()
    // Spaces is a future capability; do not expose a working action.
    signal spaceRequested()

    bottomSheet: true
    floatingSheet: true
    interactive: true
    dimBackground: true
    contentPadding: Theme.space3
    // The default sheet inset plus a fixed 360px height left empty bands
    // above Create and below the actions on iPhone. Size to real content.
    topPadding: Theme.space5
    bottomPadding: Theme.space3 + Theme.space3
    width: parent ? parent.width : 420
    height: parent ? Math.min(Math.max(1, parent.height - Theme.space4),
                               sheetContent.implicitHeight + root.topPadding + root.bottomPadding)
                   : 328

    contentItem: ColumnLayout {
        id: sheetContent
        spacing: Theme.space2

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.bottom_nav.create")
            sheetPresentation: true
            onCloseRequested: root.close()
        }

        // One Superellipse owns the exterior curves. Rows inside are flat,
        // separated by hairlines, so they never fight the sheet's silhouette.
        Superellipse {
            objectName: "createUnifiedActions"
            Layout.fillWidth: true
            Layout.preferredHeight: 216
            fillColor: Theme.surface
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveLarge

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                component CreateRow: Item {
                    id: choice
                    property string titleText: ""
                    property string subtitleText: ""
                    property string iconGlyph: ""
                    property bool available: true
                    signal activated()
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    activeFocusOnTab: available
                    Accessible.role: available ? Accessible.Button : Accessible.StaticText
                    Accessible.name: titleText
                    Accessible.onPressAction: if (available) choice.activated()
                    Keys.onReturnPressed: if (available) choice.activated()
                    Keys.onSpacePressed: if (available) choice.activated()

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.surfacePressed
                        opacity: choiceMouse.pressed && choice.available ? 0.8 : 0
                        radius: Theme.space2
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space3
                        anchors.rightMargin: Theme.space3
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection
                        Icon {
                            text: choice.iconGlyph
                            preferredSize: Theme.iconSizeLarge
                            color: choice.available ? Theme.textPrimary : Theme.textTertiary
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: choice.titleText
                                color: choice.available ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightDemiBold
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: choice.subtitleText
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        Icon {
                            text: choice.available
                                  ? (Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight)
                                  : "system/lock"
                            preferredSize: Theme.iconSizeSmall
                            color: Theme.textTertiary
                        }
                    }
                    MouseArea {
                        id: choiceMouse
                        anchors.fill: parent
                        enabled: choice.available
                        cursorShape: choice.available ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: choice.activated()
                    }
                }

                CreateRow {
                    titleText: qsTrId("genycaster.home.share_cast")
                    subtitleText: qsTrId("genycaster.home.compose_description")
                    iconGlyph: Theme.iconCompose
                    onActivated: { root.close(); root.castRequested() }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderMuted
                }
                CreateRow {
                    titleText: qsTrId("genycaster.home.post_photo")
                    subtitleText: qsTrId("genycaster.home.hub_photo_detail")
                    iconGlyph: Theme.iconImage
                    onActivated: { root.close(); root.photoRequested() }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderMuted
                }
                CreateRow {
                    titleText: qsTrId("genycaster.home.join_space")
                    subtitleText: qsTrId("genycaster.home.hub_coming_soon")
                    iconGlyph: Theme.iconMessage
                    available: false
                }
            }
        }
    }
}
