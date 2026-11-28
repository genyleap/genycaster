import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

/**
 * @brief Lightweight secondary-filter menu for feed and profile surfaces.
 *
 * Keeps the trigger visually subordinate to primary navigation while preserving
 * keyboard-accessible ComboBox semantics and a content-sized popup.
 */
ComboBox {
    id: control

    property int minimumPopupWidth: 112
    property int maximumPopupWidth: 172
    property int maximumTriggerWidth: 136

    readonly property real widestOptionWidth: {
        let widest = 0
        for (let index = 0; index < control.count; ++index)
            widest = Math.max(widest, optionMetrics.advanceWidth(control.textAt(index)))
        return widest
    }
    readonly property real currentTextWidth: currentMetrics.advanceWidth
    readonly property real naturalTriggerWidth: Math.ceil(currentTextWidth + Theme.space2 * 2 + 22)

    readonly property bool popupVisible: !!control.popup && control.popup.visible

    implicitHeight: 26
    implicitWidth: Math.max(74, Math.min(maximumTriggerWidth, naturalTriggerWidth))
    leftPadding: Theme.rtl ? 24 : Theme.space2
    rightPadding: Theme.rtl ? Theme.space2 : 24
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    LayoutMirroring.enabled: false
    opacity: enabled ? 1.0 : 0.52

    TextMetrics {
        id: currentMetrics
        text: control.displayText
        font.pixelSize: Theme.typeCaptionSize
        font.weight: Theme.weightMedium
    }

    FontMetrics {
        id: optionMetrics
        font.pixelSize: Theme.typeMetaSize
        font.weight: Theme.weightMedium
    }

    contentItem: DirectionAwareLabel {
        text: control.displayText
        color: Theme.textPrimary
        font.pixelSize: Theme.typeCaptionSize
        font.weight: Theme.weightMedium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: Theme.rtl ? Theme.space2 : control.width - width - Theme.space2
        y: Math.round((control.height - height) / 2)
        text: Theme.iconChevronDown
        color: control.popupVisible ? Theme.accent : Theme.textTertiary
        preferredSize: 10
        rotation: control.popupVisible ? 180 : 0
        Behavior on rotation {
            NumberAnimation {
                duration: Theme.reduceMotion ? 0 : Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }

    background: Superellipse {
        fillColor: control.popupVisible ? Theme.surfaceElevated
                  : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: Theme.keyboardFocusVisible(control) || control.popupVisible ? Theme.accent : Theme.borderDefault
        curveDepth: height / 2
        elevation: Theme.elevationNone
    }

    delegate: ItemDelegate {
        id: option
        required property int index
        width: ListView.view ? ListView.view.width : implicitWidth
        height: 28
        leftPadding: Theme.space2
        rightPadding: Theme.space2
        topPadding: 0
        bottomPadding: 0
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: RowLayout {
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: control.textAt(option.index)
                color: option.index === control.currentIndex ? Theme.accent : Theme.textPrimary
                font.pixelSize: Theme.typeMetaSize
                font.weight: option.index === control.currentIndex ? Theme.weightDemiBold : Theme.weightMedium
                elide: Text.ElideRight
            }

            Icon {
                visible: option.index === control.currentIndex
                text: Theme.iconCheck
                color: Theme.accent
                preferredSize: 11
            }
        }

        background: NeonStatePlate {
            anchors.margins: 2
            selected: option.index === control.currentIndex
            hovered: option.highlighted || option.hovered
            pressed: option.down
            focused: option.activeFocus
            selectedFill: Theme.primarySurface
            hoverFill: Theme.primarySurfaceHover
            pressedFill: Theme.surfacePressed
            neonOnHover: false
            cornerRadius: Theme.curveCompact
        }
    }

    popup: Popup {
        y: control.height + 3
        x: Theme.rtl ? 0 : control.width - width
        width: Math.max(control.minimumPopupWidth,
                        Math.min(control.maximumPopupWidth,
                                 Math.ceil(control.widestOptionWidth + Theme.space3 + 20)))
        implicitHeight: Math.min(contentItem.implicitHeight + Theme.space1 * 2, 196)
        padding: Theme.space1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            // Match the general ComboBox: hidden option rows are not part
            // of the feed's construction budget.
            model: control.popupVisible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollBar.vertical: GenyScrollBar {}
        }

        background: PopupSurface {
            fillColor: Theme.surfaceElevated
            borderColor: Theme.borderDefault
            curveDepth: Theme.curveCompact
            elevation: Theme.elevationPopover
        }
    }
}
