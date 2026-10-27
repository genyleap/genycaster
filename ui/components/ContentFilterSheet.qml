import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One touch-first filter editor for native and WASM phones. Values are staged
// until Apply; dismissing the sheet never mutates the active filter.
GenyDrawer {
    id: root
    objectName: "contentFilterSheet"
    property string heading: qsTrId("genycaster.settings.content_loading")
    // Each section: {id, title, options:[{id,label}], value}.
    property var sections: []
    property var draft: ({})
    signal applied(var values)

    function resetDraft() {
        const next = ({})
        for (const section of root.sections || [])
            next[String(section.id)] = String(section.value || "")
        root.draft = next
    }
    function choose(key, value) {
        const next = Object.assign({}, root.draft)
        next[String(key)] = String(value)
        root.draft = next
    }
    function clearDraft() {
        const next = ({})
        for (const section of root.sections || [])
            next[String(section.id)] = String(section.defaultValue || "")
        root.draft = next
    }

    bottomSheet: true
    dimBackground: true
    interactive: false
    width: parent ? parent.width : 380
    height: parent ? Math.min(parent.height * 0.82, 680) : 620
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    contentPadding: Theme.mobileGutter
    onOpened: resetDraft()

    contentItem: ColumnLayout {
        spacing: Theme.space3
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.heading
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
            }
            ActionButton {
                iconOnly: true
                quiet: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.shell.close")
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                onClicked: root.close()
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: GenyScrollBar {}
            ColumnLayout {
                width: parent.width
                spacing: Theme.space4
                Repeater {
                    model: root.sections || []
                    delegate: ColumnLayout {
                        id: sectionGroup
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(sectionGroup.modelData.title || "")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                        }
                        Flow {
                            Layout.fillWidth: true
                            width: parent.width
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Repeater {
                                model: sectionGroup.modelData.options || []
                                delegate: Button {
                                    id: chip
                                    required property var modelData
                                    readonly property string key: String(sectionGroup.modelData.id || "")
                                    readonly property string optionId: String(modelData.id || "")
                                    readonly property bool chosen: String(root.draft[key] || "") === optionId
                                    implicitWidth: Math.max(88, chipText.implicitWidth + Theme.space4 * 2)
                                    implicitHeight: 44
                                    Accessible.name: String(modelData.label || "")
                                    Accessible.role: Accessible.RadioButton
                                    Accessible.checked: chosen
                                    onClicked: root.choose(key, optionId)
                                    background: Superellipse {
                                        fillColor: chip.chosen ? Theme.primarySurface : Theme.surfaceRaised
                                        borderWidth: chip.chosen ? 2 : Theme.strokeHairline
                                        borderColor: chip.chosen ? Theme.accent : Theme.borderDefault
                                        curveDepth: height / 2
                                    }
                                    contentItem: DirectionAwareLabel {
                                        id: chipText
                                        text: String(chip.modelData.label || "")
                                        color: chip.chosen ? Theme.accent : Theme.textPrimary
                                        font.pixelSize: Theme.typeBodySize
                                        font.weight: chip.chosen ? Theme.weightDemiBold : Theme.weightMedium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.divider }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            ActionButton {
                text: "Clear all"
                quiet: true
                Layout.preferredHeight: 48
                onClicked: root.clearDraft()
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "Apply filters"
                prominent: true
                Layout.preferredHeight: 48
                onClicked: {
                    root.applied(Object.assign({}, root.draft))
                    root.close()
                }
            }
        }
    }
}
