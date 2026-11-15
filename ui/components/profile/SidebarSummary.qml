import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root

    property bool authenticated: false
    property double accountFid: 0
    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property var profileDetail: ({})
    property bool compact: false

    signal viewProfileRequested()
    signal accountRequested()

    readonly property bool ownsDetail: Number(root.profileDetail.fid || 0) > 0
                                       && Number(root.profileDetail.fid || 0) === Number(root.accountFid)
    readonly property string shownName: root.displayName.length > 0
                                        ? root.displayName
                                        : qsTrId("genycaster.side_nav.no_identity_open")
    readonly property string shownUsername: /^fid:[0-9]+$/i.test(root.username.trim())
                                            ? "" : root.username.trim()
    readonly property bool verified: root.ownsDetail && Boolean(root.profileDetail.verified || false)

    implicitHeight: root.compact ? 52 : 60
    fillColor: accountArea.hovered || accountArea.pressed ? Theme.surfaceInteractive : Theme.surfaceRaised
    borderWidth: 1
    borderColor: accountArea.activeFocus ? Theme.focus : Theme.borderMuted
    curveDepth: Theme.curveContainer

    Behavior on fillColor {
        ColorAnimation { duration: Theme.motionFast }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Avatar {
            Layout.preferredWidth: root.compact ? 32 : 36
            Layout.preferredHeight: root.compact ? 32 : 36
            avatarSize: root.compact ? 32 : 36
            sourceUrl: root.avatarUrl
            displayName: root.shownName
        }

        ColumnLayout {
            visible: !root.compact
            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight
                spacing: Theme.space1

                Label {
                    Layout.fillWidth: true
                    LayoutMirroring.enabled: false
                    text: root.shownName
                    color: Theme.textPrimary
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignLeft
                    elide: Text.ElideRight
                }

                Icon {
                    visible: root.authenticated && root.verified
                    text: Theme.iconCheck
                    color: Theme.verified
                    preferredSize: Theme.typeCaptionSize
                }
            }

            Label {
                visible: root.authenticated && root.shownUsername.trim().length > 0
                Layout.fillWidth: true
                LayoutMirroring.enabled: false
                text: root.shownUsername.trim().length > 0 ? "@" + root.shownUsername.trim() : ""
                color: Theme.textTertiary
                font.family: Theme.latinFontFamily
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }

        Icon {
            visible: !root.compact
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: Theme.textTertiary
            preferredSize: Theme.typeCaptionSize
        }
    }

    Button {
        id: accountArea
        anchors.fill: parent
        flat: true
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: root.authenticated ? root.shownName : qsTrId("genycaster.header.sign_in")
        Accessible.description: root.authenticated && root.username.length > 0 ? "@" + root.username : ""
        background: Item {}
        contentItem: Item {}
        onClicked: root.authenticated ? root.viewProfileRequested() : root.accountRequested()
    }
}
