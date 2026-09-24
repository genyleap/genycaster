import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool showClose: true

    signal closeRequested()

    spacing: Theme.space3
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1

        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Theme.textStartAlignment
            elide: Text.ElideRight
        }

        Label {
            visible: root.subtitle.length > 0
            Layout.fillWidth: true
            text: root.subtitle
            color: Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: Theme.textStartAlignment
            wrapMode: Text.Wrap
        }
    }

    ActionButton {
        visible: root.showClose
        compact: true
        quiet: true
        iconOnly: true
        iconGlyph: Theme.iconClose
        text: qsTrId("genycaster.locale.close")
        onClicked: root.closeRequested()
    }
}
