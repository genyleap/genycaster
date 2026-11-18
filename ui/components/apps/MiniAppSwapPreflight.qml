import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// First-party, non-executable SwapToken preflight. Mini App JS only supplies
// a validated token suggestion; no trusted quote means no signing button.
// The runtime owns request lifetime and invalidates it on navigation.
GenyAdaptiveDialog {
    id: control
    objectName: "miniAppSwapPreflight"
    property bool compact: false
    popupZ: 21000
    width: compact ? (parent ? parent.width : 440) : 480
    height: compact ? 430 : 390
    mobileHeightRatio: 0.82
    closePolicy: Popup.CloseOnEscape

    onDidClose: {
        if (miniAppHost.swapRequestActive)
            miniAppHost.cancelSwapToken(miniAppHost.swapRequestId)
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

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

        Repeater {
            model: [
                {label: qsTrId("genycaster.miniapp.swap.sell_token"), value: String((miniAppHost.swapRequest || ({})).sellToken || "—")},
                {label: qsTrId("genycaster.miniapp.swap.buy_token"), value: String((miniAppHost.swapRequest || ({})).buyToken || "—")},
                {label: qsTrId("genycaster.miniapp.swap.amount_base_units"), value: String((miniAppHost.swapRequest || ({})).sellAmount || "—")}
            ]
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.space3
                DirectionAwareLabel {
                    text: modelData.label
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: modelData.value
                    textFormat: Text.PlainText
                    color: Theme.ink
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WrapAnywhere
                    horizontalAlignment: Theme.rtl ? Text.AlignLeft : Text.AlignRight
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTrId("genycaster.miniapp.swap.quote_unavailable")
            color: Theme.inkMuted
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
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
            if (miniAppHost.swapRequestActive)
                control.open()
            else
                control.close()
        }
    }
}
