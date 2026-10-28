import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Product-owned adaptive filter editor. Selections are staged until Apply.
// Group cards share the same semantics on mobile (sheet) and desktop (drawer).
GenyDrawer {
    id: root
    objectName: "contentFilterSheet"
    property string heading: qsTrId("genycaster.filters.title")
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
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.heading
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.filters.hint")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
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

        // Give the content area an explicit height contract; a nested
        // ScrollView/ColumnLayout previously yielded a blank mobile sheet.
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
                spacing: Theme.space3

                Repeater {
                    model: root.sections || []
                    delegate: Superellipse {
                        id: sectionGroup
                        required property var modelData
                        required property int index
                        width: filterGroups.width
                        height: sectionContent.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.curveContainer

                        Column {
                            id: sectionContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: Theme.space3
                            anchors.rightMargin: Theme.space3
                            anchors.topMargin: Theme.space3
                            spacing: Theme.space3

                            RowLayout {
                                width: parent.width
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2

                                Superellipse {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    fillColor: Theme.primarySurface
                                    curveDepth: 12
                                    DirectionAwareLabel {
                                        anchors.centerIn: parent
                                        text: LocaleFormat.formatInteger(sectionGroup.index + 1)
                                        color: Theme.accent
                                        font.pixelSize: Theme.typeMetaSize
                                        font.weight: Theme.weightBold
                                    }
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: String(sectionGroup.modelData.title || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                                Icon {
                                    text: Theme.iconCheck
                                    color: Theme.accent
                                    preferredSize: Theme.iconSize
                                }
                            }
                            DirectionAwareLabel {
                                width: parent.width
                                visible: String(sectionGroup.modelData.hint || "").length > 0
                                text: String(sectionGroup.modelData.hint || "")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.WordWrap
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
                                            elide: Text.ElideRight
                                        }
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
                text: qsTrId("genycaster.filters.clear_all")
                quiet: true
                Layout.preferredHeight: 48
                onClicked: root.clearDraft()
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: qsTrId("genycaster.filters.apply")
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
