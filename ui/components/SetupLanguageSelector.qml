pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Focused, native-language picker: one control, one selection summary, no carousel.
Item {
    id: root

    property var model: []
    property alias currentIndex: languageCombo.currentIndex
    property string selectedLabel: ""
    property string selectedDetail: ""
    property bool userInteracted: false
    signal activated(int index, var entry)

    readonly property int itemCount: model && model.length !== undefined ? model.length : 0
    readonly property var currentEntry: currentIndex >= 0 && currentIndex < itemCount
                                        ? model[currentIndex] : null

    implicitHeight: pickerLayout.implicitHeight
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    Accessible.name: qsTrId("genycaster.settings.language_region")
    Accessible.description: selectedLabel

    ColumnLayout {
        id: pickerLayout
        anchors.fill: parent
        spacing: Theme.space3

        Superellipse {
            id: pickerCard
            Layout.fillWidth: true
            implicitHeight: cardContent.implicitHeight + Theme.space4 * 2
            fillColor: Theme.surface
            borderColor: Theme.borderMuted
            borderWidth: Theme.strokeStandard
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: cardContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space4
                spacing: Theme.space3

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Icon {
                        text: Theme.iconLanguage
                        color: Theme.accent
                        preferredSize: Theme.iconSize
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.locale.language")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                }

                GenyComboBox {
                    id: languageCombo
                    Layout.fillWidth: true
                    model: root.model
                    textRole: "language"
                    valueRole: "tag"
                    popupMaximumWidth: 540
                    Accessible.name: qsTrId("genycaster.locale.language")
                    onActivated: index => {
                        if (index < 0 || index >= root.itemCount)
                            return
                        root.userInteracted = true
                        root.activated(index, root.model[index])
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.currentEntry ? root.currentEntry.hello : ""
                    color: Theme.inkMuted
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightMedium
                    horizontalAlignment: Theme.textStartAlignment
                }
            }
        }

        DirectionAwareLabel {
            visible: root.userInteracted
            Layout.fillWidth: true
            text: qsTrId("genycaster.setup.language_applies_now")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Theme.textStartAlignment
        }
    }
}
