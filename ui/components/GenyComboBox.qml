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
            fillColor: control.hovered || control.popup.visible ? Theme.castHover : "transparent"
            curveDepth: Theme.curveCompact
            Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
        }

        Icon {
            anchors.centerIn: parent
            text: Theme.iconChevronDown
            color: control.popup.visible ? Theme.accent : Theme.inkMuted
            preferredSize: Theme.typeCaptionSize
            rotation: control.popup.visible ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic } }
        }
    }

    background: Superellipse {
        fillColor: control.popup.visible ? Theme.surfaceElevated
                  : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        borderWidth: Theme.keyboardFocusVisible(control) || control.popup.visible ? 2 : 1
        borderColor: Theme.keyboardFocusVisible(control) || control.popup.visible ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveControl
        elevation: Theme.elevationControl
        shadowColor: Theme.keyboardFocusVisible(control) || control.popup.visible ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: control.popup.visible ? 1.10 : 0.88
        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
    }

    delegate: ItemDelegate {
        id: optionDelegate
        required property int index
        required property var modelData
        readonly property bool currentOption: optionDelegate.index === control.currentIndex
        width: ListView.view ? ListView.view.width : implicitWidth
        implicitWidth: Math.ceil(contentItem.implicitWidth + Theme.space4 * 2)
        height: 44
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

        background: Superellipse {
            anchors.fill: parent
            anchors.margins: 2
            fillColor: optionDelegate.highlighted || optionDelegate.hovered
                       ? Theme.primarySurfaceHover
                       : optionDelegate.currentOption ? Theme.primarySurface : Theme.surfaceRaised
            curveDepth: Theme.curveCompact
            Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
        }
    }

    popup: Popup {
        y: control.height + Theme.space2
        x: Theme.rtl ? control.width - width : 0
        width: control.naturalPopupWidth
        implicitHeight: Math.min(contentItem.implicitHeight + Theme.space2, 380)
        padding: Theme.space1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
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
