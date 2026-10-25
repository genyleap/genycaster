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
    layer.enabled: false

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

            Image {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize.width: 60
                sourceSize.height: 60
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            ColumnLayout {
                spacing: 0
                Image {
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 20
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    source: Theme.dark
                            ? "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg"
                            : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg"
                    sourceSize.width: 300
                    sourceSize.height: 40
                    Accessible.name: "GenyCaster"
                }
                Label {
                    text: qsTrId("genycaster.release.beta_1_0")
                    color: Theme.inkMuted
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    renderType: Text.NativeRendering
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
            // Signed-in mobile users reach Profile from the bottom tab.
            // Keep Sign in here for guests, and retain the desktop account action.
            visible: !root.compact || !root.isAuthenticated
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
            // Authenticated identity icon navigates to the profile directly.
            // Revocation and Sign Out belong in Settings / Security instead.
            onClicked: root.isAuthenticated ? root.ownProfileRequested() : root.accountRequested()
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
