import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// An unmistakable install/discovery invitation, shared by guest Explore and
// onboarding. Never suggests that an unverified native binary is downloadable:
// the first-party release dialog is authoritative for every OS/architecture.
// After login this card is absent; Settings retains the quiet discovery entry.
NeonSurface {
    id: root
    objectName: "nativeAppDiscoveryCTA"

    property bool showSignIn: false
    property bool dense: false
    readonly property bool narrow: width < Theme.scaleUi(440)

    signal downloadsRequested()
    signal signInRequested()

    implicitHeight: content.implicitHeight + 2 * (dense ? Theme.space3 : Theme.space4)
    height: implicitHeight
    fillColor: Theme.primarySurface
    borderWidth: Theme.strokeStandard
    borderColor: Theme.accent
    curveDepth: Theme.curveContainer
    ambientNeon: true
    neonStrength: Theme.neonCardStrength
    neonColor: Theme.accent
    interactive: false

    ColumnLayout {
        id: content
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.dense ? Theme.space3 : Theme.space4
        layoutDirection: Theme.uiDirection
        spacing: root.dense ? Theme.space2 : Theme.space3

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space3

            Superellipse {
                Layout.preferredWidth: root.dense ? 40 : 48
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignTop
                curveDepth: Theme.curveCompact
                fillColor: Theme.surface
                borderWidth: Theme.strokeHairline
                borderColor: Theme.accent
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconDownload
                    preferredSize: root.dense ? Theme.iconSize : Theme.iconSizeLarge
                    color: Theme.accent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.download_cta_eyebrow")
                    color: Theme.accent
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightBold
                    wrapMode: Text.WordWrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.download_cta_title")
                    color: Theme.ink
                    font.pixelSize: root.dense || root.narrow
                                    ? Theme.typeTitle3Size : Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    wrapMode: Text.WordWrap
                    lineHeight: 1.1
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.apps.download_cta_description")
            color: Theme.inkMuted
            font.pixelSize: root.dense ? Theme.typeMetaSize : Theme.typeLabelSize
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.showSignIn && !root.narrow ? 2 : 1
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            uniformCellWidths: root.showSignIn && !root.narrow

            ActionButton {
                objectName: "nativeAppDiscoveryExplore"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                prominent: true
                iconGlyph: Theme.iconDownload
                text: qsTrId("genycaster.apps.download_cta_button")
                onClicked: root.downloadsRequested()
            }
            ActionButton {
                objectName: "nativeAppDiscoverySignIn"
                visible: root.showSignIn
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                quiet: true
                iconGlyph: Theme.iconSignIn
                text: qsTrId("genycaster.header.sign_in")
                onClicked: root.signInRequested()
            }
        }
    }
}
