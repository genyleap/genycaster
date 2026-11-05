import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Pure presentation: no loopback session token, secret or wallet key is
// exposed to QML. Only the trusted controller may launch a browser.
Item {
    id: picker
    implicitWidth: 0
    implicitHeight: 0

    readonly property bool hasRequest: miniAppHost.walletBrowserPickerActive
                                      || miniAppHost.walletBrowserWaiting

    function browserIcon(id) {
        const path = "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/"
        switch (String(id || "")) {
        case "brave": return path + "brave.svg"
        case "chrome": return path + "chrome.svg"
        case "firefox": return path + "firefox.svg"
        default: return ""
        }
    }

    function synchronize() {
        if (hasRequest) {
            if (!dialog.opened)
                dialog.open()
        } else if (dialog.opened) {
            dialog.close()
        }
    }

    GenyModal {
        id: dialog
        parent: picker.parent
        z: 1200
        padding: Theme.space4
        width: Math.min(420, parent ? Math.max(300, parent.width - Theme.space5 * 2) : 420)
        height: Math.min(parent ? Math.max(280, parent.height - Theme.space5 * 2) : 490,
                         contentLayout.implicitHeight + topPadding + bottomPadding)
        // Dismissal must NEVER revoke a wallet session implicitly. Only
        // explicit Cancel/X/Escape rejects the pending request.
        closePolicy: Popup.NoAutoClose

        Shortcut {
            sequence: "Escape"
            enabled: dialog.opened
            onActivated: miniAppHost.cancelWalletBrowser()
        }

        contentItem: ColumnLayout {
            id: contentLayout
            spacing: Theme.space3

            ModalHeader {
                Layout.fillWidth: true
                title: miniAppHost.walletBrowserWaiting
                       ? qsTr("Waiting for your wallet")
                       : qsTr("Connect a wallet")
                subtitle: miniAppHost.walletBrowserAppName.length
                          ? miniAppHost.walletBrowserAppName
                          : qsTr("Mini App wallet request")
                onCloseRequested: miniAppHost.cancelWalletBrowser()
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.scaleUi(43)
                fillColor: Theme.surfaceMuted
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveControl

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space2

                    Icon {
                        text: Theme.iconWallet
                        color: Theme.accent
                        preferredSize: Theme.iconSizeSmall
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: miniAppHost.walletBrowserOrigin
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideMiddle
                        maximumLineCount: 1
                    }
                }
            }

            ColumnLayout {
                visible: !miniAppHost.walletBrowserWaiting
                Layout.fillWidth: true
                spacing: Theme.space2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("Open the request in a browser with your wallet extension.")
                    color: Theme.inkMuted
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.typeCaptionSize
                }

                Repeater {
                    model: miniAppHost.walletBrowserOptions

                    delegate: Button {
                        id: choice
                        required property var modelData
                        readonly property string browserId: String(modelData.id || "")
                        readonly property string browserName: String(modelData.label || "")
                        readonly property bool previous: Boolean(modelData.lastUsed)

                        Layout.fillWidth: true
                        implicitHeight: Theme.scaleUi(52)
                        padding: 0
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: browserName
                        Accessible.description: previous ? qsTr("Last used browser") : ""
                        onClicked: miniAppHost.chooseWalletBrowser(browserId)

                        background: Superellipse {
                            fillColor: choice.down ? Theme.primarySurface
                                       : choice.hovered || choice.activeFocus
                                         ? Theme.surface : Theme.surfaceMuted
                            borderWidth: Theme.strokeHairline
                            borderColor: choice.activeFocus
                                         ? Theme.accent : Theme.borderSubtle
                            curveDepth: Theme.curveControl
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space3
                            LayoutMirroring.enabled: false
                            LayoutMirroring.childrenInherit: true
                            layoutDirection: Theme.uiDirection

                            Superellipse {
                                Layout.preferredWidth: Theme.scaleUi(34)
                                Layout.preferredHeight: Theme.scaleUi(34)
                                fillColor: Theme.surface
                                borderColor: Theme.borderSubtle
                                borderWidth: Theme.strokeHairline
                                curveDepth: Theme.curveCompact

                                Image {
                                    id: browserLogo
                                    anchors.centerIn: parent
                                    width: Theme.scaleUi(23)
                                    height: Theme.scaleUi(23)
                                    source: picker.browserIcon(choice.browserId)
                                    sourceSize.width: 72
                                    sourceSize.height: 72
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    visible: source.toString().length > 0
                                             && status === Image.Ready
                                }
                                Icon {
                                    anchors.centerIn: parent
                                    visible: !browserLogo.visible
                                    text: Theme.iconGlobe
                                    preferredSize: Theme.iconSizeSmall
                                    color: Theme.inkMuted
                                }
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: choice.browserName
                                color: Theme.ink
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }

                            DirectionAwareLabel {
                                visible: choice.previous
                                text: qsTr("Last used")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                            }

                            Icon {
                                text: Theme.iconArrowRight
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: miniAppHost.walletBrowserWaiting
                Layout.fillWidth: true
                spacing: Theme.space2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("Complete the request in %1.").arg(
                              miniAppHost.walletBrowserSelectedName)
                    color: Theme.ink
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("The Mini App stays open. This window will close when your wallet responds.")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }

            DirectionAwareLabel {
                visible: miniAppHost.walletBrowserError.length > 0
                Layout.fillWidth: true
                text: miniAppHost.walletBrowserError
                color: Theme.danger
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTr("Your keys stay in your wallet. Every signature needs separate approval.")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ActionButton {
                    quiet: true
                    compact: true
                    text: miniAppHost.walletBrowserWaiting
                          ? qsTr("Cancel request") : qsTr("Cancel")
                    onClicked: miniAppHost.cancelWalletBrowser()
                }
            }
        }

        // No implicit cancel in onClosed. A successful browser launch, focus
        // transition, or Qt popup animation must not terminate a Mini App.
    }

    Connections {
        target: miniAppHost
        function onWalletBrowserPickerChanged() {
            picker.synchronize()
        }
    }
    Component.onCompleted: picker.synchronize()
}
