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
    popupZ: 21000
    width: compact ? (parent ? parent.width : 440) : 490
    height: compact ? 610 : 580
    mobileHeightRatio: 0.92
    closePolicy: Popup.CloseOnEscape

    onDidClose: {
        if (miniAppHost.swapRequestActive)
            miniAppHost.cancelSwapToken(miniAppHost.swapRequestId)
    }

    contentItem: ColumnLayout {
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
            enabled: !miniAppHost.swapQuoteBusy
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
            enabled: !miniAppHost.swapQuoteBusy
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
            enabled: !miniAppHost.swapQuoteBusy
            placeholderText: "1000000"
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: {
                control.sellAmountInput = text
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
            visible: String(miniAppHost.swapQuoteError || "").length > 0
            text: String(miniAppHost.swapQuoteError || "")
            color: Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTrId("genycaster.miniapp.swap.quote_notice")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        ActionButton {
            Layout.fillWidth: true
            prominent: true
            busy: miniAppHost.swapQuoteBusy
            enabled: !miniAppHost.swapQuoteBusy && control.sellTokenInput.length > 0
                     && control.buyTokenInput.length > 0 && control.sellAmountInput.length > 0
                     && openProofConnections.cloudAuthorizationReady
            text: qsTrId("genycaster.miniapp.swap.get_quote")
            onClicked: miniAppHost.beginSwapQuote(miniAppHost.swapRequestId, {
                sellToken: control.sellTokenInput,
                buyToken: control.buyTokenInput,
                sellAmount: control.sellAmountInput
            })
        }
        ActionButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.shell.cancel")
            onClicked: miniAppHost.cancelSwapToken(miniAppHost.swapRequestId)
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
                control.open()
            } else if (!id) {
                control.renderedRequestId = ""
                control.close()
            }
        }
    }
}
