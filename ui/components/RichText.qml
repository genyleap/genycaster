import QtQuick

Text {
    id: root

    property string sourceText: ""
    property int textSize: Theme.typeBodySize
    property real textLineHeight: Theme.typeBodyLineHeight
    property bool forceLeftToRight: false
    property bool forceRightToLeft: false

    signal externalRequested(string url)

    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (localizationController.isRightToLeftText(sourceText) ? Qt.RightToLeft : Qt.LeftToRight)

    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
    }

    function styledText(value) {
        const input = String(value || "")
        const tokenPattern = /(https?:\/\/[^\s<]+)|(@[A-Za-z0-9_.-]+)|(\/[A-Za-z0-9_-]+)/g
        let result = ""
        let offset = 0
        let match
        while ((match = tokenPattern.exec(input)) !== null) {
            result += escapeHtml(input.substring(offset, match.index))
            const token = match[0]
            if (token.startsWith("http://") || token.startsWith("https://")) {
                const escaped = escapeHtml(token)
                result += "<a href=\"" + escaped + "\" style=\"color:" + Theme.accent + ";text-decoration:none\">&#x2066;" + escaped + "&#x2069;</a>"
            } else {
                result += "<span style=\"color:" + Theme.accent + "\">&#x2066;" + escapeHtml(token) + "&#x2069;</span>"
            }
            offset = match.index + token.length
        }
        result += escapeHtml(input.substring(offset))
        return result.replace(/\n/g, "<br>")
    }

    text: styledText(root.sourceText)
    textFormat: Text.RichText
    color: Theme.ink
    font.family: Theme.fontFamilyForText(sourceText)
    font.pixelSize: root.textSize
    lineHeight: root.textLineHeight
    wrapMode: Text.Wrap
    LayoutMirroring.enabled: false
    horizontalAlignment: effectiveTextDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
    onLinkActivated: link => root.externalRequested(link)
}
