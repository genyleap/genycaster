import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// The host owns the quote and wallet flow. Mini App JS never receives an
// approval capability and cannot change transaction calldata or the spender.
GenyAdaptiveDialog {
    id: control
    objectName: "miniAppSwapPreflight"
    property bool compact: false
    property string renderedRequestId: ""
    property string sellTokenInput: ""
    property string buyTokenInput: ""
    property string sellAmountInput: ""
    property string slippageInput: "100"
    readonly property bool transactionLocked: [
        "approving", "approval_receipt", "swapping", "swap_receipt"
    ].includes(miniAppHost.swapStage)
    popupZ: 21000
    width: compact ? (parent ? parent.width : 440) : 490
    height: compact ? 720 : 700
    mobileHeightRatio: 0.94
    closePolicy: control.transactionLocked ? Popup.NoAutoClose : Popup.CloseOnEscape

    onDidClose: {
        if (miniAppHost.swapRequestActive)
            miniAppHost.cancelSwapToken(miniAppHost.swapRequestId)
    }

    contentItem: Flickable {
        id: bodyFlick
        clip: true
        contentWidth: width
        contentHeight: swapForm.implicitHeight
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: swapForm
            width: bodyFlick.width
            spacing: Theme.space2

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.title")
            color: Theme.ink
            font.weight: Theme.weightBold
            font.pixelSize: Theme.typeTitleSize
            horizontalAlignment: Text.AlignHCenter
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: String((miniAppHost.swapRequest || ({})).appName || "")
            color: Theme.inkMuted
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.network_base")
            color: Theme.accent
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.sell_token")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
        }
        BidiTextField {
            Layout.fillWidth: true
            forceLeftToRight: true
            text: control.sellTokenInput
            enabled: !miniAppHost.swapQuoteBusy && !control.transactionLocked
            placeholderText: qsTrId("genycaster.miniapp.swap.sell_token")
            onTextEdited: {
                control.sellTokenInput = text
                miniAppHost.invalidateSwapQuote(miniAppHost.swapRequestId)
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.buy_token")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
        }
        BidiTextField {
            Layout.fillWidth: true
            forceLeftToRight: true
            text: control.buyTokenInput
            enabled: !miniAppHost.swapQuoteBusy && !control.transactionLocked
            placeholderText: qsTrId("genycaster.miniapp.swap.buy_token")
            onTextEdited: {
                control.buyTokenInput = text
                miniAppHost.invalidateSwapQuote(miniAppHost.swapRequestId)
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.amount_base_units")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
        }
        BidiTextField {
            Layout.fillWidth: true
            forceLeftToRight: true
            text: control.sellAmountInput
            enabled: !miniAppHost.swapQuoteBusy && !control.transactionLocked
            placeholderText: "1000000"
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: {
                control.sellAmountInput = text
                miniAppHost.invalidateSwapQuote(miniAppHost.swapRequestId)
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.miniapp.swap.max_slippage")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
        }
        BidiTextField {
            Layout.fillWidth: true
            forceLeftToRight: true
            enabled: !miniAppHost.swapQuoteBusy && !control.transactionLocked
            text: control.slippageInput
            inputMethodHints: Qt.ImhDigitsOnly
            placeholderText: qsTrId("genycaster.miniapp.swap.max_slippage")
            onTextEdited: {
                control.slippageInput = text
                miniAppHost.invalidateSwapQuote(miniAppHost.swapRequestId)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: Object.keys(miniAppHost.swapQuote || ({})).length > 0
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.miniapp.swap.min_receive")
                      + ": " + String((miniAppHost.swapQuote || ({})).minBuyAmount || "—")
                color: Theme.ink
                font.weight: Theme.weightDemiBold
                wrapMode: Text.WrapAnywhere
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.miniapp.swap.network_fee")
                      + ": " + String((miniAppHost.swapQuote || ({})).totalNetworkFee || "—")
                color: Theme.inkMuted
                wrapMode: Text.WrapAnywhere
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: (miniAppHost.swapQuote || ({})).requiresApproval
                    ? qsTrId("genycaster.miniapp.swap.approval_required")
                    : qsTrId("genycaster.miniapp.swap.approval_not_required")
                color: Theme.inkMuted
                wrapMode: Text.WordWrap
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: miniAppHost.swapQuoteBusy || control.transactionLocked
            text: {
                const stage = miniAppHost.swapStage
                if (stage === "approving" || stage === "swapping" || stage === "checking")
                    return qsTrId("genycaster.miniapp.swap.wallet_wait")
                if (stage === "approval_receipt")
                    return qsTrId("genycaster.miniapp.swap.approval_receipt_wait")
                if (stage === "swap_receipt")
                    return qsTrId("genycaster.miniapp.swap.swap_receipt_wait")
                if (stage === "refreshing")
                    return qsTrId("genycaster.miniapp.swap.quote_refreshing")
                return qsTrId("genycaster.miniapp.swap.quote_loading")
            }
            color: Theme.accent
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: String(miniAppHost.swapTxHash || "").length > 0
            text: String(miniAppHost.swapTxHash || "")
            textFormat: Text.PlainText
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: String(miniAppHost.swapQuoteError || "").length > 0
            text: String(miniAppHost.swapQuoteError || "")
            color: Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTrId("genycaster.miniapp.swap.execution_notice")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        ActionButton {
            Layout.fillWidth: true
            prominent: true
            busy: miniAppHost.swapQuoteBusy
            visible: !control.transactionLocked && miniAppHost.swapStage !== "review"
            enabled: !miniAppHost.swapQuoteBusy && control.sellTokenInput.length > 0
                     && control.buyTokenInput.length > 0 && control.sellAmountInput.length > 0
                     && Number.isInteger(Number(control.slippageInput))
                     && Number(control.slippageInput) >= 10
                     && Number(control.slippageInput) <= 300
                     && openProofConnections.cloudAuthorizationReady
            text: qsTrId("genycaster.miniapp.swap.get_quote")
            onClicked: miniAppHost.beginSwapQuote(miniAppHost.swapRequestId, {
                sellToken: control.sellTokenInput,
                buyToken: control.buyTokenInput,
                sellAmount: control.sellAmountInput,
                slippageBps: Number(control.slippageInput)
            })
        }
        ActionButton {
            Layout.fillWidth: true
            visible: miniAppHost.swapStage === "review"
                     && Object.keys(miniAppHost.swapQuote || ({})).length > 0
            enabled: !miniAppHost.swapQuoteBusy
            prominent: true
            text: (miniAppHost.swapQuote || ({})).requiresApproval
                ? qsTrId("genycaster.miniapp.swap.approve_exact")
                : qsTrId("genycaster.miniapp.swap.confirm_swap")
            onClicked: miniAppHost.executeSwapStep(miniAppHost.swapRequestId)
        }
        ActionButton {
            Layout.fillWidth: true
            enabled: !control.transactionLocked
            visible: !control.transactionLocked
            text: qsTrId("genycaster.shell.cancel")
            onClicked: miniAppHost.cancelSwapToken(miniAppHost.swapRequestId)
        }
        }
    }

    Connections {
        target: miniAppHost
        function onSwapRequestChanged() {
            const id = miniAppHost.swapRequestActive
                       ? String(miniAppHost.swapRequestId) : ""
            if (id && control.renderedRequestId !== id) {
                control.renderedRequestId = id
                const preview = miniAppHost.swapRequest || ({})
                control.sellTokenInput = String(preview.sellToken || "")
                control.buyTokenInput = String(preview.buyToken || "")
                control.sellAmountInput = String(preview.sellAmount || "")
                control.slippageInput = String(preview.slippageBps || 100)
                control.open()
            } else if (!id) {
                control.renderedRequestId = ""
                control.close()
            }
        }
    }
}
