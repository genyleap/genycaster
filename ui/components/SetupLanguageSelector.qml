pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// A single compact language field, rather than a full-width multi-row card.
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

    implicitHeight: pickerRow.implicitHeight + Theme.space3 * 2
    LayoutMirroring.enabled: false
    Accessible.name: qsTrId("genycaster.settings.language_region")
    Accessible.description: selectedLabel

    // Show confirmation only after a change, without taking permanent
    // vertical space in the initial Setup stage.
    // i18n reference for the optional accessibility hint:
    property string changedHint: userInteracted
                                 ? qsTrId("genycaster.setup.language_applies_now") : ""

    Superellipse {
        anchors.fill: parent
        fillColor: Theme.surface
        borderColor: Theme.borderMuted
        borderWidth: Theme.strokeStandard
        curveDepth: Theme.curveContainer
    }

    RowLayout {
        id: pickerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection
        Icon {
            text: Theme.iconLanguage
            color: Theme.accent
            preferredSize: Theme.iconSize
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            DirectionAwareLabel {
                text: qsTrId("genycaster.locale.language")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightMedium
            }
            DirectionAwareLabel {
                text: root.currentEntry ? root.currentEntry.hello : ""
                color: Theme.ink
                font.family: Theme.fontFamilyForText(text)
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
            }
        }
        GenyComboBox {
            id: languageCombo
            Layout.preferredWidth: Math.min(205, Math.max(118, root.width * 0.45))
            model: root.model
            textRole: "language"
            valueRole: "tag"
            popupMaximumWidth: 420
            Accessible.name: qsTrId("genycaster.locale.language")
            onActivated: index => {
                if (index < 0 || index >= root.itemCount) return
                root.userInteracted = true
                root.activated(index, root.model[index])
            }
        }
    }
}
