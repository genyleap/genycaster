import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    property string source: ""
    property string language: "auto"
    property bool lineNumbers: false
    property bool wrap: false
    property bool compact: false

    fillColor: Theme.codeSurface
    borderWidth: 1
    borderColor: Theme.codeBorder
    curveDepth: Theme.curveContainer
    // Fit short snippets instead of reserving an editor-sized empty panel.
    // Longer code remains scrollable, with the full source available via Copy.
    implicitHeight: Math.min(Math.max(root.compact ? 76 : 88,
        codeText.implicitHeight + header.implicitHeight
        + (root.compact ? Theme.space2 * 2 : Theme.space3 * 2) + Theme.space1),
        root.compact ? 360 : 440)

    function escapeHtml(value) {
        return String(value || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\"/g, "&quot;")
    }

    function tokenColor(kind) {
        switch (String(kind || "")) {
        case "keyword": return Theme.syntaxKeyword
        case "type": return Theme.syntaxType
        case "function": return Theme.syntaxFunction
        case "variable": return Theme.syntaxVariable
        case "string": return Theme.syntaxString
        case "number": return Theme.syntaxNumber
        case "comment": return Theme.syntaxComment
        case "operator": return Theme.syntaxOperator
        case "attribute": return Theme.syntaxAttribute
        case "preprocessor": return Theme.syntaxPreprocessor
        case "namespace": return Theme.syntaxNamespace
        case "property": return Theme.syntaxProperty
        case "literal": return Theme.syntaxLiteral
        case "tag": return Theme.syntaxTag
        case "command": return Theme.syntaxCommand
        case "url": return Theme.syntaxUrl
        case "header": return Theme.syntaxHeader
        case "parameter": return Theme.syntaxParameter
        default: return Theme.codeInk
        }
    }

    function highlighted() {
        const input = String(root.source || "")
        const tokens = richComposer.highlightTokens(input, root.language)
        let cursor = 0
        let html = "<pre style='white-space:" + (root.wrap ? "pre-wrap" : "pre") + ";margin:0;line-height:1.5'>"
        for (let i = 0; i < tokens.length; ++i) {
            const token = tokens[i]
            const start = Number(token.start || 0)
            const length = Number(token.length || 0)
            if (start > cursor) html += root.escapeHtml(input.substring(cursor, start))
            const body = root.escapeHtml(input.substring(start, start + length))
            html += "<span style='color:" + root.tokenColor(token.kind) + "'>" + body + "</span>"
            cursor = start + length
        }
        if (cursor < input.length) html += root.escapeHtml(input.substring(cursor))
        return html + "</pre>"
    }

    function lineNumberText() {
        if (!root.lineNumbers) return ""
        const count = Math.max(1, String(root.source || "").split("\n").length)
        let value = ""
        for (let i = 1; i <= count; ++i) value += (i > 1 ? "\n" : "") + i
        return value
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? Theme.space2 : Theme.space3
        spacing: Theme.space1

        RowLayout {
            id: header
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Label {
                text: root.language === "auto" ? richComposer.detectLanguage(root.source) : root.language
                color: Theme.codeLabel
                font.family: Theme.latinFontFamily
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                compact: true
                quiet: true
                iconOnly: root.compact
                id: copyCodeButton
                iconGlyph: Theme.iconCopy
                text: qsTrId("genycaster.composer.copy_code")
                onClicked: {
                    if (root.source.length === 0) return
                    const copied = presentationBridge.copyText(root.source)
                    copyCodeButton.acknowledgeCopy(copied)
                    copyCodeFeedbackTimer.restart()
                }
            }
            Timer {
                id: copyCodeFeedbackTimer
                interval: 1300
                repeat: false
                onTriggered: copyCodeButton.clearCopyFeedback()
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal: GenyScrollBar { policy: root.wrap ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded }
            ScrollBar.vertical: GenyScrollBar {}

            Row {
                spacing: root.lineNumbers ? Theme.space2 : 0
                width: root.wrap ? scroll.availableWidth : Math.max(scroll.availableWidth, implicitWidth)
                height: Math.max(scroll.availableHeight, Math.max(codeText.implicitHeight, lineNumbers.implicitHeight))
                layoutDirection: Qt.LeftToRight

                Text {
                    id: lineNumbers
                    visible: root.lineNumbers
                    width: visible ? Math.max(32, implicitWidth + Theme.space1) : 0
                    text: root.lineNumberText()
                    color: Theme.inkSubtle
                    font.family: Theme.codeFontFamily
                    font.pixelSize: Theme.typeMetaSize
                    lineHeight: 1.5
                    horizontalAlignment: Text.AlignRight
                    LayoutMirroring.enabled: false
                }

                TextEdit {
                    id: codeText
                    readOnly: true
                    width: root.wrap
                           ? Math.max(80, scroll.availableWidth - lineNumbers.width - parent.spacing)
                           : implicitWidth
                    text: root.highlighted()
                    textFormat: TextEdit.RichText
                    color: Theme.codeInk
                    font.family: Theme.codeFontFamily
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: root.wrap ? TextEdit.WrapAnywhere : TextEdit.NoWrap
                    LayoutMirroring.enabled: false
                    horizontalAlignment: TextEdit.AlignLeft
                    selectByMouse: true
                }
            }
        }
    }
}
