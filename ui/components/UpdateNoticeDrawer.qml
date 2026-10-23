import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Lightweight release notice. Instantiates only one drawer per AppShell,
// and opens once per newly discovered version; no forced downloads.
Drawer {
    id: root
    objectName: "updateNoticeDrawer"
    property string lastPresentedVersion: ""

    parent: Overlay.overlay
    modal: true
    focus: true
    dim: true
    interactive: false
    edge: Qt.TopEdge
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(560, Math.max(290, parent.width - Theme.space5 * 2)) : 520
    height: 234
    x: parent ? Math.max(0, (parent.width - width) / 2) : 0
    padding: Theme.space4

    function presentAvailableUpdate() {
        const version = String(updateController.latestVersion || "")
        if (updateController.updateAvailable && version.length > 0
                && version !== lastPresentedVersion) {
            lastPresentedVersion = version
            open()
        }
    }
    Component.onCompleted: Qt.callLater(presentAvailableUpdate)
    Connections {
        target: updateController
        function onChanged() { root.presentAvailableUpdate() }
    }

    background: PopupSurface {
        curveDepth: Theme.curveSheet
        elevation: Theme.elevationPopover
    }
    contentItem: ColumnLayout {
        spacing: Theme.space3
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            Superellipse {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveControl
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconRefresh
                    preferredSize: 24
                    color: Theme.accent
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("Genycaster update available")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.ink
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("A newer release is ready to review.")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                }
            }
            ActionButton {
                compact: true
                iconOnly: true
                quiet: true
                iconGlyph: Theme.iconClose
                text: qsTr("Not now")
                onClicked: root.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            ColumnLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    text: qsTr("Current version")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    text: String(updateController.appVersion || "—")
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                }
            }
            Icon {
                text: Theme.iconArrowRight
                color: Theme.inkMuted
                preferredSize: Theme.iconSize
            }
            ColumnLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    text: qsTr("New version")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    text: String(updateController.latestVersion || "—")
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.accent
                }
            }
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                text: qsTr("Release notes")
                iconGlyph: Theme.iconArrowUpRight
                onClicked: {
                    root.close()
                    updateController.openReleasePage()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: true
                prominent: true
                busy: updateController.downloading || updateController.webRefreshing
                iconGlyph: Theme.iconRefresh
                text: Qt.platform.os === "wasm" ? qsTr("Refresh to update")
                      : updateController.canDownload
                        ? (updateController.downloadedFilePath.length > 0
                            ? qsTr("Open installer") : qsTr("Download"))
                        : qsTr("View release")
                onClicked: {
                    root.close()
                    if (Qt.platform.os === "wasm")
                        updateController.refreshWebApp()
                    else if (updateController.canDownload) {
                        if (updateController.downloadedFilePath.length > 0)
                            updateController.openDownloadedUpdate()
                        else
                            updateController.downloadUpdate()
                    } else
                        updateController.openReleasePage()
                }
            }
        }
    }
}
