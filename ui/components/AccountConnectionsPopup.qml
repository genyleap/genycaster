import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

/** A lightweight, account-owner-only selector. OpenProof owns the actual
 * provider links, disconnects and browser-session validation. */
GenyModal {
    id: picker
    objectName: "accountConnectionsPopup"
    property bool compactLayout: width < 440
    property var providers: []
    signal providerSelected(string provider)

    width: Math.min(480, parent ? Math.max(280, parent.width - Theme.space4 * 2) : 480)
    height: Math.min(580, parent ? Math.max(280, parent.height - Theme.space4 * 2) : 580)
    padding: Theme.space3

    function providerTitle(provider) {
        switch (provider) {
        case "x": return "X (Twitter)"
        case "github": return "GitHub"
        case "google": return "Google"
        case "linkedin": return "LinkedIn"
        case "telegram": return "Telegram"
        case "farcaster": return "Farcaster"
        case "ethereum-wallet": return qsTr("EVM wallet")
        default: return provider
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space2

        ModalHeader {
            Layout.fillWidth: true
            title: qsTr("Connected accounts")
            subtitle: qsTr("Choose a provider to manage its connection securely through OpenProof.")
            onCloseRequested: picker.close()
        }

        ScrollView {
            id: providerScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: GenyScrollBar {}

            ColumnLayout {
                width: providerScroll.availableWidth
                spacing: Theme.space2

                Repeater {
                    model: picker.providers
                    delegate: ActionButton {
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.minimumHeight: 48
                        text: picker.providerTitle(modelData)
                        iconGlyph: Theme.iconLink
                        quiet: true
                        alignStart: true
                        onClicked: {
                            picker.providerSelected(modelData)
                            picker.close()
                        }
                    }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTr("Connection status and confirmation are verified in your OpenProof account. This list only shows supported providers.")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }
}
