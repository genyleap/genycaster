import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Quiet Telegram-style tab: no bulky blue chip/background; a single accent
// baseline carries selection for My Stickers, pack names and quick filters.
Button {
    id: root
    property bool selected: false

    implicitWidth: Math.max(Theme.scaleUi(48),
        Math.min(Theme.scaleUi(154), tabLabel.implicitWidth + Theme.space3 * 2))
    implicitHeight: Theme.scaleUi(32)
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    topPadding: Theme.space1
    bottomPadding: Theme.space1
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: text

    background: Item {
        Superellipse {
            anchors.fill: parent
            anchors.bottomMargin: Theme.scaleUi(2)
            curveDepth: Theme.curveControl
            fillColor: root.down ? Theme.surfaceHover
                : root.hovered ? Theme.surfaceMuted : "transparent"
            borderWidth: 0
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(Theme.scaleUi(24),
                Math.min(parent.width - Theme.space2 * 2, tabLabel.implicitWidth))
            height: Theme.scaleUi(2)
            color: Theme.accent
            visible: root.selected
        }
        Superellipse {
            anchors.fill: parent
            curveDepth: Theme.curveControl
            fillColor: "transparent"
            borderWidth: Theme.keyboardFocusVisible(root) ? 1 : 0
            borderColor: Theme.focus
        }
    }

    contentItem: DirectionAwareLabel {
        id: tabLabel
        text: root.text
        color: root.selected ? Theme.accent
             : root.hovered ? Theme.ink : Theme.inkMuted
        font.pixelSize: Theme.typeCaptionSize
        font.weight: root.selected ? Theme.weightDemiBold : Theme.weightMedium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
