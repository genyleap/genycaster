import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Shared editorial hierarchy: icon above a concise title, not an inline form label.
ColumnLayout {
    id: root
    property string iconGlyph: ""
    property string title: ""
    property string subtitle: ""
    property bool showIcon: iconGlyph.length > 0
    property int maximumTextWidth: 620
    readonly property int iconSize: 25
    spacing: Theme.space3

    Icon {
        visible: root.showIcon
        text: root.iconGlyph
        color: Theme.ink
        preferredSize: root.iconSize
        Layout.preferredWidth: root.showIcon ? root.iconSize : 0
        Layout.preferredHeight: root.showIcon ? root.iconSize : 0
        Accessible.name: root.title
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumTextWidth
        text: root.title
        color: Theme.ink
        font.family: Theme.fontFamilyForText(text)
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
        font.family: Theme.fontFamilyForText(text)
        font.pixelSize: Theme.typeLabelSize
        font.weight: Theme.weightRegular
        lineHeight: Theme.typeLabelLineHeight
        horizontalAlignment: Theme.textStartAlignment
        wrapMode: Text.WordWrap
    }
}
