import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    objectName: "bookmarkCollectionChooser"
    parent: Overlay.overlay
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(392,parent.width-Theme.space4*2) : 360
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? Math.max(Theme.space3,(parent.height-height)/2) : 0
    padding: Theme.space3
    property var collections: []
    property bool busy: false
    property string errorText: ""
    property string castHash: ""
    property string newName: ""
    signal collectionPicked(string id)
    signal createRequested(string name)
    signal unfiledRequested()

    background: PopupSurface { curveDepth: Theme.curveContainer }
    contentItem: ColumnLayout {
        width: Math.max(1,root.width-root.padding*2)
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: "Save to collection"
            font.pixelSize: Theme.typeBodySize
            font.weight: Theme.weightDemiBold
            color: Theme.textPrimary
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: "Your bookmark is saved. Choose a personal collection, or leave it uncategorized."
            wrapMode: Text.Wrap
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
        }
        ActionButton {
            Layout.fillWidth: true
            compact: true
            quiet: true
            text: "Uncategorized"
            enabled: !root.busy
            onClicked: {
                root.unfiledRequested()
                root.close()
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(216,Math.max(0,root.collections.length*42))
            visible: root.collections.length>0
            clip: true
            ColumnLayout {
                width: Math.max(1,root.width-root.padding*2)
                spacing: 0
                Repeater {
                    model: root.collections
                    delegate: ActionButton {
                        required property var modelData
                        Layout.fillWidth: true
                        quiet: true
                        compact: true
                        iconGlyph: Theme.iconBookmark
                        text: String(modelData.name||"")
                        enabled: !root.busy
                        onClicked: {
                            root.collectionPicked(String(modelData.id||""))
                            root.close()
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareTextField {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                placeholderText: "New collection"
                maximumLength: 80
                text: root.newName
                onTextChanged: root.newName=text
            }
            ActionButton {
                compact: true
                prominent: true
                iconGlyph: Theme.iconPlus
                text: "Create"
                enabled: !root.busy && root.newName.trim().length>0
                onClicked: root.createRequested(root.newName.trim())
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            color: Theme.negative
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        GenySpinner {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            running: root.busy
            visible: root.busy
        }
    }
}
