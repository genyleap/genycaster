import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

/** Account connections stay inside Genycaster. Provider consent is delegated
 * to the signed OpenProof handoff, and no OAuth token enters QML. */
GenyModal {
    id: picker
    objectName: "accountConnectionsPopup"
    property var providers: []
    property var connections: []
    property bool authorized: false
    property bool authorizing: false
    property bool actionBusy: false
    property string approvalUrl: ""
    property string consentUrl: ""
    property string errorText: ""
    signal consentRequested()

    signal authorizeRequested()
    signal approvalRequested(string url)
    signal refreshRequested()
    signal connectRequested(string provider)
    signal disconnectRequested(string provider, string subject)

    width: Math.min(500, parent ? Math.max(280, parent.width - Theme.space4 * 2) : 500)
    height: Math.min(620, parent ? Math.max(310, parent.height - Theme.space4 * 2) : 620)
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

    function connectionFor(provider) {
        for (let i = 0; i < connections.length; i++) {
            const connection = connections[i]
            if (String(connection.provider || "") === provider)
                return connection
        }
        return null
    }

    contentItem: ColumnLayout {
        spacing: Theme.space2

        ModalHeader {
            Layout.fillWidth: true
            title: qsTr("Connections")
            subtitle: qsTr("Connect accounts securely without leaving Genycaster.")
            onCloseRequested: picker.close()
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: !picker.authorized
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: picker.authorizing
                      ? qsTr("Confirm your Farcaster identity with OpenProof. Connections will be linked only to this verified profile.")
                      : qsTr("Verify this Farcaster profile once through OpenProof to manage your social connections here.")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }

            ActionButton {
                Layout.fillWidth: true
                enabled: !picker.authorizing
                text: picker.authorizing ? qsTr("Waiting for authorization…")
                                         : qsTr("Verify my Farcaster profile")
                iconGlyph: Theme.iconLink
                prominent: true
                onClicked: picker.authorizeRequested()
            }

            ActionButton {
                Layout.fillWidth: true
                visible: picker.authorizing && picker.approvalUrl.length > 0
                text: qsTr("Open secure approval")
                quiet: true
                onClicked: picker.approvalRequested(picker.approvalUrl)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: picker.authorized

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTr("Verified by OpenProof")
                color: Theme.inkMuted
            }

            ActionButton {
                compact: true
                quiet: true
                text: qsTr("Refresh")
                enabled: !picker.actionBusy
                onClicked: picker.refreshRequested()
            }
        }

        ActionButton {
            Layout.fillWidth: true
            visible: picker.consentUrl.length > 0
            text: qsTr("Continue to secure provider approval")
            iconGlyph: Theme.iconLink
            prominent: true
            onClicked: picker.consentRequested()
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

                    delegate: RowLayout {
                        id: providerRow
                        required property string modelData
                        property var activeConnection: picker.connectionFor(modelData)
                        property bool isConnected: activeConnection !== null
                        property bool needsWeb3Proof: modelData === "farcaster"
                                                       || modelData === "ethereum-wallet"
                        property bool confirmUnlink: false
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: picker.providerTitle(modelData)
                                font.weight: Theme.weightDemiBold
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: providerRow.isConnected
                                      ? qsTr("Connected") + " · " +
                                        String(providerRow.activeConnection.displayName
                                               || providerRow.activeConnection.username || "")
                                      : providerRow.needsWeb3Proof
                                        ? qsTr("Requires a signed identity proof")
                                        : picker.authorized ? qsTr("Not connected")
                                                            : qsTr("Verify profile first")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        ActionButton {
                            compact: true
                            quiet: true
                            enabled: picker.authorized && !picker.actionBusy
                                     && (!isConnected && !needsWeb3Proof
                                         || isConnected && modelData !== "farcaster")
                            text: isConnected ? confirmUnlink ? qsTr("Confirm")
                                                                 : qsTr("Disconnect")
                                              : needsWeb3Proof ? qsTr("Verified proof") : qsTr("Connect")
                            onClicked: {
                                if (isConnected) {
                                    if (!confirmUnlink) {
                                        confirmUnlink = true
                                    } else {
                                        picker.disconnectRequested(modelData,
                                            String(activeConnection.subject || ""))
                                        confirmUnlink = false
                                    }
                                } else {
                                    picker.connectRequested(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: picker.errorText.length > 0
            text: picker.errorText
            color: Theme.danger
            wrapMode: Text.Wrap
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTr("Only OpenProof can confirm linked identities. Provider consent may open its secure authentication window.")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }
}
