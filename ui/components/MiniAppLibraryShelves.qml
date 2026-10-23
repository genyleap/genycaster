import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Genuine, local use history and saved Mini Apps; never invent activity or rank.
Item {
    id: root
    property var recentApps: []
    property var savedApps: []
    readonly property int savedColumns: 1
    signal openRequested(var app) // Tapping an icon opens the details sheet.
    signal launchRequested(var app) // Explicit Open button starts the app.
    signal exploreRequested()

    implicitHeight: shelves.implicitHeight
    ColumnLayout {
        id: shelves
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Theme.space4

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.recentApps && root.recentApps.length > 0
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.apps.recently_used")
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
                color: Theme.ink
            }
            ListView {
                id: recentStrip
                Layout.fillWidth: true
                Layout.preferredHeight: 124
                orientation: ListView.Horizontal
                layoutDirection: Theme.uiDirection
                interactive: contentWidth > width
                spacing: Theme.space3
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                model: root.recentApps
                delegate: Item {
                    id: recentCell
                    required property var modelData
                    width: 92
                    height: 120
                    readonly property var app: recentCell.modelData || ({})
                    SuperellipseImage {
                        id: recentIcon
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 78
                        height: 78
                        source: String(recentCell.app.iconSource || "")
                        fillMode: Image.PreserveAspectFit
                        mediaCategory: "miniapps"
                        forceAutomaticDownload: true
                        deferNewLoadsWhileScrolling: false
                        framePacedPresentation: false
                        showLoadingIndicator: false
                        showFailureIndicator: false
                        showManualLoadIndicator: false
                        requestedSourceWidth: 96
                        requestedSourceHeight: 96
                        curveDepth: Theme.curveControl
                        fillColor: Theme.surfaceRaised
                        MiniAppArtPlaceholder {
                            anchors.fill: parent
                            imageReady: recentIcon.ready
                            loadFailed: recentIcon.loadFailed
                            hasSource: String(recentCell.app.iconSource || "").length > 0
                        }
                    }
                    DirectionAwareLabel {
                        anchors.top: recentIcon.bottom
                        anchors.topMargin: Theme.space2
                        anchors.left: parent.left
                        anchors.right: parent.right
                        text: String(recentCell.app.name || recentCell.app.domain || "")
                        color: Theme.ink
                        font.pixelSize: Theme.typeCaptionSize
                        maximumLineCount: 1
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                    TapHandler { onTapped: root.openRequested(recentCell.app) }
                    Accessible.role: Accessible.Button
                    Accessible.name: String(recentCell.app.name || "")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.saved")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                    color: Theme.ink
                }
                ActionButton {
                    visible: !root.savedApps || root.savedApps.length === 0
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.side_nav.explorer")
                    onClicked: root.exploreRequested()
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: root.savedColumns
                rowSpacing: Theme.space3
                columnSpacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Repeater {
                    model: root.savedApps || []
                    delegate: Item {
                        id: savedCell
                        required property var modelData
                        readonly property var app: savedCell.modelData || ({})
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredHeight: 106
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Theme.strokeHairline
                            color: Theme.divider
                        }
                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Item {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                SuperellipseImage {
                                    id: savedIcon
                                    anchors.fill: parent
                                    source: String(savedCell.app.iconSource || "")
                                    mediaCategory: "miniapps"
                                    forceAutomaticDownload: true
                                    deferNewLoadsWhileScrolling: false
                                    framePacedPresentation: false
                                    showLoadingIndicator: false
                                    showFailureIndicator: false
                                    showManualLoadIndicator: false
                                    fillMode: Image.PreserveAspectFit
                                    requestedSourceWidth: 112
                                    requestedSourceHeight: 112
                                    curveDepth: Theme.curveControl
                                    fillColor: Theme.surfaceRaised
                                }
                                MiniAppArtPlaceholder {
                                    anchors.fill: parent
                                    imageReady: savedIcon.ready
                                    loadFailed: savedIcon.loadFailed
                                    hasSource: String(savedCell.app.iconSource || "").length > 0
                                }
                                TapHandler { onTapped: root.openRequested(savedCell.app) }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(savedCell.app.name || savedCell.app.domain || "")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                    TapHandler { onTapped: root.openRequested(savedCell.app) }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: LocaleFormat.formatMiniAppCategory(savedCell.app.category)
                                         || String(savedCell.app.domain || "")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(savedCell.app.description || "")
                                    color: Theme.inkSubtle
                                    font.pixelSize: Theme.typeMetaSize
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }
                            ActionButton {
                                objectName: "savedMiniAppOpenButton"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 72
                                compact: true
                                pill: true
                                elevated: false
                                text: qsTrId("genycaster.apps.open")
                                onClicked: root.launchRequested(savedCell.app)
                            }
                        }
                    }
                }
            }
            DirectionAwareLabel {
                visible: !root.savedApps || root.savedApps.length === 0
                Layout.fillWidth: true
                text: qsTrId("genycaster.apps.my_empty_description")
                font.pixelSize: Theme.typeMetaSize
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
        }
    }
}
