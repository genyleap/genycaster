import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Switch {
    id: control

    property string description: ""

    implicitHeight: Math.max(Theme.touchTarget, switchRow.implicitHeight + Theme.space2)
    hoverEnabled: true
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    topPadding: Theme.space1
    bottomPadding: Theme.space1
    indicator: null
    Accessible.name: text
    Accessible.description: description
    containmentMask: background
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    scale: control.down ? 0.992 : 1.0
    opacity: control.enabled ? 1.0 : 0.58

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: RowLayout {
        id: switchRow
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: control.text
                color: control.enabled ? Theme.ink : Theme.inkDisabled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightMedium
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }

            Label {
                visible: control.description.length > 0
                Layout.fillWidth: true
                text: control.description
                color: control.enabled ? Theme.inkMuted : Theme.inkDisabled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.Wrap
            }
        }

        Item {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 26
            Layout.alignment: Qt.AlignVCenter

            Superellipse {
                anchors.fill: parent
                fillColor: !control.enabled ? Theme.surfacePressed
                           : control.checked ? Theme.primaryFill
                           : control.hovered ? Theme.surfacePressed : Theme.surfaceMuted
                borderWidth: control.checked ? 0 : 1
                borderColor: Theme.borderStrong
                curveDepth: height / 2
                Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
            }

            Superellipse {
                width: 20
                height: 20
                exponent: 2.01
                curveDepth: width / 2
                y: 3
                x: Theme.rtl
                   ? (control.checked ? 3 : parent.width - width - 3)
                   : (control.checked ? parent.width - width - 3 : 3)
                fillColor: Theme.inkInverse
                borderWidth: control.checked ? 0 : 1
                borderColor: Theme.borderMuted
                elevation: control.enabled ? Theme.elevationControl : Theme.elevationNone
                shadowOpacityScale: control.down ? 0.45 : 0.82

                Behavior on x {
                    NumberAnimation { duration: Theme.motionStandard; easing.type: Easing.OutCubic }
                }
            }
        }
    }

    background: Superellipse {
        fillColor: control.hovered || control.activeFocus ? Theme.castHover : "transparent"
        borderWidth: control.activeFocus ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveControl
        shadowColor: control.activeFocus ? Theme.focusHalo : Theme.shadow
        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
    }
}
