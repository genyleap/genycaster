import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shared filter presentation: one selection model, adaptive sheet on phones,
// edge drawer on desktops. Dismiss never commits staged selections.
GenyDrawer {
    id: root
    objectName: "contentFilterSheet"
    property string heading: "Filters"
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

    readonly property bool phoneLayout: parent === null
                                        || parent.width < Theme.breakpointMobile
    bottomSheet: phoneLayout
    dimBackground: true
    interactive: false
    showHandle: phoneLayout
    width: parent ? (phoneLayout ? parent.width
                   : Math.min(464, Math.max(340, parent.width * 0.43))) : 380
    height: parent ? (phoneLayout ? Math.min(parent.height * 0.82, 680)
                     : parent.height) : 620
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    contentPadding: phoneLayout ? Theme.mobileGutter : Theme.space4
    onOpened: resetDraft()

    contentItem: ColumnLayout {
        spacing: Theme.space3
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.heading
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
                elide: Text.ElideRight
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

        // Explicit contentHeight is essential here. The former ScrollView +
        // ColumnLayout + Flow left its Flickable at zero content height on iOS
        // WebAssembly, showing only the header and Apply/Clear footer.
        Flickable {
            id: filterScroller
            objectName: "contentFilterOptions"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 40
            clip: true
            contentWidth: width
            contentHeight: filterGroups.implicitHeight + Theme.space3
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: filterGroups
                width: filterScroller.width
                spacing: Theme.space4

                Repeater {
                    model: root.sections || []
                    delegate: Column {
                        id: sectionGroup
                        required property var modelData
                        width: filterGroups.width
                        spacing: Theme.space3

                        DirectionAwareLabel {
                            width: parent.width
                            text: String(sectionGroup.modelData.title || "")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                        }
                        Flow {
                            width: parent.width
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Repeater {
                                model: sectionGroup.modelData.options || []
                                delegate: Button {
                                    id: chip
                                    required property var modelData
                                    readonly property string filterKey:
                                        String(sectionGroup.modelData.id || "")
                                    readonly property string optionId:
                                        String(modelData.id || "")
                                    readonly property bool chosen:
                                        String(root.draft[filterKey] || "") === optionId
                                    implicitWidth: Math.max(88,
                                        chipText.implicitWidth + Theme.space4 * 2)
                                    implicitHeight: 44
                                    Accessible.name: String(modelData.label || "")
                                    Accessible.role: Accessible.RadioButton
                                    Accessible.checked: chosen
                                    onClicked: root.choose(filterKey, optionId)
                                    background: Superellipse {
                                        fillColor: chip.chosen ? Theme.primarySurface
                                                              : Theme.surfaceRaised
                                        borderWidth: chip.chosen ? 2 : Theme.strokeHairline
                                        borderColor: chip.chosen ? Theme.accent
                                                                 : Theme.borderDefault
                                        curveDepth: height / 2
                                    }
                                    contentItem: DirectionAwareLabel {
                                        id: chipText
                                        text: String(chip.modelData.label || "")
                                        color: chip.chosen ? Theme.accent
                                                           : Theme.textPrimary
                                        font.pixelSize: Theme.typeBodySize
                                        font.weight: chip.chosen
                                                     ? Theme.weightDemiBold
                                                     : Theme.weightMedium
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
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
