import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// A two-stage first-party purchase sheet. The server signs neither UI state nor
// an amount supplied by the client: only its owner-scoped quote is payable.
GenyDrawer {
    id: root
    objectName: "genyProDetailsDrawer"
    property bool compact: false
    signal accountRequested()
    signal signerRequested()
    property string selectedAsset: "GENY"
    property bool checkoutOpen: false
    // Opening membership details is read-only; never initiate a payment
    // quote or a secure-session challenge until the user requests checkout.
    property bool purchaseIntent: false
    // An old verification failure must not be shown as a new quote failure.
    property bool quoteAttempted: false
    property bool walletAttempted: false
    property string displayedQuoteId: ""
    property bool showBenefits: false
    property bool manualExpanded: false
    property string paymentHash: ""
    property string lastRequestedAsset: ""
    readonly property bool quoteForSelection: quotePresent && quoteAsset === selectedAsset && quoteValid
    property int expandedFeature: -1
    property double quoteNowMs: Date.now()

    readonly property var plan: openProofConnections
        ? (openProofConnections.proPlan || ({})) : ({})
    readonly property var membership: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property var paymentQuote: openProofConnections
        ? (openProofConnections.proPaymentQuote || ({})) : ({})
    readonly property bool membershipVerified: membership.status === "verified"
    readonly property string proofError: openProofConnections
        ? String(openProofConnections.accountProofError || "") : ""
    readonly property bool signerAuthorizationNeeded: !membershipVerified && appSignedIn
        && (proofError === "account_signer_unavailable"
            || proofError === "account_proof_not_accepted"
            || (presentationBridge.nativeCustodyAccount
                && !presentationBridge.writeReady && !presentationBridge.signerBusy
                && (presentationBridge.signerPhase === "missing"
                    || presentationBridge.signerPhase === "authorization-required"
                    || presentationBridge.signerPhase === "expired"
                    || presentationBridge.signerPhase === "revoked")))
    readonly property bool eligible: membershipVerified
        && membership.tier === "pro" && membership.eligible === true
    readonly property bool appSignedIn: presentationBridge
        && presentationBridge.isAuthenticated && openProofConnections
        && Number(presentationBridge.accountFid || 0) > 0
        && Number(presentationBridge.accountFid)
            === Number(openProofConnections.activeAccountFid || 0)
    readonly property bool cloudReady: openProofConnections
        && openProofConnections.cloudAuthorizationReady
        && Number(openProofConnections.activeAccountFid || 0) > 0
    readonly property bool restoringSession: openProofConnections
        && (openProofConnections.cloudSessionBusy
            || openProofConnections.accountProofBusy)
    readonly property int monthlyUsdCents: Number(plan.price_usd_cents || 0)
    // A successful, server-validated public plan is the only pricing source.
    // Missing pricing must not make a payment method look actionable.
    readonly property bool priceKnown: monthlyUsdCents > 0
    readonly property bool priceLoading: !priceKnown && openProofConnections
        && openProofConnections.proPlanBusy
    readonly property bool priceUnavailable: !priceKnown && !priceLoading
    readonly property bool paymentOptionsReady: priceKnown
        && plan.payment_available === true && cloudReady && membershipVerified
        && !eligible
    readonly property string monthlyUsd: monthlyUsdCents > 0
        ? (monthlyUsdCents / 100).toFixed(2) : "—"
    readonly property bool quotePresent: Boolean(paymentQuote.id)
    readonly property bool checkoutActive: checkoutOpen && quotePresent && !eligible
    readonly property string quoteAsset: String(paymentQuote.asset || "GENY")
    readonly property bool quoteValid: quotePresent && remainingSeconds > 45
    readonly property int remainingSeconds: Math.max(0, Math.ceil(
        (new Date(paymentQuote.expires_at).getTime() - quoteNowMs) / 1000) || 0)
    readonly property bool txSubmitted: proCheckout
        && String(proCheckout.txHash || "").length === 66
        && String(proCheckout.quoteId || "") === String(paymentQuote.id || "")
    readonly property bool validPaymentHash: /^0x[0-9a-fA-F]{64}$/.test(paymentHash.trim())
    readonly property bool canRequestQuote: membershipVerified && !eligible
        && cloudReady && openProofConnections && !openProofConnections.proPlanBusy
        && monthlyUsdCents > 0 && plan.payment_available === true
    readonly property bool busy: (openProofConnections && (openProofConnections.proBillingBusy
            || openProofConnections.proWalletBusy))
        || (proCheckout && proCheckout.busy)
    readonly property string amountText: String(paymentQuote.amount_geny || "")
        + " " + quoteAsset
    readonly property bool walletAvailable: proCheckout && proCheckout.walletAvailable
    readonly property bool walletConnected: proCheckout && proCheckout.walletConnected
    readonly property bool walletVerified: openProofConnections
        && openProofConnections.proWalletVerified

    function shortWallet(value) {
        const address = String(value || "")
        return address.length === 42 ? address.slice(0, 8) + "…" + address.slice(-6) : address
    }
    function timeRemaining(seconds) {
        const time = Math.max(0, Math.floor(seconds))
        return Math.floor(time / 60) + ":" + String(time % 60).padStart(2, "0")
    }
    function walletErrorMessage() {
        if (!proCheckout || !proCheckout.error) return ""
        const code = String(proCheckout.error)
        if (code === "wallet_rejected") return qsTrId("genycaster.pro.wallet_cancelled")
        if (code === "wallet_not_verified_for_account")
            return qsTrId("genycaster.pro.wallet_mismatch")
        if (code === "wallet_switch_base_required" || code === "wallet_wrong_chain")
            return qsTrId("genycaster.pro.wallet_base_required")
        if (code === "invalid_payment_quote")
            return qsTrId("genycaster.pro.quote_expired")
        return qsTrId("genycaster.pro.wallet_failure")
    }
    function requestCurrentQuote() {
        if (!root.canRequestQuote || root.busy
            || (root.walletAvailable && (!root.walletConnected || !root.walletVerified))) return
        root.quoteAttempted = true
        root.lastRequestedAsset = root.selectedAsset
        openProofConnections.requestProPaymentQuote(root.selectedAsset)
    }
    function openQuotedCheckout() {
        // The quote response advances the UI, never initiates wallet payment.
        // A separate, explicit click is required to authorize a transaction.
        if (!root.opened || !root.purchaseIntent || root.checkoutOpen
            || !root.quoteForSelection || root.eligible) return
        root.checkoutOpen = true
        root.purchaseIntent = false
        root.displayedQuoteId = String(root.paymentQuote.id)
        root.manualExpanded = !root.walletAvailable
        detailsViewport.contentY = 0
    }
    function prepareQuote() {
        if (!root.opened || !root.purchaseIntent || root.checkoutOpen || root.eligible
            || !root.canRequestQuote || root.busy || root.quoteForSelection
            || (root.walletAvailable && (!root.walletConnected || !root.walletVerified))
            || root.lastRequestedAsset === root.selectedAsset) return
        root.requestCurrentQuote()
    }
    function cancelPayment() {
        // Cancel the local session; any already submitted on-chain
        // transaction continues to require independent verification.
        root.checkoutOpen = false
        root.purchaseIntent = false
        root.manualExpanded = false
        root.paymentHash = ""
        // Dismiss the overlay before a wallet transport invokes callbacks;
        // on Windows a synchronous native wallet cancellation may otherwise
        // keep a modal Drawer visible while its event loop is occupied.
        root.close()
    }
    function proceed() {
        if (root.eligible) { root.close(); return }
        if (!root.appSignedIn) { root.accountRequested(); return }
        // Unknown entitlements are not a Basic subscription. Recheck the
        // signed-in account before ever presenting a purchase flow.
        if (!root.membershipVerified) {
            if (root.signerAuthorizationNeeded) {
                root.signerRequested()
                return
            }
            if (!root.restoringSession && openProofConnections) {
                if (root.cloudReady)
                    openProofConnections.fetchProEntitlements(true)
                else
                    openProofConnections.retryProEntitlements()
            }
            return
        }
        if (root.monthlyUsdCents <= 0) {
            if (openProofConnections) openProofConnections.fetchProPlan(true)
            return
        }
        if (!root.cloudReady) {
            // Retry secure account-session restoration. A native vault signer
            // or the previously registered browser signer proves the FID
            // without opening the external Farcaster authorization flow.
            if (!root.restoringSession)
                openProofConnections.retryProEntitlements()
            return
        }
        if (!root.checkoutActive) {
            root.purchaseIntent = true
            if (!root.quoteForSelection) {
                // First user gesture opens the wallet selector (native) or
                // EIP-1193 account picker (WASM). The server quote follows
                // connection; payment itself requires a second explicit click.
                if (root.walletAvailable) {
                    root.walletAttempted = true
                    if (!root.walletConnected) {
                        proCheckout.connectWallet()
                        return
                    }
                    if (!root.walletVerified) {
                        openProofConnections.requestProWalletChallenge(proCheckout.payerAddress)
                        return
                    }
                }
                root.requestCurrentQuote()
                return
            }
            root.openQuotedCheckout()
            return
        }
        if (root.txSubmitted) {
            if (!root.busy && openProofConnections)
                openProofConnections.verifyProPayment(proCheckout.txHash)
            return
        }
        if (!root.quoteValid) { root.requestCurrentQuote(); return }
        if (root.manualExpanded || !root.walletAvailable) {
            if (root.validPaymentHash && !root.busy)
                openProofConnections.verifyProPayment(root.paymentHash.trim())
            return
        }
        if (!root.busy) proCheckout.pay()
    }
    function primaryLabel() {
        if (eligible) return qsTrId("genycaster.pro.checkout_done")
        if (!appSignedIn) return qsTrId("genycaster.pro.connect_to_pay")
        if (restoringSession) return qsTrId("genycaster.pro.check_status")
        if (openProofConnections && openProofConnections.proWalletBusy
            || proCheckout && proCheckout.phase === "signing_wallet_proof")
            return qsTrId("genycaster.pro.checkout_verifying_wallet")
        if (signerAuthorizationNeeded) return qsTrId("genycaster.shell.enable_interactions")
        if (!membershipVerified) return qsTrId("genycaster.pro.check_status")
        if (monthlyUsdCents <= 0) return qsTrId("genycaster.pro.refresh_price")
        if (!cloudReady) return qsTrId("genycaster.pro.restore_secure_session")
        if (!checkoutActive && plan.payment_available !== true)
            return qsTrId("genycaster.pro.payment_unavailable")
        if (!checkoutActive && walletAvailable && !walletConnected)
            return qsTrId("genycaster.pro.checkout_open_wallet")
        if (!checkoutActive && walletAvailable && !walletVerified)
            return qsTrId("genycaster.pro.checkout_verify_wallet")
        if (!checkoutActive) return qsTrId("genycaster.pro.checkout_continue")
        if (txSubmitted) return qsTrId("genycaster.pro.checkout_check_payment")
        if (!quoteValid) return qsTrId("genycaster.pro.checkout_new_quote")
        if (manualExpanded || !walletAvailable) return qsTrId("genycaster.pro.verify_payment")
        return qsTrId("genycaster.pro.checkout_open_wallet")
    }

    // Mobile checkout fills the screen; a short sheet hides benefits and
    // makes the payment steps appear clipped above a fixed footer.
    bottomSheet: root.compact
    // Short sheets hug their content; large checkout forms can expand to the
    // available viewport, with a scrolling body above the pinned actions.
    fullScreen: root.compact && root.manualExpanded && root.checkoutActive
    showHandle: root.compact && !root.fullScreen
    dimBackground: true
    // A full-height payment form scrolls within its own Flickable. Disable
    // Qt Drawer edge-gesture grabs so native desktop clicks (especially the
    // Close/Cancel buttons) cannot be consumed by the drag recognizer.
    // The visible mobile handle and backdrop retain dismissal paths.
    interactive: false
    // Billing always opens from the physical right on desktop, regardless
    // of interface language. Other GenyDrawer instances keep their policy.
    edge: root.compact ? Qt.BottomEdge : Qt.RightEdge
    width: parent ? (root.compact ? parent.width
        : Math.min(500, Math.max(360, parent.width * 0.40))) : 468
    // A Qt Drawer is edge anchored: its height MUST match the parent height
    // on desktop. A shorter height leaves it top-aligned, visually broken,
    // and outside of the edge-to-edge drawer contract. Sparse Pro content
    // stays at the top; scroll controls and footer remain correctly pinned.
    height: parent ? (root.compact && !root.fullScreen
        ? Math.min(parent.height, Math.max(Theme.scaleUi(380),
            fixedHeader.height + detailsColumn.implicitHeight + fixedActions.height
            + Theme.space4 * 5))
        : parent.height) : 640
    contentPadding: root.compact ? Theme.space3 : Theme.space4

    Timer {
        interval: 1000
        repeat: true
        running: root.opened && root.checkoutActive && root.quotePresent
        onTriggered: root.quoteNowMs = Date.now()
    }

    Component.onCompleted: {
        if (openProofConnections) openProofConnections.fetchProPlan(false)
    }
    onOpened: {
        purchaseIntent = false
        quoteAttempted = false
        walletAttempted = false
        checkoutOpen = false
        lastRequestedAsset = ""
        manualExpanded = false
        showBenefits = false
        expandedFeature = -1
        quoteNowMs = Date.now()
        detailsViewport.contentY = 0
        if (openProofConnections) {
            // Opening a UI surface does not force repeated billing/status
            // requests. Membership is fetched once from the account cache.
            openProofConnections.fetchProPlan(false)
            // The membership view never silently requests a signer proof or
            // begins a payment session. Restore/check is an explicit action.
            if (root.cloudReady)
                openProofConnections.fetchProEntitlements(false)
        }
    }
    onClosed: {
        root.purchaseIntent = false
        if (!root.txSubmitted && proCheckout
            && (proCheckout.busy || proCheckout.walletConnected))
            Qt.callLater(() => proCheckout.cancel())
    }
    onCanRequestQuoteChanged: root.prepareQuote()
    onBusyChanged: {
        if (!root.busy && root.purchaseIntent && root.walletAvailable
            && !root.walletConnected && proCheckout
            && String(proCheckout.error || "").length > 0) {
            root.purchaseIntent = false
            root.lastRequestedAsset = ""
        } else if (!root.busy && root.purchaseIntent && openProofConnections
            && String(openProofConnections.proWalletError || "").length > 0) {
            root.purchaseIntent = false
            root.lastRequestedAsset = ""
        } else if (!root.busy && root.purchaseIntent && root.quoteAttempted
            && root.lastRequestedAsset === root.selectedAsset
            && !root.quoteForSelection && openProofConnections
            && String(openProofConnections.proBillingError || "").length > 0) {
            // A failed request must not remain latched as an attempted asset.
            // Do not auto-retry a failed payment quote.
            root.purchaseIntent = false
            root.lastRequestedAsset = ""
        } else {
            root.prepareQuote()
        }
    }
    onSelectedAssetChanged: {
        root.lastRequestedAsset = ""
        root.prepareQuote()
    }
    onPaymentQuoteChanged: {
        if (root.opened && root.quotePresent) {
            root.quoteNowMs = Date.now()
            // A network response cannot automatically initiate wallet payment.
            if (root.checkoutOpen
                && String(root.paymentQuote.id) !== root.displayedQuoteId) {
                root.checkoutOpen = false
                root.manualExpanded = false
                root.paymentHash = ""
            }
        }
        root.openQuotedCheckout()
        root.prepareQuote()
    }
    Connections {
        target: proCheckout
        function onWalletConnectedSuccessfully() {
            // eth_requestAccounts alone does not verify wallet ownership.
            // Ask the server to bind a one-time EIP-191 proof to this account.
            if (root.purchaseIntent && openProofConnections)
                openProofConnections.requestProWalletChallenge(proCheckout.payerAddress)
        }
    }
    Connections {
        target: openProofConnections
        function onProWalletChallengeReady(id, message) {
            if (root.opened && root.purchaseIntent && proCheckout)
                proCheckout.signWalletChallenge(id,message)
        }
        function onProWalletVerificationCompleted() {
            // The backend checked an actual wallet signature for this account.
            // Only now can we ask it for a price-bound payment quote.
            root.prepareQuote()
        }
    }
    // No refresh from generic account stateChanged: it includes unrelated
    // account mutations and previously caused repeated Pro status checks.

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
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Superellipse {
                id: proIdentityMark
                objectName: "genyProIdentityMark"
                // The mobile header must not reserve an empty logo slot.
                // Show the same membership mark across all viewports.
                Layout.preferredWidth: Theme.scaleUi(root.compact ? 48 : 44)
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignVCenter
                fillColor: Theme.proSurface
                borderColor: Theme.proBorder
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveCompact
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconSpark
                    color: Theme.proAccent
                    preferredSize: Theme.scaleUi(root.compact ? 29 : 25)
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                    horizontalAlignment: root.compact ? Text.AlignHCenter : Theme.textStartAlignment
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.eligible ? qsTrId("genycaster.pro.status_verified")
                        : root.checkoutActive ? qsTrId("genycaster.pro.checkout_payment")
                        : qsTrId("genycaster.pro.checkout_membership")
                    font.pixelSize: Theme.typeCaptionSize
                    color: root.eligible ? Theme.positive : Theme.textSecondary
                    horizontalAlignment: root.compact ? Text.AlignHCenter : Theme.textStartAlignment
                    elide: Text.ElideRight
                }
            }
            ActionButton {
                objectName: "genyProManualStatusRefresh"
                visible: root.opened && root.appSignedIn
                Layout.alignment: Qt.AlignVCenter
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.pro.check_status")
                enabled: !root.busy && !openProofConnections.proEntitlementsBusy
                onClicked: {
                    if (root.cloudReady)
                        openProofConnections.fetchProEntitlements(true)
                    else
                        openProofConnections.retryProEntitlements()
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
                onClicked: root.cancelPayment()
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
            anchors.bottomMargin: Theme.space3
            contentWidth: width
            contentHeight: detailsColumn.implicitHeight + Theme.space2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: detailsColumn
                width: detailsViewport.width
                spacing: Theme.space3

                Superellipse {
                    id: priceCard
                    objectName: "genyProNeutralEligibilitySummary"
                    visible: root.membershipVerified
                    Layout.fillWidth: true
                    implicitHeight: priceColumn.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer
                    ColumnLayout {
                        id: priceColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space3
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            objectName: "genyProCompactPriceHeadline"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: root.eligible ? qsTrId("genycaster.pro.status_verified")
                                : root.priceKnown
                                  ? qsTrId("genycaster.pro.monthly_price").arg(root.monthlyUsd)
                                  : root.priceLoading
                                    ? qsTrId("genycaster.pro.price_loading")
                                    : qsTrId("genycaster.pro.price_unavailable")
                            // Avoid the old unbounded Title1 headline clipping
                            // at narrow drawer widths and in long translations.
                            font.pixelSize: root.priceKnown && !root.checkoutActive
                                ? Theme.typeTitle3Size : Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            objectName: "genyProPriceUnavailableHint"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            visible: !root.eligible && root.priceUnavailable
                            text: qsTrId("genycaster.pro.price_retry_hint")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "genyProQuoteRequestError"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            visible: root.quoteAttempted && !root.checkoutActive
                                && openProofConnections
                                && String(openProofConnections.proBillingError || "").length > 0
                                && openProofConnections.proBillingError !== "payment_pending"
                            text: String(openProofConnections.proBillingError || "")
                                    === "verified_wallet_required"
                                ? qsTrId("genycaster.pro.quote_wallet_required")
                                : qsTrId("genycaster.pro.quote_retry_notice")
                            color: Theme.negative
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "genyProWalletConnectError"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            visible: root.walletAttempted && !root.checkoutActive
                                && proCheckout && String(proCheckout.error || "").length > 0
                            text: root.walletErrorMessage()
                            color: Theme.negative
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            objectName: "genyProWalletProofError"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            visible: root.walletAttempted && !root.checkoutActive
                                && openProofConnections
                                && String(openProofConnections.proWalletError || "").length > 0
                            text: qsTrId("genycaster.pro.wallet_verification_unavailable")
                            color: Theme.negative
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: !root.eligible && !root.checkoutActive
                            text: qsTrId("genycaster.pro.manual_renewal")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textSecondary
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
                            wrapMode: Text.Wrap
                        }
                    }
                }

                // A missing account grant is not evidence that an existing
                // Pro membership expired. Do not show the purchase chooser or
                // the next renewal price until the server verifies this FID.
                Superellipse {
                    objectName: "genyProMembershipUnknownNotice"
                    visible: root.appSignedIn && !root.membershipVerified
                    Layout.fillWidth: true
                    implicitHeight: membershipRecoveryColumn.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceMuted
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer
                    ColumnLayout {
                        id: membershipRecoveryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: Theme.iconSpark
                                color: Theme.proAccent
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.pro.check_status")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                wrapMode: Text.Wrap
                            }
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.signerAuthorizationNeeded
                                ? qsTrId("genycaster.shell.farcaster_sign_in_openproof_description")
                                : qsTrId("genycaster.pro.verification_unavailable")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            visible: root.signerAuthorizationNeeded
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.setup.recovery_signer_consent")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }
                }
                // Even when the account proof is unavailable, the sheet
                // should remain useful without claiming Pro is active.
                ColumnLayout {
                    objectName: "genyProPreviewBenefits"
                    visible: root.appSignedIn && !root.membershipVerified
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.checkout_benefits")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                    Repeater {
                        model: [
                            qsTrId("genycaster.pro.feature_miniapps_title"),
                            qsTrId("genycaster.pro.market_pulse"),
                            qsTrId("genycaster.pro.feature_composer_title")
                        ]
                        delegate: RowLayout {
                            required property string modelData
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: Theme.iconCheck
                                color: Theme.proAccent
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: modelData
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                // Active membership needs a useful, compact benefits summary
                // rather than an empty slab between the price and Done.
                Superellipse {
                    id: activeProBenefits
                    objectName: "genyProActiveBenefits"
                    visible: root.eligible && !root.checkoutActive
                    Layout.fillWidth: true
                    implicitHeight: activeBenefitsColumn.implicitHeight + Theme.space4 * 2
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        id: activeBenefitsColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.space4
                        spacing: Theme.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: Theme.iconSpark
                                color: Theme.proAccent
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.pro.checkout_benefits")
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightBold
                                color: Theme.textPrimary
                                wrapMode: Text.Wrap
                            }
                        }

                        Repeater {
                            model: [
                                {icon: Theme.iconApps,
                                 title: qsTrId("genycaster.pro.feature_miniapps_title")},
                                {icon: Theme.iconBookmark,
                                 title: qsTrId("genycaster.pro.bookmark_collections")},
                                {icon: Theme.iconLink,
                                 title: qsTrId("genycaster.pro.connections")},
                                {icon: Theme.iconChart,
                                 title: qsTrId("genycaster.pro.market_pulse")},
                                {icon: Theme.iconCompose,
                                 title: qsTrId("genycaster.pro.feature_composer_title")}
                            ]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space3
                                layoutDirection: Theme.uiDirection
                                Icon {
                                    text: String(modelData.icon)
                                    color: Theme.textSecondary
                                    preferredSize: Theme.iconSizeSmall
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: String(modelData.title)
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeBodySize
                                    wrapMode: Text.Wrap
                                }
                                Icon {
                                    text: Theme.iconCheck
                                    color: Theme.positive
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }
                        }
                    }
                }

                // Intro: a short explanation and two explicit payment methods.
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !root.checkoutActive && !root.eligible && root.membershipVerified
                    spacing: Theme.space3

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.checkout_choose_currency")
                        color: root.paymentOptionsReady ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }

                    RowLayout {
                        layoutDirection: Theme.uiDirection
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Repeater {
                            model: ["GENY", "USDC"]
                            delegate: Button {
                                required property string modelData
                                readonly property bool chosen: root.selectedAsset === modelData
                                Layout.fillWidth: true
                                implicitHeight: Theme.scaleUi(44)
                                enabled: root.paymentOptionsReady
                                opacity: enabled ? 1.0 : 0.52
                                focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
                                hoverEnabled: enabled && !root.compact
                                Accessible.name: modelData
                                Accessible.description: chosen && enabled
                                    ? qsTrId("genycaster.pro.checkout_selected") : ""
                                onClicked: root.selectedAsset = modelData
                                background: Superellipse {
                                    curveDepth: Theme.curveControl
                                    fillColor: chosen && root.paymentOptionsReady
                                        ? Theme.surfaceMuted : Theme.surfaceRaised
                                    borderColor: chosen && root.paymentOptionsReady
                                        ? Theme.accent : Theme.borderSubtle
                                    borderWidth: chosen && root.paymentOptionsReady
                                        ? Theme.strokeEmphasis : Theme.strokeHairline
                                }
                                contentItem: RowLayout {
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2
                                    Item { Layout.fillWidth: true }
                                    ProTokenMark {
                                        asset: modelData
                                        tokenSize: Theme.scaleUi(30)
                                    }
                                    DirectionAwareLabel {
                                        text: modelData
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: chosen ? Theme.weightBold : Theme.weightMedium
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.paymentOptionsReady
                        text: root.selectedAsset === "USDC"
                              ? qsTrId("genycaster.pro.checkout_usdc_hint")
                              : qsTrId("genycaster.pro.checkout_geny_hint")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    // Value proposition does not dominate payment. Expand on demand.
                    Button {
                        id: benefitsToggle
                        Layout.fillWidth: true
                        implicitHeight: Theme.scaleUi(46)
                        padding: Theme.space2
                        flat: true
                        hoverEnabled: !root.compact
                        onClicked: root.showBenefits = !root.showBenefits
                        background: Superellipse {
                            fillColor: benefitsToggle.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            borderColor: Theme.borderSubtle
                            borderWidth: Theme.strokeHairline
                            curveDepth: Theme.curveCompact
                        }
                        contentItem: RowLayout {
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: Theme.iconSpark
                                color: Theme.proAccent
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.pro.checkout_benefits")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightMedium
                            }
                            Icon {
                                text: root.showBenefits ? Theme.iconChevronUp : Theme.iconChevronDown
                                color: Theme.textSecondary
                                preferredSize: Theme.iconSizeSmall
                            }
                        }
                    }
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
                        visible: root.showBenefits && !root.checkoutActive && !root.eligible
                            && root.membershipVerified
                        implicitHeight: Math.max(42, featureLayout.implicitHeight + Theme.space2 * 2)
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
                            Icon {
                                text: String(feature.modelData.icon || "")
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
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
                                text: feature.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.checkoutActive
                    spacing: Theme.space3

                    ActionButton {
                        objectName: "genyProChangePaymentMethod"
                        visible: !root.txSubmitted
                        enabled: !root.busy
                        compact: true
                        quiet: true
                        text: qsTrId("genycaster.pro.checkout_change_method")
                        onClicked: {
                            root.checkoutOpen = false
                            root.manualExpanded = false
                            detailsViewport.contentY = 0
                        }
                    }

                    RowLayout {
                        layoutDirection: Theme.uiDirection
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        ProTokenMark {
                            asset: root.quoteAsset
                            tokenSize: Theme.scaleUi(40)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel {
                                text: qsTrId("genycaster.pro.checkout_pay_amount")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.amountText
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                                forceLeftToRight: true
                                elide: Text.ElideRight
                            }
                        }
                        DirectionAwareLabel {
                            text: root.quoteAsset + " · Base"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: validityRow.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        borderColor: Theme.borderSubtle
                        borderWidth: Theme.strokeHairline
                        curveDepth: Theme.curveCompact
                        RowLayout {
                            layoutDirection: Theme.uiDirection
                            id: validityRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.space3
                            spacing: Theme.space2
                            Icon {
                                text: Theme.iconCalendar
                                color: root.quoteValid ? Theme.textSecondary : Theme.negative
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.quoteValid
                                    ? qsTrId("genycaster.pro.checkout_expires_in")
                                        .arg(root.timeRemaining(root.remainingSeconds))
                                    : qsTrId("genycaster.pro.quote_expired")
                                color: root.quoteValid ? Theme.textSecondary : Theme.negative
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: !root.txSubmitted
                        text: qsTrId("genycaster.pro.checkout_sender").arg(
                            root.shortWallet((root.paymentQuote.verified_payers || [])[0]))
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.txSubmitted && !root.eligible
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.checkout_confirming")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.shortWallet(proCheckout ? proCheckout.txHash : "")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.checkout_confirmation_hint")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: proCheckout && String(proCheckout.error || "").length > 0
                            && !root.txSubmitted
                        text: root.walletErrorMessage()
                        color: Theme.negative
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: openProofConnections
                            && String(openProofConnections.proBillingError || "").length > 0
                            && openProofConnections.proBillingError !== "payment_pending"
                        text: qsTrId("genycaster.pro.billing_error")
                        color: Theme.negative
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    Button {
                        id: advancedPayment
                        Layout.fillWidth: true
                        visible: !root.txSubmitted && root.quoteValid
                        flat: true
                        hoverEnabled: !root.compact
                        implicitHeight: Theme.scaleUi(38)
                        onClicked: root.manualExpanded = !root.manualExpanded
                        contentItem: RowLayout {
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.walletAvailable
                                    ? qsTrId("genycaster.pro.checkout_manual_fallback")
                                    : qsTrId("genycaster.pro.checkout_manual_only")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            Icon {
                                text: root.manualExpanded ? Theme.iconChevronUp : Theme.iconChevronDown
                                preferredSize: Theme.iconSizeSmall
                                color: Theme.textSecondary
                            }
                        }
                        background: Item {}
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.manualExpanded && !root.txSubmitted && root.quoteValid
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.pro.payment_instructions")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
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
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                            elide: Text.ElideMiddle
                        }
                        RowLayout {
                            layoutDirection: Theme.uiDirection
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            ActionButton {
                                Layout.fillWidth: true
                                compact: true
                                quiet: true
                                text: qsTrId("genycaster.pro.copy_amount")
                                iconGlyph: Theme.iconCopy
                                onClicked: presentationBridge.copyText(
                                    String(root.paymentQuote.amount_geny || ""))
                            }
                            ActionButton {
                                Layout.fillWidth: true
                                compact: true
                                quiet: true
                                text: qsTrId("genycaster.pro.copy_recipient")
                                iconGlyph: Theme.iconCopy
                                onClicked: presentationBridge.copyText(
                                    String(root.paymentQuote.treasury || ""))
                            }
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
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.pro.checkout_gas_note")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.textSecondary
                        wrapMode: Text.Wrap
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.appSignedIn && !root.cloudReady && !root.eligible
                    text: root.restoringSession
                        ? qsTrId("genycaster.pro.secure_session_hint")
                        : root.proofError === "account_service_unavailable"
                          ? qsTrId("genycaster.pro.verification_unavailable")
                          : root.proofError.length > 0
                            ? qsTrId("genycaster.pro.secure_session_unavailable")
                            : qsTrId("genycaster.pro.secure_session_hint")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.monthlyUsdCents > 0 && root.plan.payment_available === false
                    text: qsTrId("genycaster.pro.payment_unavailable")
                    color: Theme.negative
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
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
            height: Math.max(60, implicitHeight)
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            ActionButton {
                objectName: "genyProCancelCheckout"
                visible: true
                compact: true
                quiet: true
                text: qsTrId("genycaster.wallet.cancel")
                onClicked: root.cancelPayment()
            }
            ActionButton {
                Layout.fillWidth: true
                compact: false
                prominent: true
                iconGlyph: root.walletAvailable && !root.txSubmitted
                    && (root.checkoutActive || !root.walletConnected)
                    ? Theme.iconWallet : ""
                busy: root.busy
                enabled: !root.busy
                    && (root.eligible || !root.membershipVerified || !root.cloudReady
                        || root.checkoutActive || root.canRequestQuote)
                    && !(openProofConnections && openProofConnections.proPlanBusy)
                    && !(root.appSignedIn && root.restoringSession)
                    && (root.eligible || !root.checkoutActive || root.txSubmitted
                        || !root.quoteValid || (root.manualExpanded || !root.walletAvailable)
                        ? root.validPaymentHash || root.eligible || !root.checkoutActive
                          || root.txSubmitted || !root.quoteValid
                        : root.walletAvailable)
                text: root.primaryLabel()
                onClicked: root.proceed()
            }
        }
    }
}
