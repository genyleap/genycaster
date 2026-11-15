import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Wallet approval is owned by the trusted host. The native Mini App surface
// is temporarily hidden while this Qt modal is open, then restored unchanged.
Item {
    id: picker
    property var walletHost: miniAppHost
    implicitWidth: 0
    implicitHeight: 0

    function browserIcon(id) {
        const base = "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/"
        switch (String(id || "")) {
        case "brave": return base + "brave.svg"
        case "chrome": return base + "chrome.svg"
        case "firefox": return base + "firefox.svg"
        default: return ""
        }
    }

    function sync() {
        // Waiting after launch belongs to the Mini App, not a second modal
        // stacked beneath its native WebKit window.
        const selecting = picker.walletHost.walletBrowserPickerActive
        if (selecting && !dialog.opened)
            dialog.open()
        else if (!selecting && dialog.opened)
            dialog.close()
    }

    GenyModal {
        id: dialog
        parent: picker.parent
        z: 1200
        padding: Theme.space3
        width: Math.min(380, parent ? Math.max(280, parent.width - Theme.space4 * 2) : 380)
        height: Math.min(parent ? parent.height - Theme.space4 * 2 : 420,
                         column.implicitHeight + topPadding + bottomPadding)
        closePolicy: Popup.NoAutoClose

        // A click outside the chooser must not close the Mini App or silently
        // reject a pending wallet request.
        Overlay.modal: DrawerBackdrop {
            // The host hides its native Mini App surface during approval.
            // Capture only the trusted workspace, never Window.contentItem.
            sourceItem: picker.parent && picker.parent !== Overlay.overlay
                        ? picker.parent : null
            snapshotBlurOnBrowser: true
            active: dialog.visible
            addScrim: true
            blockInput: true
            // Intentionally no onBackgroundTapped: this dialog is NoAutoClose.
        }

        Shortcut {
            sequence: "Escape"
            enabled: dialog.opened
            onActivated: picker.walletHost.cancelWalletBrowser()
        }

        contentItem: ColumnLayout {
            id: column
            spacing: Theme.space2
            LayoutMirroring.enabled: false
            layoutDirection: Theme.uiDirection

            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.wallet.browser_title")
                subtitle: picker.walletHost.walletBrowserAppName
                onCloseRequested: picker.walletHost.cancelWalletBrowser()
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: picker.walletHost.walletBrowserOrigin
                forceLeftToRight: true
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideMiddle
            }

            Repeater {
                model: picker.walletHost.walletBrowserOptions

                delegate: Button {
                    id: option
                    required property var modelData
                    readonly property string browserId: String(modelData.id || "")
                    readonly property string browserName: browserId === "system"
                        ? qsTrId("genycaster.wallet.default_browser")
                        : String(modelData.label || "")
                    readonly property bool lastUsed: Boolean(modelData.lastUsed)

                    Layout.fillWidth: true
                    implicitHeight: Theme.scaleUi(44)
                    padding: 0
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: browserName
                    Accessible.description: lastUsed ? qsTrId("genycaster.wallet.previous_browser") : ""
                    onClicked: picker.walletHost.chooseWalletBrowser(browserId)

                    background: Superellipse {
                        fillColor: option.down ? Theme.primarySurface
                                   : option.hovered || option.activeFocus
                                     ? Theme.surface : Theme.surfaceMuted
                        borderColor: option.activeFocus ? Theme.accent : Theme.borderSubtle
                        borderWidth: Theme.strokeHairline
                        curveDepth: Theme.curveControl
                    }

                    contentItem: Item {
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space2
                            anchors.rightMargin: Theme.space2
                            spacing: Theme.space2
                            LayoutMirroring.enabled: false
                            layoutDirection: Theme.uiDirection

                        Superellipse {
                            Layout.preferredWidth: Theme.scaleUi(30)
                            Layout.preferredHeight: Theme.scaleUi(30)
                            fillColor: Theme.surface
                            borderColor: Theme.borderSubtle
                            borderWidth: Theme.strokeHairline
                            curveDepth: Theme.curveCompact

                            Image {
                                id: brand
                                anchors.centerIn: parent
                                width: Theme.scaleUi(20)
                                height: Theme.scaleUi(20)
                                source: picker.browserIcon(option.browserId)
                                sourceSize.width: 60
                                sourceSize.height: 60
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: false
                                visible: status === Image.Ready && source.toString().length > 0
                            }
                            Icon {
                                anchors.centerIn: parent
                                visible: !brand.visible
                                text: Theme.iconGlobe
                                preferredSize: Theme.iconSizeSmall
                                color: Theme.inkMuted
                            }
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: option.browserName
                            alignToUi: true
                            font.weight: Theme.weightDemiBold
                            font.pixelSize: Theme.typeMetaSize
                            color: Theme.ink
                            elide: Text.ElideRight
                        }

                        DirectionAwareLabel {
                            visible: option.lastUsed
                            text: qsTrId("genycaster.wallet.last_used")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                        }

                        Icon {
                            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            preferredSize: Theme.iconSizeSmall
                            color: option.hovered ? Theme.accent : Theme.inkMuted
                        }
                        }
                    }
                }
            }

            DirectionAwareLabel {
                visible: picker.walletHost.walletBrowserError.length > 0
                Layout.fillWidth: true
                text: picker.walletHost.walletBrowserError
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.danger
                wrapMode: Text.Wrap
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.wallet.approval_hint")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ActionButton {
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.wallet.cancel")
                    onClicked: picker.walletHost.cancelWalletBrowser()
                }
            }
        }
    }

    Connections {
        target: picker.walletHost
        function onWalletBrowserPickerChanged() { picker.sync() }
    }
    Component.onCompleted: picker.sync()
}
