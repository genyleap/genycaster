import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

ColumnLayout {
    id: root

    property string iconGlyph: ""
    property string title: ""
    property string subtitle: ""
    property bool showIcon: iconGlyph.length > 0
    property int maximumTextWidth: 620

    spacing: Theme.space2

    Icon {
        visible: root.showIcon
        text: root.iconGlyph
        color: Theme.accent
        preferredSize: 32
        Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
        Accessible.name: root.title
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumTextWidth
        text: root.title
        color: Theme.ink
        font.pixelSize: Theme.typeTitle2Size
        font.weight: Theme.weightBold
        lineHeight: Theme.typeTitle2LineHeight
        horizontalAlignment: Theme.textStartAlignment
        wrapMode: Text.WordWrap
    }

    DirectionAwareLabel {
        visible: root.subtitle.length > 0
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumTextWidth
        text: root.subtitle
        color: Theme.inkMuted
        font.pixelSize: Theme.typeLabelSize
        font.weight: Theme.weightRegular
        lineHeight: Theme.typeLabelLineHeight
        horizontalAlignment: Theme.textStartAlignment
        wrapMode: Text.WordWrap
    }
}
