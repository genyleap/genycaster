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
    property string manualLinkedInUrl: ""
    property bool catalogFallback: false
    property string approvalUrl: ""
    property string errorText: ""
    readonly property var proEntitlements: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool proEligible:
        proEntitlements.status === "verified" && proEntitlements.eligible === true

    signal authorizeRequested()
    signal approvalRequested(string url)
    signal refreshRequested()
    signal connectRequested(string provider)
    signal disconnectRequested(string provider, string subject)
    signal openProfileRequested(string url)
    signal manualLinkedInRequested(string url)
    signal proDetailsRequested()

    // Shared GenyDrawer handles bottom sheets on phones and consistently
    // right-anchored inspectors on desktop in both LTR and RTL.
    readonly property real viewportWidth: Overlay.overlay && Overlay.overlay.width > 0
                                          ? Overlay.overlay.width
                                          : (parent ? parent.width : 0)
    readonly property bool mobilePresentation: viewportWidth > 0
                                              && viewportWidth < Theme.breakpointMobile
    bottomSheet: mobilePresentation
    fullScreen: mobilePresentation
    dimBackground: true
    interactive: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    showHandle: false
    contentPadding: Theme.space4
    width: parent ? (mobilePresentation ? parent.width : Math.min(500, parent.width * 0.94)) : 500
    height: parent ? parent.height : 720

    readonly property string logoRoot: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/"

    function providerTitle(provider) {
        switch (provider) {
        case "x": return "X"
        case "github": return "GitHub"
        case "google": return "Google"
        case "linkedin": return "LinkedIn"
        case "telegram": return "Telegram"
        case "farcaster": return "Farcaster"
        case "ethereum-wallet": return qsTrId("genycaster.connections.copy_ethereum_wallet")
        default: return provider
        }
    }

    function providerDescription(provider) {
        switch (provider) {
        case "x": return qsTrId("genycaster.connections.copy_social_account")
        case "github": return qsTrId("genycaster.connections.copy_developer_profile")
        case "google": return qsTrId("genycaster.connections.copy_google_account")
        case "linkedin": return qsTrId("genycaster.connections.copy_professional_profile")
        case "telegram": return qsTrId("genycaster.connections.copy_messaging_account")
        case "farcaster": return qsTrId("genycaster.connections.copy_protocol_identity")
        case "ethereum-wallet": return qsTrId("genycaster.connections.copy_on_chain_identity")
        default: return qsTrId("genycaster.connections.copy_external_account")
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
        if (provider === "linkedin") {
            const official = String(connected.profileUrl || "")
            if (/^https:\/\/www\.linkedin\.com\/(?:in\/[A-Za-z0-9_-]{3,100}\/?|profile-thirdparty-redirect\/[-A-Za-z0-9_%.]{1,1024})$/.test(official))
                return official
            const manual = String(connected.manualProfileUrl || "")
            return /^https:\/\/www\.linkedin\.com\/in\/[A-Za-z0-9_-]{3,100}\/?$/.test(manual) ? manual : ""
        }
        if (provider === "x" && /^[A-Za-z0-9_]{1,15}$/.test(username))
            return "https://x.com/" + username
        if (provider === "github"
                && /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(username))
            return "https://github.com/" + username
        if (provider === "telegram" && /^[A-Za-z][A-Za-z0-9_]{4,31}$/.test(username))
            return "https://t.me/" + username
        return ""
    }

    // LinkedIn's standard OIDC response identifies a member but usually does
    // not include the public /in/ vanity slug. A name is not an identifier:
    // a people-search destination is deliberately distinct from a profile URL.
    function linkedinSearchFor(provider) {
        if (provider !== "linkedin" || !connectionFor(provider)) return ""
        const connection = connectionFor(provider)
        const name = String(connection.displayName || "").trim().slice(0, 128)
        if (!name) return ""
        return "https://www.linkedin.com/search/results/people/?keywords="
               + encodeURIComponent(name)
    }

    function destinationFor(provider) {
        return profileUrlFor(provider) || linkedinSearchFor(provider)
    }

    function connectionFor(provider) {
        for (let i = 0; i < connections.length; i++) {
            const connection = connections[i]
            if (String(connection.provider || "") === provider) return connection
        }
        return null
    }

    // The provider catalog is a list of available integrations, NOT the
    // account's connected networks. Keep linked networks first, with the
    // optional integrations visually separated on every screen size.
    readonly property var orderedProviders: {
        const linked = []
        const available = []
        const list = picker.providers || []
        for (let i = 0; i < list.length; ++i) {
            const provider = String(list[i])
            if (picker.connectionFor(provider)) linked.push(provider)
            else available.push(provider)
        }
        return linked.concat(available)
    }
    readonly property int linkedCount: {
        let count = 0
        const list = picker.providers || []
        for (let i = 0; i < list.length; ++i)
            if (picker.connectionFor(String(list[i]))) ++count
        return count
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.connections.title")
            subtitle: qsTrId("genycaster.connections.description")
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
                        text: qsTrId("genycaster.connections.copy_farcaster_verification")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: picker.authorizing
                          ? qsTrId("genycaster.connections.copy_confirm_this_farcaster_account_in_your_wallet_app_to_manage_connections")
                          : picker.cachedSnapshot
                            ? qsTrId("genycaster.connections.copy_these_links_are_saved_with_your_genycaster_account_verify_with_farcaster")
                            : qsTrId("genycaster.connections.copy_verify_ownership_with_farcaster_to_manage_your_connections_no_genyleap_a")
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
                        text: picker.authorizing ? qsTrId("genycaster.connections.copy_waiting_for_confirmation") : qsTrId("genycaster.connections.verify")
                        iconGlyph: picker.authorizing ? "" : Theme.iconArrowUpRight
                        onClicked: picker.authorizeRequested()
                    }
                    ActionButton {
                        visible: picker.authorizing && picker.approvalUrl.length > 0
                        compact: true
                        text: qsTrId("genycaster.connections.copy_open_farcaster_verification")
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
                text: qsTrId("genycaster.connections.connected_accounts")
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: LocaleFormat.formatInteger(picker.linkedCount)
                alignToUi: true
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
                    layoutDirection: Theme.uiDirection
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
                        text: qsTrId("genycaster.connections.copy_verified")
                        color: Theme.success
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }
            DirectionAwareLabel {
                visible: picker.cachedSnapshot && !picker.authorized && !picker.mobilePresentation
                text: qsTrId("genycaster.connections.copy_saved_account_links")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }
            ActionButton {
                visible: picker.cachedSnapshot && !picker.authorized && !picker.mobilePresentation
                compact: true
                text: picker.editAccessRequested ? qsTrId("genycaster.connections.copy_hide_verification") : qsTrId("genycaster.connections.copy_edit_connections")
                quiet: true
                onClicked: picker.editAccessRequested = !picker.editAccessRequested
            }
            ActionButton {
                visible: picker.authorized
                compact: true
                iconOnly: true
                quiet: true
                text: qsTrId("genycaster.connections.refresh")
                iconGlyph: Theme.iconRefresh
                enabled: !picker.actionBusy
                onClicked: picker.refreshRequested()
            }
        }

        RowLayout {
            visible: picker.mobilePresentation && picker.cachedSnapshot && !picker.authorized
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTrId("genycaster.connections.copy_saved_account_links")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
            ActionButton {
                compact: true
                text: picker.editAccessRequested ? qsTrId("genycaster.connections.copy_hide_verification") : qsTrId("genycaster.connections.copy_edit_connections")
                quiet: true
                onClicked: picker.editAccessRequested = !picker.editAccessRequested
            }
        }

        RowLayout {
            layoutDirection: Theme.uiDirection
            objectName: "connectionsProGatingBanner"
            visible: picker.authorized && !picker.proEligible
            Layout.fillWidth: true
            spacing: Theme.space2
            Icon {
                text: Theme.iconSpark
                preferredSize: Theme.iconSizeSmall
                color: Theme.proAccent
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.pro.connections_hint")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
            ActionButton {
                compact: true
                quiet: true
                text: qsTrId("genycaster.pro.check_status")
                onClicked: {
                    if (openProofConnections) openProofConnections.fetchProEntitlements(true)
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: picker.actionBusy && picker.pendingProvider.length > 0
            text: qsTrId("genycaster.connections.copy_connecting_1_confirm_in_the_provider_window_genycaster_will_update_this_")
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
                    width: Math.max(0, providerScroll.availableWidth)
                    spacing: 0

                    Repeater {
                        model: picker.orderedProviders

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

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                Layout.topMargin: Theme.space3
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                Layout.bottomMargin: Theme.space1
                                visible: !providerRow.isConnected
                                         && providerRow.index === picker.linkedCount
                                text: qsTrId("genycaster.connections.available_networks")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredHeight: Theme.scaleUi(66)
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space3

                                Rectangle {
                                    Layout.preferredWidth: Theme.scaleUi(42)
                                    Layout.preferredHeight: Theme.scaleUi(42)
                                    radius: Theme.curveCompact
                                    color: Theme.connectionIconBackground
                                    border.width: 1
                                    border.color: Theme.connectionIconBorder

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
                                    Layout.minimumWidth: 0
                                    spacing: Theme.space1
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: picker.providerTitle(providerRow.modelData)
                                        alignToUi: true
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
                                                           ? qsTrId("genycaster.connections.copy_saved_link_1").arg("\u2066" + nick + "\u2069")
                                                           : qsTrId("genycaster.connections.copy_saved_account_link")
                                                return nick.length > 0 ? nick : qsTrId("genycaster.connections.status_connected")
                                            }
                                            if (picker.authorized && !providerRow.redirectCapable)
                                                return qsTrId("genycaster.connections.copy_signed_proof_required")
                                            if (picker.authorized) return qsTrId("genycaster.connections.status_not_connected")
                                            return picker.providerDescription(providerRow.modelData)
                                        }
                                        alignToUi: true
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
                                                         ? qsTrId("genycaster.connections.copy_verified_account")
                                                         : qsTrId("genycaster.connections.copy_saved_account_link_verification_required_for_changes")
                                    }
                                    ActionButton {
                                        visible: providerRow.isConnected
                                                 && picker.destinationFor(providerRow.modelData).length > 0
                                        compact: true
                                        quiet: true
                                        iconOnly: picker.mobilePresentation
                                        iconGlyph: Theme.iconArrowUpRight
                                        text: picker.profileUrlFor(providerRow.modelData).length > 0
                                              ? qsTrId("genycaster.connections.copy_view") : qsTrId("genycaster.connections.copy_find")
                                        onClicked: picker.openProfileRequested(picker.destinationFor(providerRow.modelData))
                                    }
                                    ActionButton {
                                        visible: picker.authorized && providerRow.modelData === "linkedin"
                                                 && providerRow.isConnected
                                                 && picker.profileUrlFor("linkedin").length === 0
                                        compact: true
                                        quiet: true
                                        text: qsTrId("genycaster.connections.copy_reconnect")
                                        enabled: !picker.actionBusy
                                        onClicked: {
                                            if (picker.proEligible) picker.connectRequested("linkedin")
                                            else connectionsProNotice.open()
                                        }
                                    }
                                    ActionButton {
                                        visible: picker.authorized && providerRow.redirectCapable
                                        compact: true
                                        quiet: providerRow.isConnected && !providerRow.confirmUnlink
                                        destructive: providerRow.confirmUnlink
                                        elevated: false
                                        enabled: !picker.actionBusy
                                        text: providerRow.isConnected
                                              ? providerRow.confirmUnlink ? qsTrId("genycaster.connections.copy_confirm") : qsTrId("genycaster.connections.copy_remove")
                                              : picker.proEligible
                                                ? qsTrId("genycaster.connections.copy_connect")
                                                : qsTrId("genycaster.pro.requires_pro")
                                        onClicked: {
                                            if (providerRow.isConnected) {
                                                if (!providerRow.confirmUnlink) {
                                                    providerRow.confirmUnlink = true
                                                } else {
                                                    picker.disconnectRequested(providerRow.modelData,
                                                        String(providerRow.activeConnection.subject || ""))
                                                    providerRow.confirmUnlink = false
                                                }
                                            } else if (picker.proEligible) {
                                                picker.connectRequested(providerRow.modelData)
                                            } else {
                                                connectionsProNotice.open()
                                            }
                                        }
                                    }
                                    ActionButton {
                                        visible: providerRow.confirmUnlink
                                        compact: true
                                        quiet: true
                                        elevated: false
                                        text: qsTrId("genycaster.connections.copy_cancel")
                                        onClicked: providerRow.confirmUnlink = false
                                    }
                                    DirectionAwareLabel {
                                        visible: picker.authorized && !providerRow.redirectCapable
                                        text: providerRow.isConnected ? qsTrId("genycaster.connections.copy_linked") : qsTrId("genycaster.connections.copy_proof_only")
                                        color: providerRow.isConnected ? Theme.success : Theme.inkSubtle
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                            ColumnLayout {
                                visible: providerRow.modelData === "linkedin"
                                         && providerRow.isConnected && picker.authorized
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                Layout.bottomMargin: Theme.space3
                                spacing: Theme.space1

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.connections.copy_public_linkedin_profile_url")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                RowLayout {
                                    layoutDirection: Theme.uiDirection
                                    Layout.fillWidth: true
                                    spacing: Theme.space2
                                    DirectionAwareTextField {
                                        id: manualLinkedInField
                                        Layout.fillWidth: true
                                        implicitHeight: Theme.scaleUi(38)
                                        text: picker.manualLinkedInUrl
                                        placeholderText: "https://www.linkedin.com/in/username"
                                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                                        forceLeftToRight: true
                                        color: Theme.ink
                                        selectByMouse: true
                                        background: Rectangle {
                                            radius: Theme.curveCompact
                                            color: Theme.surface
                                            border.color: manualLinkedInField.activeFocus
                                                          ? Theme.accent : Theme.borderMuted
                                            border.width: 1
                                        }
                                    }
                                    ActionButton {
                                        compact: true
                                        text: qsTrId("genycaster.connections.copy_save")
                                        enabled: !picker.actionBusy && manualLinkedInField.text !== picker.manualLinkedInUrl
                                        onClicked: picker.manualLinkedInRequested(manualLinkedInField.text)
                                    }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.connections.copy_add_your_public_linkedin_profile_url")
                                    wrapMode: Text.Wrap
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }
                            DirectionAwareLabel {
                                visible: providerRow.modelData === "linkedin"
                                         && providerRow.isConnected
                                         && picker.profileUrlFor("linkedin").length === 0
                                         && picker.linkedinSearchFor("linkedin").length === 0
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.space3
                                Layout.rightMargin: Theme.space3
                                Layout.bottomMargin: Theme.space2
                                text: qsTrId("genycaster.connections.copy_linkedin_does_not_share_a_public_profile_url_through_standard_sign_in_fi")
                                wrapMode: Text.Wrap
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
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
                      ? qsTrId("genycaster.connections.copy_showing_saved_provider_list_refresh_when_online")
                      : qsTrId("genycaster.connections.copy_connected_accounts_share_your_secure_identity")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkSubtle
                wrapMode: Text.Wrap
            }
        }
    }
    ProUpgradeNotice {
        id: connectionsProNotice
        featureLabel: qsTrId("genycaster.pro.connections")
        onUpgradeRequested: picker.proDetailsRequested()
    }
}
