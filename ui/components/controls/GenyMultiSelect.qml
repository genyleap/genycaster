import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// A reusable multi-select field. Canonical values stay outside the control;
// translations affect labels only. The owner retains control of persistence.
Item {
    id: control
    objectName: "genyMultiSelect"
    property var options: []
    property var selectedValues: []
    property int maximumSelection: 3
    property string placeholderText: ""
    property string accessibleLabel: placeholderText
    property string removeLabel: accessibleLabel
    property var optionLabelProvider: function(value) { return value }
    signal selectionEdited(var nextValues)

    function optionLabel(value) {
        return String(optionLabelProvider(String(value)))
    }
    function isSelected(value) {
        return (selectedValues || []).indexOf(value) >= 0
    }
    function toggleOption(value) {
        const canonical = String(value)
        if (options.indexOf(canonical) < 0)
            return
        const next = (selectedValues || []).slice()
        const existing = next.indexOf(canonical)
        if (existing >= 0)
            next.splice(existing, 1)
        else if (next.length < maximumSelection)
            next.push(canonical)
        else
            return
        selectionEdited(next)
    }
    function openChoices() {
        // Touch-first choices: opening must never request the soft keyboard.
        if (picker.visible) picker.close()
        else picker.open()
    }

    readonly property int previewCount: width < 350 ? 1 : width < 490 ? 2 : 3
    readonly property var visibleValues: (selectedValues || []).slice(0, previewCount)
    readonly property int hiddenCount: Math.max(0, (selectedValues || []).length - visibleValues.length)
    readonly property var availableOptions: options || []

    implicitHeight: 50
    implicitWidth: 310
    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleLabel
    Accessible.description: (selectedValues || []).map(value=>optionLabel(value)).join(", ")
    LayoutMirroring.enabled: false

    NeonSurface {
        anchors.fill: parent
        interactive: openButton.enabled
        ambientNeon: picker.visible
        externalHovered: openButton.hovered
        focused: Theme.keyboardFocusVisible(openButton) || picker.visible
        neonStrength: Theme.neonControlStrength
        neonColor: picker.visible
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
            : "transparent"
        fillColor: picker.visible ? Theme.surfaceElevated : Theme.surfaceRaised
        borderWidth: picker.visible || Theme.keyboardFocusVisible(openButton) ? 2 : 1
        borderColor: picker.visible || Theme.keyboardFocusVisible(openButton)
                     ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveControl
    }

    // A full-size trigger stays clickable even when the selection limit is
    // reached. Selected tags have higher stacking order and remove themselves.
    Button {
        id: openButton
        anchors.fill: parent
        background: Item {}
        contentItem: Item {}
        Accessible.name: control.accessibleLabel
        onClicked: control.openChoices()
    }

    RowLayout {
        id: previewRow
        anchors.fill: parent
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        spacing: Theme.space1
        layoutDirection: Theme.uiDirection

        DirectionAwareLabel {
            visible: !control.selectedValues || control.selectedValues.length === 0
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: control.placeholderText
            color: Theme.inkMuted
            font.pixelSize: Theme.typeLabelSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Repeater {
            model: control.visibleValues
            delegate: Button {
                id: selectionChip
                required property var modelData
                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: 32
                implicitWidth: Math.min(116, chipRow.implicitWidth + Theme.space2 * 2)
                implicitHeight: 32
                Accessible.name: control.removeLabel + ": " + control.optionLabel(modelData)
                onClicked: control.toggleOption(String(modelData))
                background: NeonSurface {
                    ambientNeon: true
                    interactive: selectionChip.enabled
                    neonStrength: Theme.neonControlStrength
                    neonColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                    fillColor: Theme.primarySurface
                    borderColor: Theme.accent
                    borderWidth: Theme.strokeHairline
                    curveDepth: height / 2
                }
                contentItem: RowLayout {
                    id: chipRow
                    spacing: Theme.space1
                    layoutDirection: Theme.uiDirection
                    Icon {
                        text: Theme.iconClose
                        color: Theme.accent
                        preferredSize: Theme.typeCaptionSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: control.optionLabel(selectionChip.modelData)
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Superellipse {
            visible: control.hiddenCount > 0
            Layout.preferredWidth: 35
            Layout.preferredHeight: 28
            fillColor: Theme.primarySurface
            curveDepth: 14
            DirectionAwareLabel {
                anchors.centerIn: parent
                text: "+" + control.hiddenCount
                color: Theme.accent
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightBold
            }
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

        Icon {
            text: Theme.iconChevronDown
            color: picker.visible ? Theme.accent : Theme.inkMuted
            rotation: picker.visible ? 180 : 0
            preferredSize: Theme.iconSize
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Behavior on rotation {
                NumberAnimation { duration: Theme.motionFast }
            }
        }
    }

    Popup {
        id: picker
        objectName: "genyMultiSelectPopup"
        parent: control
        x: 0
        y: {
            const overlay=control.Overlay.overlay
            if (!overlay) return control.height + Theme.space1
            const globalPos=control.mapToItem(overlay, 0, 0)
            return globalPos.y + control.height + implicitHeight + Theme.space2
                   > overlay.height
                   ? -implicitHeight - Theme.space1 : control.height + Theme.space1
        }
        width: control.width
        implicitHeight: Math.min(310, popupContent.implicitHeight + Theme.space3 * 2)
        padding: Theme.space3
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        // There is deliberately no text editor: selecting categories must
        // not cause the phone's keyboard to hide the profile form.
        background: PopupSurface {
            fillColor: Theme.surfaceElevated
            borderColor: Theme.borderDefault
            curveDepth: Theme.curveContainer
            elevation: Theme.elevationPopover
        }

        contentItem: ColumnLayout {
            id: popupContent
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: (control.selectedValues || []).length
                      + "/" + control.maximumSelection
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
            }

            GridView {
                id: availableList
                objectName: "genyMultiSelectChoices"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(214,
                    Math.ceil(control.availableOptions.length / 2) * cellHeight)
                clip: true
                model: control.availableOptions
                cellWidth: Math.max(1, width / 2)
                cellHeight: 46
                layoutDirection: Theme.uiDirection
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: GenyScrollBar {}

                delegate: Button {
                    id: optionButton
                    required property var modelData
                    readonly property string canonicalValue: String(modelData)
                    readonly property bool selected: control.isSelected(canonicalValue)
                    width: availableList.cellWidth - Theme.space1
                    height: 44
                    enabled: selected
                             || control.selectedValues.length < control.maximumSelection
                    hoverEnabled: true
                    Accessible.role: Accessible.CheckBox
                    Accessible.checked: selected
                    Accessible.name: control.optionLabel(canonicalValue)
                    onClicked: control.toggleOption(canonicalValue)
                    background: Superellipse {
                        fillColor: optionButton.down || optionButton.hovered
                                   ? Theme.surfaceHover : "transparent"
                        curveDepth: Theme.curveCompact
                    }
                    contentItem: RowLayout {
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        Superellipse {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            fillColor: optionButton.selected ? Theme.accent
                                                               : Theme.surfaceRaised
                            borderColor: optionButton.selected ? Theme.accent
                                                                : Theme.borderDefault
                            borderWidth: 1
                            curveDepth: 6
                            Icon {
                                anchors.centerIn: parent
                                visible: optionButton.selected
                                text: Theme.iconCheck
                                color: Theme.accentInk
                                preferredSize: 13
                            }
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: control.optionLabel(optionButton.canonicalValue)
                            color: optionButton.enabled ? Theme.textPrimary
                                                        : Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
