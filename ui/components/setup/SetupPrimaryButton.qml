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

    contentItem: RowLayout {
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: root.busy
            running: visible
            color: root.foregroundColor
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Icon {
            visible: !root.busy && root.iconGlyph.length > 0
            text: root.iconGlyph
            color: root.foregroundColor
            preferredSize: Theme.iconSizeSmall
        }

        DirectionAwareLabel {
            id: primaryLabel
            Layout.fillWidth: true
            text: root.text
            color: root.foregroundColor
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
        }

        Icon {
            visible: root.showArrow && !root.busy
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: root.foregroundColor
            preferredSize: Theme.iconSizeSmall
        }
    }
}
