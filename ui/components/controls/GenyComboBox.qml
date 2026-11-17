import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

ComboBox {
    id: control

    property int popupMaximumWidth: 680
    // Display-only translation; currentText remains the protocol/account
    // canonical value for validation and persistence.
    property var optionLabelProvider: function(value) { return value }
    function localizedOption(value) { return String(optionLabelProvider(String(value))) }
    readonly property real measuredTextWidth: {
        let widest = 0
        for (let index = 0; index < optionMetrics.count; ++index) {
            const metric = optionMetrics.itemAt(index)
            const candidate = metric ? metric.implicitWidth : 0
            if (candidate > widest)
                widest = candidate
        }
        return widest
    }
    readonly property real naturalWidth: Math.ceil(measuredTextWidth + Theme.space4 * 2 + Theme.space7)
    readonly property real naturalPopupWidth: Math.min(popupMaximumWidth,
                                                       Math.max(width, naturalWidth + Theme.space3))

    readonly property bool popupVisible: !!control.popup && control.popup.visible

    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(260, Math.min(480, naturalWidth))
    leftPadding: Theme.rtl ? Theme.space8 : Theme.space4
    rightPadding: Theme.rtl ? Theme.space4 : Theme.space8
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    opacity: control.enabled ? 1.0 : 0.58

    Repeater {
        id: optionMetrics
        model: control.model

        delegate: DirectionAwareLabel {
            required property int index
            visible: false
            text: control.localizedOption(control.textAt(index))
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightMedium
        }
    }

    contentItem: DirectionAwareLabel {
        leftPadding: 0
        rightPadding: 0
        text: control.localizedOption(control.displayText)
        color: control.enabled ? Theme.ink : Theme.inkSubtle
        font.pixelSize: Theme.typeLabelSize
        font.weight: Theme.weightMedium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Item {
        x: Theme.rtl ? Theme.space2 : control.width - width - Theme.space2
        y: Math.round((control.height - height) / 2)
        width: 28
        height: 28

        Superellipse {
            anchors.fill: parent
            fillColor: control.hovered || control.popupVisible ? Theme.castHover : "transparent"
            curveDepth: Theme.curveCompact
            Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
        }

        Icon {
            anchors.centerIn: parent
            text: Theme.iconChevronDown
            color: control.popupVisible ? Theme.accent : Theme.inkMuted
            preferredSize: Theme.typeCaptionSize
            rotation: control.popupVisible ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic } }
        }
    }

    background: NeonSurface {
        interactive: control.enabled
        ambientNeon: control.popupVisible
        focused: Theme.keyboardFocusVisible(control) || control.popupVisible
        neonStrength: Theme.neonControlStrength
        neonColor: control.popupVisible
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.34)
            : "transparent"
        fillColor: control.popupVisible ? Theme.surfaceElevated
                  : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        borderWidth: Theme.keyboardFocusVisible(control) || control.popupVisible ? 2 : 1
        borderColor: Theme.keyboardFocusVisible(control) || control.popupVisible ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveControl
        elevation: Theme.elevationControl
        shadowColor: Theme.keyboardFocusVisible(control) || control.popupVisible ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: control.popupVisible ? 1.10 : 0.88
    }

    delegate: ItemDelegate {
        id: optionDelegate
        required property int index
        required property var modelData
        readonly property bool currentOption: optionDelegate.index === control.currentIndex
        width: ListView.view ? ListView.view.width : implicitWidth
        implicitWidth: Math.ceil(contentItem.implicitWidth + Theme.space4 * 2)
        height: 42
        highlighted: control.highlightedIndex === index
        leftPadding: Theme.space3
        rightPadding: Theme.space3
        hoverEnabled: true
        LayoutMirroring.enabled: false

        contentItem: RowLayout {
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: control.localizedOption(control.textAt(optionDelegate.index))
                // Unselected choices must be fully readable, not appear disabled.
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: optionDelegate.currentOption ? Theme.weightDemiBold : Theme.weightMedium
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideNone
            }

            Icon {
                visible: optionDelegate.currentOption
                text: Theme.iconCheck
                color: Theme.accent
                preferredSize: Theme.typeMetaSize
                Layout.alignment: Qt.AlignVCenter
            }
        }

        background: NeonStatePlate {
            anchors.margins: Math.max(1, Theme.space1 / 2)
            selected: optionDelegate.currentOption
            hovered: optionDelegate.highlighted || optionDelegate.hovered
            pressed: optionDelegate.down
            focused: Theme.keyboardFocusVisible(optionDelegate)
            selectedFill: Theme.primarySurface
            hoverFill: Theme.surfaceHover
            pressedFill: Theme.surfacePressed
            cornerRadius: Theme.curveCompact
        }
    }

    popup: Popup {
        y: control.height + Theme.space2
        x: Theme.rtl ? control.width - width : 0
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 286)
        padding: 2
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popupVisible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: Theme.motionFast
            ScrollBar.vertical: GenyScrollBar {}
        }

        background: PopupSurface {
            fillColor: Theme.surfaceElevated
            borderColor: Theme.borderDefault
            curveDepth: Theme.curveContainer
            elevation: Theme.elevationPopover
        }
    }
}
