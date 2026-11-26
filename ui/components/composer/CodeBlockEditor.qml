import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    required property int blockIndex
    required property var blockData
    property bool compact: false
    signal editorFocusChanged(int blockIndex, var editor, bool active)

    fillColor: Theme.codeSurface
    borderWidth: codeEditor.activeFocus ? 2 : 1
    borderColor: codeEditor.activeFocus ? Theme.focus : Theme.codeBorder
    curveDepth: Theme.curveContainer
    implicitHeight: Math.max(260, codeEditor.contentHeight + 132)

    function currentLanguage() {
        return String(root.blockData.language || "auto")
    }

    function semanticPalette() {
        return {
            "keyword": Theme.syntaxKeyword,
            "type": Theme.syntaxType,
            "function": Theme.syntaxFunction,
            "variable": Theme.syntaxVariable,
            "string": Theme.syntaxString,
            "number": Theme.syntaxNumber,
            "comment": Theme.syntaxComment,
            "operator": Theme.syntaxOperator,
            "attribute": Theme.syntaxAttribute,
            "preprocessor": Theme.syntaxPreprocessor,
            "namespace": Theme.syntaxNamespace,
            "property": Theme.syntaxProperty,
            "literal": Theme.syntaxLiteral,
            "tag": Theme.syntaxTag,
            "command": Theme.syntaxCommand,
            "url": Theme.syntaxUrl,
            "header": Theme.syntaxHeader,
            "parameter": Theme.syntaxParameter
        }
    }

    function lineNumberText() {
        if (!Boolean(root.blockData.lineNumbers)) return ""
        const count = Math.max(1, String(codeEditor.text || "").split("\n").length)
        let result = ""
        for (let i = 1; i <= count; ++i) result += (i > 1 ? "\n" : "") + i
        return result
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            LanguageSelector {
                languageId: root.currentLanguage()
                onLanguageSelected: language => {
                    richComposer.setCodeLanguage(root.blockIndex, language)
                    richComposer.attachSyntaxDocument(codeEditor.textDocument, language, root.semanticPalette())
                }
            }

            Item { Layout.fillWidth: true }

            GenySwitch {
                text: qsTrId("genycaster.composer.wrap")
                checked: Boolean(root.blockData.wrap)
                onToggled: richComposer.setCodeWrap(root.blockIndex, checked)
            }
            GenySwitch {
                visible: !root.compact
                text: qsTrId("genycaster.composer.line_numbers")
                checked: Boolean(root.blockData.lineNumbers)
                onToggled: richComposer.setCodeLineNumbers(root.blockIndex, checked)
            }
            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconTrash
                Accessible.name: qsTrId("genycaster.composer.clear_block")
                onClicked: {
                    codeEditor.text = ""
                    richComposer.setBlockText(root.blockIndex, "")
                    codeEditor.forceActiveFocus()
                }
            }
            ActionButton {
                compact: true
                quiet: true
                destructive: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                Accessible.name: qsTrId("genycaster.composer.remove_block")
                onClicked: richComposer.removeBlock(root.blockIndex)
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal: GenyScrollBar { policy: Boolean(root.blockData.wrap) ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded }
            ScrollBar.vertical: GenyScrollBar {}

            Item {
                width: Boolean(root.blockData.wrap)
                       ? scroll.availableWidth
                       : Math.max(scroll.availableWidth, gutter.width + codeEditor.contentWidth + Theme.space4)
                height: Math.max(scroll.availableHeight, codeEditor.contentHeight + Theme.space2)

                Text {
                    id: gutter
                    visible: Boolean(root.blockData.lineNumbers)
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: visible ? Math.max(34, implicitWidth + Theme.space2) : 0
                    text: root.lineNumberText()
                    color: Theme.inkSubtle
                    font.family: Theme.codeFontFamily
                    font.pixelSize: Theme.typeMetaSize
                    lineHeight: 1.5
                    horizontalAlignment: Text.AlignRight
                    LayoutMirroring.enabled: false
                }

                DirectionAwareTextArea {
                    id: codeEditor
                    x: gutter.width + (gutter.visible ? Theme.space2 : 0)
                    width: Boolean(root.blockData.wrap)
                           ? Math.max(80, parent.width - x)
                           : Math.max(scroll.availableWidth - x, contentWidth + Theme.space3)
                    height: Math.max(parent.height, contentHeight + Theme.space2)
                    text: String(root.blockData.text || "")
                    color: Theme.codeInk
                    selectionColor: Theme.primarySurfaceHover
                    selectedTextColor: Theme.ink
                    font.family: Theme.codeFontFamily
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: Boolean(root.blockData.wrap) ? TextEdit.WrapAnywhere : TextEdit.NoWrap
                    selectByMouse: true
                    persistentSelection: true
                    LayoutMirroring.enabled: false
                    horizontalAlignment: Text.AlignLeft
                    background: null

                    Timer {
                        id: syntaxRefresh
                        interval: 220
                        repeat: false
                        onTriggered: richComposer.attachSyntaxDocument(codeEditor.textDocument, root.currentLanguage(), root.semanticPalette())
                    }

                    Component.onCompleted: richComposer.attachSyntaxDocument(textDocument, root.currentLanguage(), root.semanticPalette())
                    onTextChanged: if (activeFocus) syntaxRefresh.restart()
                    onActiveFocusChanged: {
                        root.editorFocusChanged(root.blockIndex, codeEditor, activeFocus)
                        if (activeFocus)
                            richComposer.attachSyntaxDocument(textDocument, root.currentLanguage(), root.semanticPalette())
                    }
                    Keys.onPressed: event => {
                        if ((event.modifiers & Qt.ControlModifier || event.modifiers & Qt.MetaModifier) && event.key === Qt.Key_Enter)
                            event.accepted = false
                    }
                }
            }
        }
    }
}
