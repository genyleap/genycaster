import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool expanded: false
    property string labelText: qsTrId("genycaster.header.new_cast")
    property string iconGlyph: Theme.iconCompose
    property bool replyMode: false

    signal composeRequested()
    signal replyRequested()

    readonly property real expandedWidth: Math.min(220, Math.max(144, contentRow.implicitWidth + Theme.space5))
    implicitWidth: expanded ? expandedWidth : 58
    implicitHeight: 58
    width: implicitWidth
    height: implicitHeight
    hoverEnabled: true
    containmentMask: background
    Accessible.name: control.labelText
    leftPadding: expanded ? Theme.space3 : 0
    rightPadding: expanded ? Theme.space3 : 0
    topPadding: 0
    bottomPadding: 0
    onClicked: control.replyMode ? replyRequested() : composeRequested()
    scale: control.down ? 0.975 : 1.0
    opacity: control.enabled ? 1.0 : 0.55

    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    Behavior on implicitWidth { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic } }

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                text: control.iconGlyph
                color: Theme.accentInk
                preferredSize: Theme.iconSizeLarge
            }

            DirectionAwareLabel {
                visible: control.expanded
                anchors.verticalCenter: parent.verticalCenter
                text: control.labelText
                color: Theme.accentInk
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
        }
    }

    background: Superellipse {
        fillColor: control.down ? Theme.primaryPressed
                  : control.hovered ? Theme.primaryFillHover
                  : Theme.primaryFill
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : Theme.strokeHairline
        borderColor: Theme.keyboardFocusVisible(control) ? Theme.focusOnAccent : Qt.rgba(Theme.accentInk.r, Theme.accentInk.g, Theme.accentInk.b, 0.16)
        curveDepth: control.height / 2
        exponent: Theme.superellipseExponent
        highQualityEdges: true
        elevation: Theme.elevationPopover
        shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: control.down ? 0.40 : (control.hovered ? 1.08 : 0.94)
    }

    onHoveredChanged: {
        if (control.hovered && !control.expanded)
            tooltipDelay.restart()
        else {
            tooltipDelay.stop()
            composeTooltip.close()
        }
    }
    onExpandedChanged: if (control.expanded) composeTooltip.close()

    Timer {
        id: tooltipDelay
        interval: 420
        onTriggered: {
            if (!control.hovered || control.expanded || !control.window) return
            const point = control.mapToItem(Overlay.overlay, 0, 0)
            composeTooltip.x = Math.max(Theme.space2,
                                        Math.min(Overlay.overlay.width - composeTooltip.width - Theme.space2,
                                                 point.x + (control.width - composeTooltip.width) / 2))
            composeTooltip.y = Math.max(Theme.space2, point.y - composeTooltip.height - Theme.space2)
            composeTooltip.open()
        }
    }

    Popup {
        id: composeTooltip
        parent: Overlay.overlay
        modal: false
        focus: false
        padding: Theme.space2
        closePolicy: Popup.NoAutoClose
        background: PopupSurface { curveDepth: Theme.curveCompact; elevation: Theme.elevationPopover }
        contentItem: DirectionAwareLabel {
            text: control.labelText
            color: Theme.textPrimary
            font.pixelSize: Theme.typeCaptionSize
        }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionFast } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionFast } }
    }
}
