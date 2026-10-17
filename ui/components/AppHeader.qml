import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool isAuthenticated: false
    property bool writeReady: false
    property bool writeBusy: false
    property bool compact: false
    property bool showBrand: true
    property int notificationUnreadCount: 0
    property bool notificationsSelected: false

    signal accountRequested()
    signal ownProfileRequested()
    signal notificationsRequested()
    signal settingsRequested()

    implicitHeight: Theme.appBarHeight
    color: Theme.backgroundPrimary

    readonly property int gutter: Theme.gutter(root.width)

    RowLayout {
        anchors.fill: parent
        layoutDirection: Theme.uiDirection
        anchors.leftMargin: root.gutter
        anchors.rightMargin: root.gutter
        spacing: Theme.space1

        RowLayout {
            visible: root.showBrand
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                fillColor: Theme.primaryFill
                curveDepth: Theme.curveControl
                elevation: Theme.elevationControl
                shadowOpacityScale: 0.48

                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconLeaf
                    color: Theme.accentInk
                    preferredSize: Theme.iconSizeLarge
                }
            }

            ColumnLayout {
                spacing: 0
                Label {
                    text: qsTrId("genycaster.header.genycaster")
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }
                Label {
                    text: qsTrId("genycaster.release.beta_1_0")
                    color: Theme.inkMuted
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    Accessible.ignored: true
                }
            }
        }

        Item { Layout.fillWidth: true }

        ActionButton {
            compact: true
            quiet: true
            selected: root.notificationsSelected
            iconOnly: true
            iconGlyph: Theme.iconNotification
            badgeText: root.notificationUnreadCount > 0 && !root.notificationsSelected
                       ? (root.notificationUnreadCount > 99 ? (LocaleFormat.formatInteger(99) + "+")
                          : LocaleFormat.formatInteger(root.notificationUnreadCount))
                       : ""
            text: qsTrId("genycaster.side_nav.notifications")
            onClicked: root.notificationsRequested()
        }

        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconSettings
            text: qsTrId("genycaster.settings.title")
            Accessible.name: qsTrId("genycaster.settings.open")
            onClicked: root.settingsRequested()
        }



        ActionButton {
            compact: true
            quiet: root.isAuthenticated
            prominent: !root.isAuthenticated
            iconOnly: root.compact
            iconGlyph: root.isAuthenticated ? Theme.iconProfile : Theme.iconSignIn
            text: root.isAuthenticated
                  ? qsTrId("genycaster.header.account")
                  : qsTrId("genycaster.header.sign_in")
            Accessible.description: root.isAuthenticated
                                    ? qsTrId("genycaster.header.open_your_signed_in_account_session")
                                    : qsTrId("genycaster.header.sign_in_to_interact_on_farcaster")
            onClicked: {
                if (root.isAuthenticated)
                    root.ownProfileRequested()
                else
                    root.accountRequested()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderMuted
        opacity: 0.72
    }
}
