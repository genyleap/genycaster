import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Shared, honest Pro locked-state explanation. This never self-upgrades an
// account: the refresh action asks the server to verify the logged-in FID.
Popup {
    id: root
    objectName: "genyProRequiredNotice"
    parent: Overlay.overlay
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    property string featureLabel: ""
    signal upgradeRequested()
    readonly property var membership: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property int monthlyUsdCents: Number(membership.monthly_usd_cents || 0)
    width: parent ? Math.min(Theme.scaleUi(380), parent.width - Theme.space4 * 2)
                  : Theme.scaleUi(360)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? Math.max(Theme.space2, (parent.height - height) / 2) : 0
    padding: Theme.space4
    background: PopupSurface { curveDepth: Theme.curveContainer }
    contentItem: ColumnLayout {
        spacing: Theme.space3
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Superellipse {
                Layout.preferredWidth: Theme.scaleUi(40)
                Layout.preferredHeight: Layout.preferredWidth
                fillColor: Theme.proSurface
                borderWidth: 1
                borderColor: Theme.proBorder
                curveDepth: Theme.curveCompact
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconSpark
                    color: Theme.proAccent
                    preferredSize: Theme.iconSizeSmall
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.title")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.featureLabel
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.pro.unlock_hint")
                .arg(root.monthlyUsdCents > 0
                    ? (root.monthlyUsdCents / 100).toFixed(2) : "—")
            font.pixelSize: Theme.typeBodySize
            color: Theme.textPrimary
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.membership.status === "unavailable"
            text: qsTrId("genycaster.pro.verification_unavailable")
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
        ActionButton {
            Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
            visible: root.membership.status !== "verified"
            compact: true
            quiet: true
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.pro.check_status")
            onClicked: {
                if (openProofConnections) openProofConnections.retryProEntitlements()
                root.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
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
                iconGlyph: Theme.iconSpark
                text: qsTrId("genycaster.pro.get_pro")
                onClicked: {
                    root.close()
                    root.upgradeRequested()
                }
            }
        }
    }
}
