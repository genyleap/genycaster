import QtQuick
import QtQuick.Layouts

// One persistent indicator travels between stable tab targets. Selected text
// never changes weight or geometry, so rapid switching cannot flicker labels.
Item {
    id: root
    property string currentMode: "for-you"
    property bool motionReady: false
    signal modeRequested(string mode)
    implicitHeight: 48
    readonly property var tabModel: [
        { id: "for-you", label: qsTrId("genycaster.home.for_you") },
        { id: "visuals", label: qsTrId("genycaster.home.visual_filter_nfts") },
        { id: "conversations", label: qsTrId("genycaster.home.conversations") },
        { id: "devs", label: qsTrId("genycaster.home.devs") }
    ]
    readonly property int selectedIndex: {
        for (let i = 0; i < tabModel.length; ++i)
            if (tabModel[i].id === currentMode) return i
        return 0
    }
    Component.onCompleted: motionReady = true

    RowLayout {
        id: tabs
        anchors.fill: parent
        spacing: Theme.space1
        layoutDirection: Theme.uiDirection

        Repeater {
            id: tabRepeater
            model: root.tabModel
            delegate: Item {
                id: tabButton
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: String(modelData.id) === "conversations" ? 1.65 : 1
                Layout.fillHeight: true
                readonly property bool selected: String(modelData.id) === root.currentMode
                Accessible.role: Accessible.Tab
                Accessible.name: String(modelData.label)
                Accessible.selected: selected

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.space2
                    color: hover.hovered && !tabButton.selected
                           ? Theme.surfaceHover : "transparent"
                    Behavior on color {
                        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
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
                            duration: Theme.reduceMotion ? 0 : Theme.motionFast
                            easing.type: Easing.InOutCubic
                        }
                    }
                    font.family: Theme.fontFamily
                    font.pixelSize: root.width < 400 ? 12 : Theme.typeMetaSize
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 11
                    elide: Text.ElideRight
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: {
                        if (!tabButton.selected)
                            root.modeRequested(String(tabButton.modelData.id || ""))
                    }
                }
            }
        }
    }
    Rectangle {
        id: activeUnderline
        z: 3
        y: root.height - 3
        height: 3
        width: 36
        radius: 1.5
        color: Theme.accent
        x: {
            const item = tabRepeater.itemAt(root.selectedIndex)
            return item ? item.x + (item.width - width) / 2 : 0
        }
        Behavior on x {
            enabled: root.motionReady && !Theme.reduceMotion
            NumberAnimation {
                duration: Theme.motionStandard
                easing.type: Easing.OutCubic
            }
        }
    }
}
