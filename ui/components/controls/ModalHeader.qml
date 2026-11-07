import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

RowLayout {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool showClose: true
    // Compact bottom sheets own a drag handle. Their title is centered
    // beneath it and the redundant corner close action is omitted.
    property bool sheetPresentation: false

    signal closeRequested()

    spacing: root.sheetPresentation ? 0 : Theme.space3
    Layout.minimumHeight: root.sheetPresentation
                          ? Theme.scaleUi(32)
                          : Theme.scaleUi(root.subtitle.length > 0 ? 56 : 44)
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: Theme.space1

        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
            horizontalAlignment: root.sheetPresentation
                ? Text.AlignHCenter : Theme.textStartAlignment
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Label {
            visible: root.subtitle.length > 0
            Layout.fillWidth: true
            text: root.subtitle
            color: Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            horizontalAlignment: root.sheetPresentation
                ? Text.AlignHCenter : Theme.textStartAlignment
            wrapMode: Text.Wrap
        }
    }

    ActionButton {
        visible: root.showClose && !root.sheetPresentation
        Layout.alignment: Qt.AlignVCenter
        compact: true
        quiet: true
        iconOnly: true
        iconGlyph: Theme.iconClose
        text: qsTrId("genycaster.locale.close")
        onClicked: root.closeRequested()
    }
}
