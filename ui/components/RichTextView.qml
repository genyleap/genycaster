import QtQuick
import QtQuick.Layouts

Item {
    id: root
    required property var blockData
    property bool compact: false
    property int emojiSurface: 1
    signal externalRequested(string url)

    // Loader/Repeater teardown can transiently replace a QVariantMap with null.
    // Normalize at the component boundary so bindings never dereference it.
    readonly property var safeBlockData: root.blockData || ({})
    readonly property string kind: String(root.safeBlockData.kind || "paragraph")
    readonly property bool quoteBlock: kind === "quote"
    readonly property bool textBlock: kind === "paragraph" || kind === "heading" || kind === "quote"
                                      || kind === "bullet-list" || kind === "ordered-list"
    readonly property bool rtlContent: localizationController.isRightToLeftText(
        String(root.safeBlockData.text || ""))

    implicitHeight: content.implicitHeight

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: root.quoteBlock ? Theme.space2 : 0
        layoutDirection: root.rtlContent ? Qt.RightToLeft : Qt.LeftToRight
        LayoutMirroring.enabled: false

        Superellipse {
            visible: root.quoteBlock
            Layout.preferredWidth: 3
            Layout.fillHeight: true
            Layout.minimumHeight: 30
            fillColor: Theme.accent
            curveDepth: 2
        }

        // Native text layout with inline emoji objects (see RichInlineText).
        RichInlineText {
            visible: root.textBlock
            Layout.fillWidth: true
            LayoutMirroring.enabled: false
            emojiSurface: root.emojiSurface
            blockData: root.safeBlockData
            fontBold: root.kind === "heading"
            onExternalRequested: url => root.externalRequested(url)
        }
    }
}
