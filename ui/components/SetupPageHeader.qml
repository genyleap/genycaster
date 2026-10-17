import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One shared baseline for the icon/title on every Setup stage.
ColumnLayout {
    id: root
    property string iconGlyph: ""
    property string title: ""
    property string subtitle: ""
    property bool showIcon: iconGlyph.length > 0
    property int maximumTextWidth: 620
    readonly property int iconSize: 27
    spacing: Theme.space2

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection
        Icon {
            visible: root.showIcon
            text: root.iconGlyph
            color: Theme.accent
            preferredSize: root.iconSize
            Layout.preferredWidth: root.showIcon ? root.iconSize : 0
            Layout.alignment: Qt.AlignVCenter
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
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
    DirectionAwareLabel {
        visible: root.subtitle.length > 0
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumTextWidth
        // The body copy uses the same title text start edge.
        Layout.leftMargin: root.showIcon && !Theme.rtl ? root.iconSize + Theme.space3 : 0
        Layout.rightMargin: root.showIcon && Theme.rtl ? root.iconSize + Theme.space3 : 0
        text: root.subtitle
        color: Theme.inkMuted
        font.pixelSize: Theme.typeLabelSize
        font.weight: Theme.weightRegular
        lineHeight: Theme.typeLabelLineHeight
        horizontalAlignment: Theme.textStartAlignment
        wrapMode: Text.WordWrap
    }
}
