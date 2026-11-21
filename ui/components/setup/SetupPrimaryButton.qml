import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Setup CTA is an ActionButton variant, not a second Button/Neon renderer.
// All hover, focus, pressed, destructive and accent material policy belongs
// to the shared ActionButton background.
ActionButton {
    id: root

    property bool showArrow: true

    prominent: true
    elevated: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(58, primaryLabel.implicitHeight + Theme.space3 * 2)
    implicitWidth: Math.max(168, primaryLabel.implicitWidth + Theme.space8)
    leftPadding: Theme.space4
    rightPadding: Theme.space4

    Accessible.name: text

    // Equal fixed-size leading/trailing slots keep the actual label centered,
    // regardless of whether a spinner, icon or arrow is present (LTR/RTL).
    contentItem: RowLayout {
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Item {
            Layout.preferredWidth: Theme.iconSizeSmall
            Layout.preferredHeight: Theme.iconSizeSmall
            GenySpinner {
                anchors.centerIn: parent
                width: Theme.iconSizeSmall
                height: width
                visible: root.busy
                running: visible
                color: root.foregroundColor
            }
            Icon {
                anchors.centerIn: parent
                visible: !root.busy && root.iconGlyph.length > 0
                text: root.iconGlyph
                color: root.foregroundColor
                preferredSize: Theme.iconSizeSmall
            }
        }

        DirectionAwareLabel {
            id: primaryLabel
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.text
            color: root.foregroundColor
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
        }

        Item {
            Layout.preferredWidth: Theme.iconSizeSmall
            Layout.preferredHeight: Theme.iconSizeSmall
            Icon {
                anchors.centerIn: parent
                visible: root.showArrow && !root.busy
                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                color: root.foregroundColor
                preferredSize: Theme.iconSizeSmall
            }
        }
    }
}
