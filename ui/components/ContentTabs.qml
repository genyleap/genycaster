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
                Layout.preferredWidth: String(modelData.id) === "conversations" ? 1.65 : 1
                Layout.fillHeight: true
                readonly property bool selected: String(modelData.id) === root.currentMode

                HoverHandler { id: hover }

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.space2
                    color: hover.hovered ? Theme.surfaceHover : "transparent"
                    Behavior on color {
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    text: String(tabButton.modelData.label || "")
                    color: tabButton.selected ? Theme.accent
                                              : (hover.hovered ? Theme.ink : Theme.inkMuted)
                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.motionFast
                            easing.type: Easing.InOutCubic
                        }
                    }
                    font.family: Theme.fontFamily
                    font.pixelSize: root.width < 400 ? 12 : Theme.typeMetaSize
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 11
                    elide: Text.ElideRight
                    font.weight: tabButton.selected ? Theme.weightBold : Theme.weightMedium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(36, parent.width - Theme.space3)
                    height: 3
                    radius: 1.5
                    color: Theme.accent
                    opacity: tabButton.selected ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.motionFast }
                    }
                }

                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.modeRequested(String(tabButton.modelData.id || ""))
                }
            }
        }
    }
}
