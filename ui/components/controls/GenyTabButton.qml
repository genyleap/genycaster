import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool selected: false
    // Optional semantic icon; legacy tabs remain unchanged when omitted.
    property string iconGlyph: ""
    property real minimumWidth: 72
    property bool dense: false
    property bool segmented: true
    property bool showSelectionIndicator: true

    implicitHeight: 40
    implicitWidth: Math.max(control.minimumWidth, contentItem.implicitWidth + Theme.space4)
    hoverEnabled: true
    containmentMask: background
    flat: true
    scale: control.down ? 0.988 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: Item {
        implicitWidth: tabContent.implicitWidth
        implicitHeight: tabContent.implicitHeight
        RowLayout {
            id: tabContent
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width)
            height: parent.height
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection
            Icon {
                visible: control.iconGlyph.length > 0
                text: control.iconGlyph
                Layout.preferredWidth: visible ? Theme.scaleUi(16) : 0
                Layout.preferredHeight: Theme.scaleUi(16)
                Layout.alignment: Qt.AlignVCenter
                preferredSize: Theme.scaleUi(15)
                color: control.selected ? (control.segmented ? Theme.ink : Theme.accent)
                                        : (control.hovered ? Theme.ink : Theme.inkMuted)
                Behavior on color {
                    ColorAnimation { duration: Theme.reduceMotion ? 0 : 110 }
                }
            }
            Label {
                Layout.fillWidth: true
                text: control.text
                color: control.selected ? (control.segmented ? Theme.ink : Theme.accent)
                                        : (control.hovered ? Theme.ink : Theme.inkMuted)
                Behavior on color {
                    ColorAnimation { duration: Theme.reduceMotion ? 0 : 110 }
                }
                font.family: Theme.fontFamily
                font.pixelSize: control.dense ? Theme.typeCaptionSize : Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }

    background: Superellipse {
        // Hover must never create a second painted tab. The selected
        // surface is stable; nonselected tabs remain transparent.
        fillColor: control.segmented && control.selected
                   ? Theme.surfaceRaised : "transparent"
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveControl
        elevation: control.segmented && control.selected ? Theme.elevationControl : Theme.elevationNone
        shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.62
        Behavior on fillColor {
            ColorAnimation {
                duration: Theme.reduceMotion ? 0 : 140
                easing.type: Easing.OutCubic
            }
        }

        Superellipse {
            visible: !control.segmented && control.showSelectionIndicator
            opacity: control.selected ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: Theme.motionFast; easing.type: Easing.InOutCubic }
            }
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: Math.min(parent.width - Theme.space5, 38)
            height: 3
            fillColor: Theme.accent
            curveDepth: 1.5
        }
    }
}
