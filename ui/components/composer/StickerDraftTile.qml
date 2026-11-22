import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Image-first draft presentation: a sticker is not a photo attachment card.
// Preserve its original alpha, scale without cropping and keep Remove reachable
// for mouse, keyboard and touch. One shared tile serves native and WASM editors.
Item {
    id: root
    objectName: "composerStickerDraftTile"

    property string imageUrl: ""
    property string altText: ""
    property bool removable: true
    property real tileSize: 164

    signal removeRequested()

    implicitWidth: tileSize
    implicitHeight: tileSize
    Accessible.role: Accessible.Graphic
    Accessible.name: altText.length > 0 ? altText : qsTrId("genycaster.stickers.title")

    Image {
        id: artwork
        objectName: "stickerDraftArtwork"
        anchors.fill: parent
        anchors.margins: Theme.space2
        source: root.imageUrl
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
        smooth: true
        mipmap: false
        sourceSize.width: 512
        sourceSize.height: 512
    }

    // A loading state is a placeholder, not a fake upload progress bar.
    Icon {
        anchors.centerIn: parent
        visible: artwork.status === Image.Loading || artwork.status === Image.Error
        text: artwork.status === Image.Error ? Theme.iconWarning : Theme.iconSticker
        color: Theme.inkMuted
        preferredSize: Theme.typeTitle2Size
        opacity: 0.66
    }

    ActionButton {
        id: removeAction
        objectName: "removeDraftSticker"
        visible: root.removable
        z: 2
        anchors.top: parent.top
        anchors.right: parent.right
        compact: true
        quiet: false
        iconOnly: true
        iconGlyph: Theme.iconClose
        text: qsTrId("genycaster.composer.remove_block")
        onClicked: root.removeRequested()
    }
}
