import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string currentMode: "for-you"
    signal modeRequested(string mode)

    implicitHeight: 48

    readonly property var tabModel: [
        { id: "for-you", label: qsTrId("genycaster.home.for_you") },
        { id: "visuals", label: qsTrId("genycaster.home.visual_filter_nfts") },
        { id: "conversations", label: qsTrId("genycaster.home.conversations") },
        { id: "devs", label: qsTrId("genycaster.home.devs") }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: Theme.space1
        layoutDirection: Theme.uiDirection

        Repeater {
            model: root.tabModel

            delegate: Item {
                id: tabButton
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property bool selected: String(modelData.id) === root.currentMode

                HoverHandler { id: hover }

                Superellipse {
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    fillColor: tabButton.selected ? Theme.surfaceSelected
                               : hover.hovered ? Theme.surfaceHover : "transparent"
                    borderWidth: tabButton.selected ? Theme.strokeHairline : 0
                    borderColor: tabButton.selected ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveControl
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    text: String(tabButton.modelData.label || "")
                    color: tabButton.selected ? Theme.accent
                                              : (hover.hovered ? Theme.ink : Theme.inkMuted)
                    font.family: Theme.fontFamily
                    font.pixelSize: root.width < 400 ? 12 : Theme.typeMetaSize
                    font.weight: tabButton.selected ? Theme.weightBold : Theme.weightMedium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }

                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.modeRequested(String(tabButton.modelData.id || ""))
                }
            }
        }
    }
}
