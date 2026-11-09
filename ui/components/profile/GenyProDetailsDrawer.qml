import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Product-owned Pro explainer. It never upgrades an account locally:
// membership and wallet balances are verified by the account service.
GenyDrawer {
    id: root
    objectName: "genyProDetailsDrawer"
    property bool compact: false
    signal accountRequested()
    // A signed-in Farcaster/OpenProof application identity is distinct from
    // the independently verified account-server payment session.
    readonly property bool appSignedIn: presentationBridge
        && presentationBridge.isAuthenticated
        && openProofConnections
        && Number(presentationBridge.accountFid || 0) > 0
        && Number(presentationBridge.accountFid)
            === Number(openProofConnections.activeAccountFid || 0)
    readonly property bool restoringSession: openProofConnections
        && openProofConnections.cloudSessionBusy
    readonly property bool verifyingIdentity: openProofConnections
        && openProofConnections.authorizing
    property bool startedVerification: false
    readonly property var plan: openProofConnections
        ? (openProofConnections.proPlan || ({})) : ({})
    readonly property bool cloudReady: openProofConnections
        && openProofConnections.cloudAuthorizationReady
        && Number(openProofConnections.activeAccountFid || 0) > 0
    readonly property bool canRequestQuote: cloudReady
        && openProofConnections && !openProofConnections.proPlanBusy
        && monthlyUsdCents > 0 && plan.payment_available === true
    readonly property var membership: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool eligible: membership.status === "verified"
                                     && membership.tier === "pro"
                                     && membership.eligible === true
    readonly property var paymentQuote: openProofConnections
        ? (openProofConnections.proPaymentQuote || ({})) : ({})
    readonly property int monthlyUsdCents: Number(plan.price_usd_cents || 0)
    readonly property string monthlyUsd: monthlyUsdCents > 0
        ? (monthlyUsdCents / 100).toFixed(2) : "—"
    property string paymentHash: ""
    property double quoteNowMs: Date.now()
    readonly property bool quoteValid: Boolean(paymentQuote.id)
        && new Date(paymentQuote.expires_at).getTime() > quoteNowMs
    property int expandedFeature: -1
    Timer {
        interval: 1000
        repeat: true
        running: root.opened && Boolean(root.paymentQuote.id)
        onTriggered: root.quoteNowMs = Date.now()
    }

    bottomSheet: root.compact
    showHandle: root.compact
    dimBackground: true
    width: root.compact ? parent.width : Math.min(440,parent.width*0.94)
    height: root.compact ? Math.min(parent.height*0.88,760) : parent.height
    contentPadding: Theme.space4

    Component.onCompleted: {
        if (openProofConnections) openProofConnections.fetchProPlan(false)
    }

    onOpened: {
        expandedFeature = -1
        quoteNowMs = Date.now()
        detailsViewport.contentY = 0
        if (openProofConnections) {
            openProofConnections.fetchProPlan(true)
            openProofConnections.retryProEntitlements()
        }
    }

    onClosed: {
        // Only cancel a proof initiated from this drawer; a verification
        // already started in account connections belongs to that workflow.
        if (startedVerification && openProofConnections
            && openProofConnections.authorizing)
            openProofConnections.stopAuthorization()
        startedVerification = false
    }

    Connections {
        target: openProofConnections
        function onStateChanged() {
            if (root.cloudReady)
                root.startedVerification = false
            if (root.opened && root.cloudReady && !openProofConnections.proEntitlementsBusy)
                openProofConnections.fetchProEntitlements(false)
        }
    }

    contentItem: Item {
        id: viewport
        clip: true

        RowLayout {
            id: fixedHeader
            objectName: "genyProDrawerFixedHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Theme.scaleUi(56)
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                visible: !root.compact
                Layout.preferredWidth: visible ? 40 : 0
                Layout.preferredHeight: visible ? 40 : 0
                curveDepth: Theme.curveCompact
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconSpark
                    color: Theme.proAccent
                    preferredSize: Theme.iconSizeLarge
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    horizontalAlignment: root.compact ? Text.AlignHCenter : Theme.textStartAlignment
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.eligible
                          ? qsTrId("genycaster.pro.status_verified")
                          : !root.appSignedIn
                            ? qsTrId("genycaster.pro.requires_pro")
                            : !root.cloudReady
                              ? qsTrId("genycaster.pro.payment_identity_pending")
                              : root.membership.status === "unavailable"
                                ? qsTrId("genycaster.pro.status_unavailable")
                                : qsTrId("genycaster.pro.requires_pro")
                    font.pixelSize: Theme.typeCaptionSize
                    color: root.eligible ? Theme.positive : Theme.textSecondary
                    horizontalAlignment: root.compact ? Text.AlignHCenter : Theme.textStartAlignment
                    elide: Text.ElideRight
                }
            }
            ActionButton {
                visible: !root.compact
                Layout.alignment: Qt.AlignVCenter
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.pro.not_now")
                onClicked: root.close()
            }
        }

        Flickable {
            id: detailsViewport
            objectName: "genyProDrawerScrollableDetails"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: fixedHeader.bottom
            anchors.bottom: fixedActions.top
            anchors.topMargin: Theme.space2
            anchors.bottomMargin: Theme.space2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: detailsColumn.implicitHeight + Theme.space3
            interactive: contentHeight > height
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: detailsColumn
                width: detailsViewport.width
                spacing: Theme.space2
                // A server-priced subscription, not a token-balance threshold.
                Superellipse {
                    objectName: "genyProNeutralEligibilitySummary"
                    Layout.fillWidth: true
                    implicitHeight: summaryColumn.implicitHeight + Theme.space4 * 2
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer
                    ColumnLayout {
                        id: summaryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space4
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.monthlyUsdCents > 0
                                ? qsTrId("genycaster.pro.monthly_price").arg(root.monthlyUsd)
                                : openProofConnections && openProofConnections.proPlanBusy
                                  ? qsTrId("genycaster.pro.price_loading")
                                  : qsTrId("genycaster.pro.price_unavailable")
                            font.pixelSize: Theme.typeTitle1Size
                            font.weight: Theme.weightBold
                            color: Theme.textPrimary
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.manual_renewal")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: root.appSignedIn && !root.cloudReady
                            text: root.restoringSession
                                ? qsTrId("genycaster.pro.restoring_payment_session")
                                : root.verifyingIdentity
                                  ? qsTrId("genycaster.pro.approve_payment_identity")
                                  : qsTrId("genycaster.pro.verify_payment_identity")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            visible: root.appSignedIn && !root.cloudReady
                                && root.verifyingIdentity
                                && openProofConnections
                                && String(openProofConnections.approvalUrl || "").length > 0
                            compact: true
                            text: qsTrId("genycaster.connections.copy_open_farcaster_verification")
                            iconGlyph: Theme.iconArrowUpRight
                            onClicked: Qt.openUrlExternally(openProofConnections.approvalUrl)
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: root.appSignedIn && root.startedVerification
                                && !root.cloudReady && !root.verifyingIdentity
                                && openProofConnections
                                && String(openProofConnections.error || "").length > 0
                            text: openProofConnections ? openProofConnections.error : ""
                            color: Theme.negative
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: root.eligible && Boolean(root.membership.expires_at)
                            text: qsTrId("genycaster.pro.active_until").arg(
                                LocaleFormat.formatDateTime(new Date(root.membership.expires_at).getTime()))
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            color: Theme.positive
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: root.membership.status === "unavailable"
                            text: qsTrId("genycaster.pro.verification_unavailable")
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                }

                // Quote identity and amount come only from the signed-in
                // GenyCaster account API. No client-side GENY/USD calculations.
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Boolean(root.paymentQuote.id)
                    spacing: Theme.space2

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.quote_label")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(root.paymentQuote.amount_geny || "") + " GENY"
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                        color: Theme.textPrimary
                        forceLeftToRight: true
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.payer_wallet").arg(
                            String((root.paymentQuote.verified_payers || [])[0] || ""))
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.textSecondary
                        wrapMode: Text.WrapAnywhere
                        forceLeftToRight: true
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.receiving_address")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(root.paymentQuote.treasury || "")
                        color: Theme.textPrimary
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        ActionButton {
                            Layout.fillWidth: true
                            compact: true
                            quiet: true
                            text: qsTrId("genycaster.pro.copy_amount")
                            onClicked: presentationBridge.copyText(
                                String(root.paymentQuote.amount_geny || ""))
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            compact: true
                            quiet: true
                            text: qsTrId("genycaster.pro.copy_recipient")
                            onClicked: presentationBridge.copyText(
                                String(root.paymentQuote.treasury || ""))
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.quote_expiry").arg(
                            LocaleFormat.formatDateTime(new Date(root.paymentQuote.expires_at).getTime()))
                        color: root.quoteValid ? Theme.textSecondary : Theme.negative
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.payment_instructions")
                        color: Theme.textSecondary
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    DirectionAwareTextField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        enabled: root.quoteValid
                        maximumLength: 66
                        placeholderText: qsTrId("genycaster.pro.transaction_hash")
                        text: root.paymentHash
                        onTextChanged: root.paymentHash = text
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        compact: true
                        prominent: true
                        busy: openProofConnections ? openProofConnections.proBillingBusy : false
                        enabled: root.quoteValid && !busy
                            && /^0x[0-9a-fA-F]{64}$/.test(root.paymentHash.trim())
                        text: qsTrId("genycaster.pro.verify_payment")
                        onClicked: openProofConnections.verifyProPayment(root.paymentHash.trim())
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: openProofConnections && String(openProofConnections.proBillingError || "").length > 0
                    text: openProofConnections && openProofConnections.proBillingError === "payment_pending"
                        ? qsTrId("genycaster.pro.payment_pending")
                        : qsTrId("genycaster.pro.billing_error")
                    color: Theme.negative
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.monthlyUsdCents > 0
                        && root.plan.payment_available === false
                    text: qsTrId("genycaster.pro.payment_unavailable")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }

                Repeater {
                    model: [
                        { icon: Theme.iconApps,
                          title: qsTrId("genycaster.pro.feature_miniapps_title"),
                          detail: qsTrId("genycaster.pro.feature_miniapps_desc") },
                        { icon: Theme.iconBookmark,
                          title: qsTrId("genycaster.pro.bookmark_collections"),
                          detail: qsTrId("genycaster.pro.feature_collections_desc") },
                        { icon: Theme.iconLink,
                          title: qsTrId("genycaster.pro.connections"),
                          detail: qsTrId("genycaster.pro.feature_connections_desc") },
                        { icon: Theme.iconChart,
                          title: qsTrId("genycaster.pro.market_pulse"),
                          detail: qsTrId("genycaster.pro.feature_market_desc") },
                        { icon: Theme.iconCompose,
                          title: qsTrId("genycaster.pro.feature_composer_title"),
                          detail: qsTrId("genycaster.pro.feature_composer_desc") }
                    ]
                    delegate: Button {
                        id: feature
                        required property var modelData
                        required property int index
                        readonly property bool expanded: root.expandedFeature === index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: Math.max(48, featureLayout.implicitHeight + Theme.space2 * 2)
                        leftPadding: Theme.space3
                        rightPadding: Theme.space3
                        topPadding: Theme.space2
                        bottomPadding: Theme.space2
                        flat: true
                        hoverEnabled: !root.compact
                        Accessible.name: String(modelData.title || "")
                        Accessible.description: String(modelData.detail || "")
                        onClicked: root.expandedFeature = expanded ? -1 : index

                        background: Superellipse {
                            fillColor: feature.expanded ? Theme.surfaceMuted
                                : feature.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            borderColor: Theme.borderSubtle
                            borderWidth: Theme.strokeHairline
                            curveDepth: Theme.curveCompact
                            Behavior on fillColor {
                                enabled: !Theme.reduceMotion
                                ColorAnimation { duration: Theme.motionFast }
                            }
                        }
                        contentItem: RowLayout {
                            id: featureLayout
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Superellipse {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                Layout.alignment: Qt.AlignTop
                                fillColor: Theme.surfaceMuted
                                borderWidth: 0
                                curveDepth: Theme.curveCompact
                                Icon {
                                    anchors.centerIn: parent
                                    text: String(feature.modelData.icon || "")
                                    color: Theme.inkMuted
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.title || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                    wrapMode: feature.expanded ? Text.WordWrap : Text.NoWrap
                                    maximumLineCount: feature.expanded ? 2 : 1
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    visible: feature.expanded
                                    Layout.fillWidth: true
                                    text: String(feature.modelData.detail || "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            Icon {
                                Layout.alignment: Qt.AlignTop
                                text: feature.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: fixedActions.top
            height: 1
            color: Theme.divider
        }
        RowLayout {
            id: fixedActions
            objectName: "genyProDrawerFixedActions"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(56,implicitHeight)
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                text: qsTrId("genycaster.pro.not_now")
                onClicked: root.close()
            }
            ActionButton {
                Layout.fillWidth: true
                compact: true
                prominent: true
                iconGlyph: Theme.iconWallet
                spinIconWhileBusy: true
                busy: openProofConnections ? openProofConnections.proBillingBusy : false
                enabled: !busy
                    && !(openProofConnections && openProofConnections.proPlanBusy)
                    && !(root.appSignedIn && (root.restoringSession
                                               || root.verifyingIdentity))
                    && (!root.cloudReady || root.monthlyUsdCents <= 0
                        || root.canRequestQuote)
                text: root.monthlyUsdCents <= 0
                    ? qsTrId("genycaster.pro.refresh_price")
                    : !root.appSignedIn
                      ? qsTrId("genycaster.pro.connect_to_pay")
                      : root.restoringSession
                        ? qsTrId("genycaster.pro.restoring_payment_session")
                        : root.verifyingIdentity
                          ? qsTrId("genycaster.connections.copy_waiting_for_confirmation")
                          : !root.cloudReady
                            ? qsTrId("genycaster.pro.verify_with_farcaster")
                            : qsTrId("genycaster.pro.get_quote")
                onClicked: {
                    if (!openProofConnections) return
                    if (root.monthlyUsdCents <= 0) {
                        openProofConnections.fetchProPlan(true)
                    } else if (!root.appSignedIn) {
                        root.accountRequested()
                    } else if (!root.cloudReady
                               && !root.verifyingIdentity && !root.restoringSession) {
                        // This proves control of the already signed-in FID.
                        // Never ask the user to create or log into a second
                        // identity, and never grant Pro from local state.
                        root.startedVerification = true
                        openProofConnections.authorizeAccount()
                    } else if (root.canRequestQuote) {
                        openProofConnections.requestProPaymentQuote()
                    }
                }
            }
        }
    }
}
