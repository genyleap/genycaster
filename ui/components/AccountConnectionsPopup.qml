import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

/** OpenProof is the authority for connection state; brand SVGs ship in Qt
 * resources, with no remote image dependencies or credentials in QML. */
GenyDrawer {
    id: picker
    objectName: "accountConnectionsPopup"
    property var providers: []
    property var connections: []
    property bool authorized: false
    property bool cachedSnapshot: false
    property bool editAccessRequested: false
    property bool authorizing: false
    property bool actionBusy: false
    property string pendingProvider: ""
    property string linkedInProfileUrl: ""
    property bool linkedInUrlError: false
    property bool catalogFallback: false
    property string approvalUrl: ""
    property string errorText: ""

    signal authorizeRequested()
    signal approvalRequested(string url)
    signal refreshRequested()
    signal connectRequested(string provider)
    signal disconnectRequested(string provider, string subject)
    signal saveLinkedInUrlRequested(string url)
    signal openProfileRequested(string url)

    // Connections is an in-product sheet: bottom-up on phones and
    // a right-anchored inspector on desktop, independent of UI text direction.
    readonly property real viewportWidth: Overlay.overlay && Overlay.overlay.width > 0
                                          ? Overlay.overlay.width
                                          : (parent ? parent.width : 0)
    readonly property bool mobilePresentation: viewportWidth > 0
                                              && viewportWidth < Theme.breakpointMobile
    bottomSheet: mobilePresentation
    edge: mobilePresentation ? Qt.BottomEdge : Qt.RightEdge
    dimBackground: true
    interactive: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    showHandle: mobilePresentation
    contentPadding: Theme.space4
    width: parent ? (mobilePresentation ? parent.width : Math.min(500, parent.width * 0.94)) : 500
    height: parent ? (mobilePresentation ? Math.min(parent.height * 0.92, 720) : parent.height) : 720

    onOpened: linkedInUrlError = false

    readonly property string logoRoot: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/"

    function providerTitle(provider) {
        switch (provider) {
        case "x": return "X"
        case "github": return "GitHub"
        case "google": return "Google"
        case "linkedin": return "LinkedIn"
        case "telegram": return "Telegram"
        case "farcaster": return "Farcaster"
        case "ethereum-wallet": return qsTr("Ethereum wallet")
        default: return provider
        }
    }

    function providerDescription(provider) {
        switch (provider) {
        case "x": return qsTr("Social account")
        case "github": return qsTr("Developer profile")
        case "google": return qsTr("Google account")
        case "linkedin": return qsTr("Professional profile")
        case "telegram": return qsTr("Messaging account")
        case "farcaster": return qsTr("Protocol identity")
        case "ethereum-wallet": return qsTr("On-chain identity")
        default: return qsTr("External account")
        }
    }

    function providerIcon(provider) {
        // Fixed, local allowlist. User-provided provider names never become paths.
        switch (provider) {
        case "x": return logoRoot + "x.svg"
        case "github": return logoRoot + "github.svg"
        case "google": return logoRoot + "google.svg"
        case "linkedin": return logoRoot + "linkedin.svg"
        case "telegram": return logoRoot + "telegram.svg"
        case "farcaster": return logoRoot + "farcaster.svg"
        case "ethereum-wallet": return logoRoot + "ethereum-wallet.svg"
        default: return ""
        }
    }

    function supportsRedirect(provider) {
        return provider === "x" || provider === "github" || provider === "google"
               || provider === "linkedin" || provider === "telegram"
    }

    function profileUrlFor(provider) {
        const connected = connectionFor(provider)
        if (!connected) return ""
        const username = String(connected.username || "").replace(/^@/, "")
        if (provider === "linkedin") return linkedInProfileUrl
        if (provider === "x" && /^[A-Za-z0-9_]{1,15}$/.test(username))
            return "https://x.com/" + username
        if (provider === "github"
                && /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(username))
            return "https://github.com/" + username
        if (provider === "telegram" && /^[A-Za-z][A-Za-z0-9_]{4,31}$/.test(username))
            return "https://t.me/" + username
        return ""
    }

    function connectionFor(provider) {
        for (let i = 0; i < connections.length; i++) {
            const connection = connections[i]
            if (String(connection.provider || "") === provider) return connection
        }
        return null
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

        ModalHeader {
            Layout.fillWidth: true
            title: qsTr("Connections")
            subtitle: qsTr("Manage connected accounts securely in Genycaster.")
            onCloseRequested: picker.close()
        }

        Rectangle {
            Layout.fillWidth: true
            visible: !picker.authorized && (!picker.cachedSnapshot || picker.editAccessRequested)
            radius: Theme.curveContainer
            color: Theme.surfaceMuted
            border.width: 1
            border.color: Theme.borderMuted
            implicitHeight: verifyContent.implicitHeight + Theme.space3 * 2

            ColumnLayout {
                id: verifyContent
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Icon {
                        text: Theme.iconLink
                        color: Theme.accent
                        preferredSize: 19
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTr("Farcaster verification")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: picker.authorizing
                          ? qsTr("Confirm this Farcaster account in your wallet app to manage connections.")
                          : picker.cachedSnapshot
                            ? qsTr("These are your last verified links. They survive refresh; confirm with Farcaster only to make changes.")
                            : qsTr("Verify ownership with Farcaster to manage your connections. No Genyleap account or password is required.")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    ActionButton {
                        Layout.fillWidth: true
                        compact: true
                        prominent: !picker.authorizing
                        quiet: picker.authorizing
                        enabled: !picker.authorizing
                        text: picker.authorizing ? qsTr("Waiting for confirmation…") : qsTr("Verify with Farcaster")
                        iconGlyph: picker.authorizing ? "" : Theme.iconArrowUpRight
                        onClicked: picker.authorizeRequested()
                    }
                    ActionButton {
                        visible: picker.authorizing && picker.approvalUrl.length > 0
                        compact: true
                        text: qsTr("Open Farcaster verification")
                        iconGlyph: Theme.iconArrowUpRight
                        onClicked: picker.approvalRequested(picker.approvalUrl)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            DirectionAwareLabel {
                text: qsTr("Networks")
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: String(picker.providers.length)
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                visible: picker.authorized
                radius: height / 2
                color: Theme.successSurface
                implicitHeight: 28
                implicitWidth: verifiedRow.implicitWidth + Theme.space3 * 2
                RowLayout {
                    id: verifiedRow
                    anchors.centerIn: parent
                    spacing: Theme.space1
                    Rectangle {
                        Layout.preferredWidth: 7
                        Layout.preferredHeight: 7
                        radius: 3.5
                        color: Theme.success
                    }
                    DirectionAwareLabel {
                        text: qsTr("Verified")
                        color: Theme.success
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }
            DirectionAwareLabel {
                visible: picker.cachedSnapshot && !picker.authorized
                text: qsTr("Last verified")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }
            ActionButton {
                visible: picker.cachedSnapshot && !picker.authorized
                compact: true
                text: picker.editAccessRequested ? qsTr("Hide verification") : qsTr("Edit connections")
                quiet: true
                onClicked: picker.editAccessRequested = !picker.editAccessRequested
            }
            ActionButton {
                visible: picker.authorized
                compact: true
                iconOnly: true
                quiet: true
                text: qsTr("Refresh connections")
                iconGlyph: Theme.iconRefresh
                enabled: !picker.actionBusy
                onClicked: picker.refreshRequested()
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: picker.actionBusy && picker.pendingProvider.length > 0
            text: qsTr("Connecting %1. Confirm in the provider window; Genycaster will update this list automatically.")
                  .arg(picker.providerTitle(picker.pendingProvider))
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.curveContainer
            color: Theme.surface
            border.color: Theme.borderMuted
            border.width: 1
            clip: true

            ScrollView {
                id: providerScroll
                anchors.fill: parent
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: providerScroll.availableWidth
                    spacing: 0

                    Repeater {
                        model: picker.providers

                        delegate: ColumnLayout {
                            id: providerRow
                            required property string modelData
                            required property int index
                            property var activeConnection: picker.connectionFor(modelData)
                            readonly property bool isConnected: activeConnection !== null
                            readonly property bool redirectCapable: picker.supportsRedirect(modelData)
                            property bool confirmUnlink: false
                            Layout.fillWidth: true
                            spacing: 0

                            Connections {
                                target: picker
                                function onClosed() { providerRow.confirmUnlink = false }
                            }
                            onIsConnectedChanged: confirmUnlink = false

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.scaleUi(66)
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space3

                                Rectangle {
                                    Layout.preferredWidth: Theme.scaleUi(42)
                                    Layout.preferredHeight: Theme.scaleUi(42)
                                    radius: Theme.curveCompact
                                    color: "#FFFFFF"
                                    border.width: 1
                                    border.color: "#E8EAEE"

                                    Image {
                                        anchors.centerIn: parent
                                        width: Math.min(parent.width - 16, Theme.scaleUi(26))
                                        height: width
                                        source: picker.providerIcon(providerRow.modelData)
                                        sourceSize.width: Math.ceil(width * 2)
                                        sourceSize.height: Math.ceil(height * 2)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        asynchronous: false
                                        Accessible.ignored: true
                                    }
                                    Icon {
                                        anchors.centerIn: parent
                                        visible: picker.providerIcon(providerRow.modelData).length === 0
                                        text: Theme.iconLink
                                        color: Theme.inkSubtle
                                        preferredSize: 20
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space1
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: picker.providerTitle(providerRow.modelData)
                                        color: Theme.ink
                                        font.weight: Theme.weightDemiBold
                                        font.pixelSize: Theme.typeLabelSize
                                        elide: Text.ElideRight
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: {
                                            const connection = providerRow.activeConnection
                                            if (connection && (picker.authorized || picker.cachedSnapshot)) {
                                                const nick = String(connection.displayName || connection.username || "")
                                                if (picker.cachedSnapshot && !picker.authorized)
                                                    return nick.length > 0
                                                           ? qsTr("Previously linked: %1").arg(nick)
                                                           : qsTr("Previously linked")
                                                return nick.length > 0 ? nick : qsTr("Connected")
                                            }
                                            if (picker.authorized && !providerRow.redirectCapable)
                                                return qsTr("Signed proof required")
                                            if (picker.authorized) return qsTr("Not connected")
                                            return picker.providerDescription(providerRow.modelData)
                                        }
                                        color: providerRow.isConnected && picker.authorized ? Theme.success : Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        elide: Text.ElideRight
                                    }
                                }

                                RowLayout {
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space1
                                    Icon {
                                        visible: providerRow.isConnected
                                        text: Theme.iconCheck
                                        color: picker.authorized ? Theme.success : Theme.inkSubtle
                                        preferredSize: 16
                                        Layout.preferredWidth: 20
                                        Layout.preferredHeight: 20
                                        Accessible.name: picker.authorized
                                                         ? qsTr("Verified account")
                                                         : qsTr("Previously verified account")
                                    }
                                    ActionButton {
                                        visible: providerRow.isConnected
                                                 && picker.profileUrlFor(providerRow.modelData).length > 0
                                        compact: true
                                        quiet: true
                                        iconGlyph: Theme.iconArrowUpRight
                                        text: qsTr("View")
                                        onClicked: picker.openProfileRequested(picker.profileUrlFor(providerRow.modelData))
                                    }
                                    ActionButton {
                                        visible: picker.authorized && providerRow.redirectCapable
                                        compact: true
                                        quiet: providerRow.isConnected && !providerRow.confirmUnlink
                                        destructive: providerRow.confirmUnlink
                                        elevated: false
                                        enabled: !picker.actionBusy
                                        text: providerRow.isConnected
                                              ? providerRow.confirmUnlink ? qsTr("Confirm") : qsTr("Remove")
                                              : qsTr("Connect")
                                        onClicked: {
                                            if (providerRow.isConnected) {
                                                if (!providerRow.confirmUnlink) {
                                                    providerRow.confirmUnlink = true
                                                } else {
                                                    picker.disconnectRequested(providerRow.modelData,
                                                        String(providerRow.activeConnection.subject || ""))
                                                    providerRow.confirmUnlink = false
                                                }
                                            } else {
                                                picker.connectRequested(providerRow.modelData)
                                            }
                                        }
                                    }
                                    ActionButton {
                                        visible: providerRow.confirmUnlink
                                        compact: true
                                        quiet: true
                                        elevated: false
                                        text: qsTr("Cancel")
                                        onClicked: providerRow.confirmUnlink = false
                                    }
                                    DirectionAwareLabel {
                                        visible: picker.authorized && !providerRow.redirectCapable
                                        text: providerRow.isConnected ? qsTr("Linked") : qsTr("Proof only")
                                        color: providerRow.isConnected ? Theme.success : Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                Layout.bottomMargin: Theme.space3
                                visible: providerRow.modelData === "linkedin" && providerRow.isConnected
                                spacing: Theme.space2
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTr("Public LinkedIn profile URL")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space2
                                    DirectionAwareTextField {
                                        id: linkedinUrlField
                                        Layout.fillWidth: true
                                        forceLeftToRight: true
                                        inputMethodHints: Qt.ImhUrlCharactersOnly
                                        placeholderText: "https://www.linkedin.com/in/your-name"
                                        text: picker.linkedInProfileUrl
                                        selectByMouse: true
                                        onTextEdited: picker.linkedInUrlError = false
                                        onAccepted: picker.saveLinkedInUrlRequested(text)
                                    }
                                    ActionButton {
                                        compact: true
                                        prominent: linkedinUrlField.text.trim().length > 0
                                        text: qsTr("Save")
                                        onClicked: picker.saveLinkedInUrlRequested(linkedinUrlField.text)
                                    }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: picker.linkedInUrlError
                                          ? qsTr("Enter a valid https://www.linkedin.com/in/... address.")
                                          : qsTr("LinkedIn confirmed your account, but this public URL is provided by you.")
                                    color: picker.linkedInUrlError ? Theme.danger : Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                Layout.preferredHeight: 1
                                visible: index < picker.providers.length - 1
                                color: Theme.borderMuted
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
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Icon {
                text: Theme.iconLink
                color: Theme.inkSubtle
                preferredSize: 15
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: picker.catalogFallback
                      ? qsTr("Showing saved provider list. Refresh when online.")
                      : qsTr("Connected accounts share your secure identity.")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkSubtle
                wrapMode: Text.Wrap
            }
        }
    }
}
