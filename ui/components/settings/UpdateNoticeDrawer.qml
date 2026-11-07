import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Lightweight release notice. Instantiates only one drawer per AppShell,
// and opens once per newly discovered version; no forced downloads.
Drawer {
    id: root
    objectName: "updateNoticeDrawer"
    property string lastPresentedVersion: ""
    readonly property bool sameSemanticRelease: Qt.platform.os === "wasm"
        && updateController.latestVersion.length > 0
        && updateController.latestVersion === updateController.appVersion
        && updateController.webCurrentBuild.length >= 8
        && updateController.webLatestBuild.length === 8
    // Release categories are a bounded server-controlled enum. Use static
    // translation IDs so extraction/coverage checks can inspect every label.
    function releaseTypeLabel(kind) {
        switch (String(kind || "")) {
        case "fix": return qsTrId("genycaster.update.type_fix")
        case "improvement": return qsTrId("genycaster.update.type_improvement")
        case "feature": return qsTrId("genycaster.update.type_feature")
        case "security": return qsTrId("genycaster.update.type_security")
        case "maintenance": return qsTrId("genycaster.update.type_maintenance")
        default: return ""
        }
    }

    parent: Overlay.overlay
    modal: true
    focus: true
    dim: true
    interactive: false
    edge: Qt.TopEdge
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(580, parent.width) : 520
    // The footer must stay inside the modal even with very long release notes.
    // Leave room for the system safe area; scroll the notes, not the actions.
    height: parent ? Math.max(0, Math.min(parent.height - Theme.space4 * 2,
                                          Math.max(340, Math.min(640, parent.height * 0.86)))) : 560
    x: parent ? Math.max(0, (parent.width - width) / 2) : 0
    padding: Theme.space3
    // A bounded viewport: header and actions never become part of the
    // scrollable release notes, even when the translated notes are lengthy.

    // Animate Drawer.position, never its x/y: Qt owns edge anchoring.
    enter: Transition {
        NumberAnimation {
            property: "position"
            from: 0
            to: 1
            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "position"
            to: 0
            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
            easing.type: Easing.InOutCubic
        }
    }

    function presentAvailableUpdate() {
        const version = String(updateController.latestVersion || "")
        // Multiple WASM deployments can share the same semantic release.
        // Identify the notice by both release version and verified build ID.
        const releaseKey = Qt.platform.os === "wasm"
            ? version + "@" + String(updateController.webLatestBuild || "")
            : version
        if (updateController.updateAvailable && version.length > 0
                && releaseKey !== lastPresentedVersion) {
            lastPresentedVersion = releaseKey
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
    // QML ColumnLayout can advertise the release notes' full implicit height,
    // making a top-edge Drawer grow beyond the viewport and hide its header.
    // An Item is the bounded viewport. Only the middle Flickable may scroll.
    contentItem: Item {
        id: updateLayoutViewport
        implicitHeight: 0
        clip: true
        ColumnLayout {
            id: updateFixedChrome
            objectName: "updateNoticeStickyHeader"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.space2
        // Subtle grabber communicates top-sheet direction on touch screens.
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 40
            Layout.preferredHeight: 4
            radius: 2
            color: Theme.inkMuted
            opacity: 0.48
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            Superellipse {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                fillColor: Theme.successSurface
                curveDepth: Theme.curveControl
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconRefresh
                    preferredSize: 24
                    color: Theme.success
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.update.available")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.ink
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: updateController.releaseTitle.length > 0
                          ? updateController.releaseTitle
                          : qsTrId("genycaster.update.ready")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    elide: Text.ElideRight
                }
            }
            ActionButton {
                compact: true
                iconOnly: true
                quiet: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.update.not_now")
                onClicked: root.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ColumnLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    text: qsTrId("genycaster.update.current")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    text: root.sameSemanticRelease
                          ? qsTrId("genycaster.update.build_label")
                              .arg(LocaleFormat.localizeDigits(updateController.webCurrentBuild.slice(0, 8)))
                          : LocaleFormat.localizeDigits("v" + String(updateController.appVersion || "—"))
                    alignToUi: true
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                }
                DirectionAwareLabel {
                    visible: Qt.platform.os === "wasm"
                    text: root.sameSemanticRelease
                          ? LocaleFormat.localizeDigits("v" + updateController.appVersion)
                          : qsTrId("genycaster.update.build_label")
                              .arg(LocaleFormat.localizeDigits(updateController.webCurrentBuild.slice(0, 8)))
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            Icon {
                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                color: Theme.inkMuted
                preferredSize: Theme.iconSize
            }
            ColumnLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    text: qsTrId("genycaster.update.new")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    text: root.sameSemanticRelease
                          ? qsTrId("genycaster.update.build_label")
                              .arg(LocaleFormat.localizeDigits(updateController.webLatestBuild))
                          : LocaleFormat.localizeDigits("v" + String(updateController.latestVersion || "—"))
                    alignToUi: true
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.accent
                }
                DirectionAwareLabel {
                    visible: Qt.platform.os === "wasm"
                    text: root.sameSemanticRelease
                          ? LocaleFormat.localizeDigits("v" + updateController.latestVersion)
                          : qsTrId("genycaster.update.build_label")
                              .arg(LocaleFormat.localizeDigits(updateController.webLatestBuild))
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }
        }
        ColumnLayout {
            id: updateScrollableSection
            anchors.top: updateFixedChrome.bottom
            anchors.bottom: updateNoticeStickyActions.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: Theme.space3
            anchors.bottomMargin: Theme.space2
            spacing: Theme.space2
            // This section's viewport is bounded by sticky chrome.
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0

            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: updateController.lastInstallError.length > 0
                text: updateController.lastInstallError
                textFormat: Text.PlainText
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.update.whats_new")
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    visible: updateController.releaseType.length > 0
                    text: root.releaseTypeLabel(updateController.releaseType)
                    color: Theme.accent
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    visible: updateController.releasePublishedAt.length > 0
                    text: updateController.releasePublishedAt
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            Flickable {
                objectName: "updateReleaseNotesScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.preferredHeight: 0
                clip: true
                interactive: contentHeight > height
                flickableDirection: Flickable.VerticalFlick
                contentWidth: width
                contentHeight: Math.max(height, releaseNotesText.implicitHeight + Theme.space3)
                boundsBehavior: Flickable.StopAtBounds
                DirectionAwareLabel {
                    id: releaseNotesText
                    width: parent.width
                    text: updateController.releaseNotes.length > 0
                          ? LocaleFormat.localizeDigits(
                                updateController.releaseSummary.length > 0
                                ? updateController.releaseSummary + "\n\n" + updateController.releaseNotes
                                : updateController.releaseNotes)
                          : qsTrId("genycaster.update.no_notes")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: updateNoticeStickyActions.top
            height: 1
            color: Theme.divider
        }
        RowLayout {
            id: updateNoticeStickyActions
            objectName: "updateNoticeStickyActions"
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.max(Theme.scaleUi(48), implicitHeight)
            spacing: Theme.space2
            ActionButton {
                Layout.fillWidth: true
                compact: true
                quiet: true
                visible: Qt.platform.os !== "wasm"
                text: qsTrId("genycaster.update.notes")
                iconGlyph: Theme.iconArrowUpRight
                onClicked: {
                    root.close()
                    updateController.openReleasePage()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                compact: true
                positive: true
                busy: updateController.downloading || updateController.webRefreshing
                iconGlyph: Theme.iconRefresh
                text: Qt.platform.os === "wasm" ? qsTrId("genycaster.update.refresh")
                      : updateController.canDownload
                        ? (updateController.downloadedFilePath.length > 0
                            ? qsTrId("genycaster.update.installer") : qsTrId("genycaster.update.download"))
                        : qsTrId("genycaster.update.view")
                onClicked: {
                    root.close()
                    if (Qt.platform.os === "wasm")
                        updateController.refreshWebApp()
                    else if (updateController.canDownload) {
                        if (updateController.downloadedFilePath.length > 0)
                            updateController.installDownloadedUpdate()
                        else
                            updateController.downloadUpdate()
                    } else
                        updateController.openReleasePage()
                }
            }
        }
    }
}
