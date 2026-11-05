import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// No local bridge URL, session token or wallet credential is exposed here.
// The C++ controller validates the destination and owns process launches.
Item {
    id: picker
    implicitWidth: 0
    implicitHeight: 0

    GenyModal {
        id: dialog
        parent: picker.parent
        z: 1200
        padding: Theme.space4
        width: Math.min(480, parent ? parent.width - Theme.space5 * 2 : 480)
        height: Math.min(540, parent ? parent.height - Theme.space5 * 2 : 540)
        closePolicy: Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: Theme.space3

            ModalHeader {
                Layout.fillWidth: true
                title: qsTr("Connect your wallet")
                subtitle: qsTr("Choose a browser for the Mini App wallet request")
                onCloseRequested: miniAppHost.cancelWalletBrowser()
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTr("GenyCaster found these installed browsers. A browser is not a wallet: a compatible wallet extension must also be enabled.")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeMetaSize
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Repeater {
                    model: miniAppHost.walletBrowserOptions
                    delegate: ActionButton {
                        required property var modelData
                        Layout.fillWidth: true
                        alignStart: true
                        quiet: true
                        text: String(modelData.label || "")
                        onClicked: miniAppHost.chooseWalletBrowser(String(modelData.id || ""))
                    }
                }
            }

            DirectionAwareLabel {
                visible: miniAppHost.walletBrowserError.length > 0
                Layout.fillWidth: true
                text: miniAppHost.walletBrowserError
                color: Theme.danger
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTr("Your private key stays in your wallet. Connecting a wallet does not authorize future signatures or transactions.")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
            }

            Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
            ActionButton {
                Layout.alignment: Qt.AlignRight
                quiet: true
                text: qsTr("Cancel")
                onClicked: miniAppHost.cancelWalletBrowser()
            }
        }

        onClosed: {
            if (miniAppHost.walletBrowserPickerActive)
                miniAppHost.cancelWalletBrowser()
        }
    }

    Connections {
        target: miniAppHost
        function onWalletBrowserPickerChanged() {
            if (miniAppHost.walletBrowserPickerActive) {
                if (!dialog.opened) dialog.open()
            } else if (dialog.opened) {
                dialog.close()
            }
        }
    }
    Component.onCompleted: {
        if (miniAppHost.walletBrowserPickerActive)
            dialog.open()
    }
}
