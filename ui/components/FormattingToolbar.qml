import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
    id: root
    property int activeBlockIndex: -1
    property var activeEditor: null
    property var editorHost: null
    signal linkRequested(int blockIndex, int selectionStart, int selectionLength, string selectedText)
    signal mentionRequested(int blockIndex, int selectionStart, int selectionLength, string selectedText)
    signal mediaRequested(int blockIndex)
    signal embedRequested(int blockIndex)
    signal pollRequested()
    signal emojiRequested()

    fillColor: Theme.surfaceElevated
    borderWidth: Theme.strokeHairline
    borderColor: Theme.borderSubtle
    curveDepth: Theme.curveControl
    elevation: Theme.elevationNone
    implicitHeight: 40

    Flickable {
        anchors.fill: parent
        anchors.margins: 3
        contentWidth: row.implicitWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        clip: true

        RowLayout {
            id: row
            height: parent.height
            spacing: 1
            layoutDirection: Theme.uiDirection

            ActionButton {
                quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconBold
                Accessible.name: qsTrId("genycaster.composer.bold")
                enabled: root.activeBlockIndex >= 0 && root.activeEditor !== null && root.activeEditor.selectionStart !== root.activeEditor.selectionEnd
                onClicked: if (root.editorHost) root.editorHost.applyInlineStyle("strong")
            }
            ActionButton {
                quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconItalic
                Accessible.name: qsTrId("genycaster.composer.italic")
                enabled: root.activeBlockIndex >= 0 && root.activeEditor !== null && root.activeEditor.selectionStart !== root.activeEditor.selectionEnd
                onClicked: if (root.editorHost) root.editorHost.applyInlineStyle("emphasis")
            }
            ActionButton {
                quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconCode
                Accessible.name: qsTrId("genycaster.composer.inline_code")
                enabled: openProofConnections.proEntitlements.eligible === true
                         && root.activeBlockIndex >= 0 && root.activeEditor !== null && root.activeEditor.selectionStart !== root.activeEditor.selectionEnd
                onClicked: if (root.editorHost) root.editorHost.applyInlineStyle("inline-code")
            }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: Theme.divider }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconHeading; text: qsTrId("genycaster.composer.heading"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("heading", root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconQuote; text: qsTrId("genycaster.composer.quote"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("quote", root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconList; text: qsTrId("genycaster.composer.list"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("bullet-list", root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconOrderedList; text: qsTrId("genycaster.composer.ordered_list"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("ordered-list", root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconCode; text: qsTrId("genycaster.composer.code_block"); enabled: openProofConnections.proEntitlements.eligible === true; onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("code", root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconSeparator; text: qsTrId("genycaster.composer.separator"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); richComposer.insertBlock("separator", root.activeBlockIndex) } }
            ActionButton {
                quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconLink; text: qsTrId("genycaster.composer.link")
                enabled: root.activeBlockIndex >= 0 && root.activeEditor !== null && root.activeEditor.selectionStart !== root.activeEditor.selectionEnd
                onClicked: { const start = root.activeEditor.selectionStart; const length = root.activeEditor.selectionEnd - start; const selected = root.activeEditor.selectedText; if (root.editorHost) root.editorHost.commitActiveEditor(); root.linkRequested(root.activeBlockIndex, start, length, selected) }
            }
            ActionButton {
                quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconMention; text: qsTrId("genycaster.composer.mention")
                enabled: root.activeBlockIndex >= 0 && root.activeEditor !== null && root.activeEditor.selectionStart !== root.activeEditor.selectionEnd
                onClicked: { const start = root.activeEditor.selectionStart; const length = root.activeEditor.selectionEnd - start; const selected = root.activeEditor.selectedText; if (root.editorHost) root.editorHost.commitActiveEditor(); root.mentionRequested(root.activeBlockIndex, start, length, selected) }
            }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconLink; text: qsTrId("genycaster.composer.embed"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); root.embedRequested(root.activeBlockIndex) } }
            ActionButton { quiet: true; compact: true; focusPolicy: Qt.NoFocus; iconOnly: true; iconSize: Theme.iconSizeSmall; iconGlyph: Theme.iconImage; text: qsTrId("genycaster.composer.media"); onClicked: { if (root.editorHost) root.editorHost.commitActiveEditor(); root.mediaRequested(root.activeBlockIndex) } }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: Theme.divider }
            ActionButton {
                quiet: true
                compact: true
                focusPolicy: Qt.NoFocus
                iconOnly: true
                iconGlyph: Theme.iconEmoji
                iconSize: Theme.iconSizeSmall
                text: qsTrId("genycaster.composer.emoji")
                onClicked: root.emojiRequested()
            }
        }
    }
}
