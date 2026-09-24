import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Window

Control {
    id: root

    /** Semantic icon name from Theme, for example Theme.iconHome. */
    property string text: ""
    property alias glyph: root.text
    property color color: Theme.ink
    property bool filled: false
    /** Exact visual size for role-specific icons; zero uses the design-system default. */
    property real preferredSize: 0
    readonly property url source: Theme.iconSource(root.text, root.filled)
    readonly property real iconPixelSize: root.preferredSize > 0 ? root.preferredSize : Theme.iconSize
    readonly property int rasterSize: Math.max(16, Math.ceil(root.iconPixelSize
        * Math.max(1, Screen.devicePixelRatio) * renderingController.vectorRasterScale))

    implicitWidth: Math.max(22, Math.ceil(root.iconPixelSize + 2))
    implicitHeight: implicitWidth
    padding: 0
    background: null

    contentItem: Item {
        // Semantic SVG source; equivalent to the Controls path `icon.source: root.source`.
        // SVG assets intentionally come from multiple upstream families. Some use
        // currentColor while others contain literal black strokes. MultiEffect's
        // colorization preserves source luminance, so first lift every opaque source
        // pixel to white and then apply the semantic theme color. Alpha/duotone
        // opacity remains untouched and therefore works in both light and dark themes.
        Image {
            id: vectorSource
            anchors.centerIn: parent
            width: root.iconPixelSize
            height: root.iconPixelSize
            source: root.source
            sourceSize.width: root.rasterSize
            sourceSize.height: root.rasterSize
            fillMode: Image.PreserveAspectFit
            asynchronous: false
            cache: true
            smooth: true
            mipmap: false
            visible: false
        }

        MultiEffect {
            anchors.centerIn: parent
            width: root.iconPixelSize
            height: root.iconPixelSize
            source: vectorSource
            visible: root.source.toString().length > 0
            brightness: 1.0
            colorization: 1.0
            colorizationColor: root.color
            autoPaddingEnabled: false
            antialiasing: true
        }

        Text {
            anchors.fill: parent
            visible: root.source.toString().length === 0
            text: Theme.iconGlyph(root.text)
            color: root.color
            font.family: Theme.iconFamily(root.text)
            font.styleName: Theme.iconUsesRegular(root.text) ? "Regular" : "Solid"
            font.weight: Theme.iconUsesRegular(root.text) ? Font.Normal : Font.Black
            font.pixelSize: Math.round(root.iconPixelSize * 0.82)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
            antialiasing: true
        }
    }
}
