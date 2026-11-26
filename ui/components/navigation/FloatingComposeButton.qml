import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control
    readonly property bool browserEditorOccludes: true

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
    HoverHandler {
        id: composePointer
        enabled: Theme.neonEnabled
        blocking: false
    }
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
        // The old free-width Row could paint the label outside the pill
        // while the button animated between icon-only and expanded sizes.
        clip: true

        RowLayout {
            id: contentRow
            anchors.fill: parent
            anchors.leftMargin: control.expanded ? Theme.space3 : Theme.space1
            anchors.rightMargin: anchors.leftMargin
            spacing: control.expanded ? Theme.space2 : 0
            layoutDirection: Theme.uiDirection

            Item { Layout.fillWidth: !control.expanded; visible: !control.expanded }

            Icon {
                Layout.alignment: Qt.AlignVCenter
                text: control.iconGlyph
                color: Theme.accentInk
                preferredSize: Theme.iconSizeLarge
            }

            DirectionAwareLabel {
                visible: control.expanded
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                text: control.labelText
                color: Theme.accentInk
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Item { Layout.fillWidth: !control.expanded; visible: !control.expanded }
        }
    }

    background: NeonSurface {
        interactive: control.enabled
        ambientNeon: true
        inverted: !Theme.dark
        externalHovered: control.hovered
        externalPointerX: composePointer.hovered ? composePointer.point.position.x : width / 2
        externalPointerY: composePointer.hovered ? composePointer.point.position.y : height / 2
        neonStrength: Math.min(1, Theme.neonCardStrength * 1.18)
        ambientStrength: Math.min(0.80, Theme.neonAmbientStrength * 1.9)
        neonColor: Qt.lighter(Theme.primaryFill, 1.28)
        fillColor: control.down ? Theme.primaryPressed
                  : control.hovered ? Theme.primaryFillHover
                  : Theme.primaryFill
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : Theme.strokeHairline
        borderColor: Theme.keyboardFocusVisible(control)
                     ? Qt.lighter(Theme.primaryFill, 1.28)
                     : Qt.rgba(Theme.primaryFill.r, Theme.primaryFill.g,
                               Theme.primaryFill.b, control.hovered ? 0.95 : 0.45)
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
