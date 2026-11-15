import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool selected: false
    property string iconGlyph: ""
    property string badgeText: ""
    property bool badgeDot: false
    readonly property color foregroundColor: !control.enabled ? Theme.inkDisabled
                                              : control.selected ? Theme.textPrimary
                                              : Theme.textSecondary

    implicitHeight: 46
    implicitWidth: 218
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    HoverHandler {
        id: navPointer
        enabled: Theme.neonEnabled
        blocking: false
    }
    containmentMask: background
    Accessible.name: text
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    scale: control.down ? 0.988 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: RowLayout {
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            fillColor: control.selected ? Theme.primarySurfaceHover
                       : control.hovered ? Theme.surfaceHover : "transparent"
            curveDepth: Theme.curveCompact

            Icon {
                anchors.centerIn: parent
                text: control.iconGlyph
                filled: control.selected
                color: control.foregroundColor
                preferredSize: Theme.iconSizeNavigation
            }
        }

        Label {
            Layout.fillWidth: true
            text: control.text
            color: control.foregroundColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeLabelSize
            font.weight: control.selected ? Theme.weightDemiBold : Theme.weightMedium
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Theme.textStartAlignment
            elide: Text.ElideRight
        }

        Superellipse {
            visible: control.badgeText.length > 0
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Math.max(22, badgeLabel.implicitWidth + 12)
            Layout.preferredHeight: 20
            curveDepth: 10
            fillColor: control.enabled ? Theme.danger : Theme.borderMuted
            borderWidth: 0

            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: control.badgeText
                color: control.enabled ? Theme.dangerInk : Theme.inkDisabled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Superellipse {
            visible: control.badgeDot
            Layout.preferredWidth: 9
            Layout.preferredHeight: 9
            Layout.alignment: Qt.AlignVCenter
            fillColor: Theme.danger
            curveDepth: 4.5
            Accessible.ignored: true
        }
    }

    background: NeonStatePlate {
        selected: control.selected
        hovered: control.hovered
        pressed: control.down
        focused: Theme.keyboardFocusVisible(control)
        selectedFill: Theme.primarySurface
        hoverFill: Theme.castHover
        pressedFill: Theme.surfacePressed
        cornerRadius: Theme.curveControl
        pointerX: navPointer.hovered ? navPointer.point.position.x : width / 2
        pointerY: navPointer.hovered ? navPointer.point.position.y : height / 2
    }
}
