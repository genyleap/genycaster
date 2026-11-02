import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'

const source = (file) => readFileSync(new URL(`../../ui/components/${file}`, import.meta.url), 'utf8')

test('composer never accepts blank, whitespace or invisible Unicode alone', () => {
    const composer = source('ComposerDrawer.qml')
    const match = composer.match(/function hasMeaningfulText\(value\) \{([\s\S]*?)\n    \}/)
    assert.ok(match, 'meaningful-text validation exists')
    const validate = new Function('value', match[1])
    for (const value of ['', ' ', '   \n\t', '\u00a0', '\u200b', '\u200c', '\u200d', '\u2060', '\ufeff'])
        assert.equal(validate(value), false, JSON.stringify(value))
    for (const value of ['Hi', 'سلام', '🚀', ' 🪐 ', 'word\n'])
        assert.equal(validate(value), true, JSON.stringify(value))
    assert.match(composer, /hasText = root\.hasMeaningfulText\(markdownEditor\.text\)/)
    assert.match(composer, /hasText = root\.hasMeaningfulText\(mobilePlainEditor\.text\)/)
    assert.match(composer, /hasText = visualEditor\.hasLiveContent/)
    assert.match(composer, /root\.hasMeaningfulText\(text\)/)
})

test('rich editor measures document text instead of nonempty HTML scaffolding', () => {
    const rich = source('RichTextEditor.qml')
    assert.match(rich, /root\.activeEditor\.getText\(0, root\.activeEditor\.length\)/)
    assert.match(rich, /root\.activeEditor\.text === undefined/)
    assert.match(rich, /activeText\.replace\(/)
})

test('composer geometry preserves desktop rounding and viewport scrolling', () => {
    const composer = source('ComposerDrawer.qml')
    assert.match(composer, /fullScreen: root\.compact/)
    assert.match(composer, /onAboutToShow: root\.compact = root\.requestedCompact/)
    assert.doesNotMatch(composer, /parent\.height > parent\.width \* 1\.15/)
    assert.match(composer, /Layout\.minimumHeight: 0\s+clip: true/)
    assert.match(composer, /Math\.max\(128, Math\.min\(255, root\.height \* 0\.4\)\)/)
    assert.match(composer, /Layout\.bottomMargin: !root\.compact \? Theme\.space2 : 0/)
})

test('right click never opens cast actions, while System text has native actions', () => {
    const cast = source('CastItem.qml')
    assert.doesNotMatch(cast, /acceptedButtons: Qt\.RightButton/)
    assert.match(cast, /onClicked: root\.openMoreMenu\(\)/)
    const rich = source('RichInlineText.qml')
    assert.match(rich, /objectName: "systemCastSelectionMenu"/)
    assert.match(rich, /popupType: Qt\.platform\.os === "wasm" \? Popup\.Item : Popup\.Native/)
    assert.match(rich, /acceptedButtons: Qt\.RightButton/)
    assert.match(rich, /onTriggered: presentationBridge\.copyText\(platformText\.selectedText\)/)
})

test('an open composer keeps one editor tree through desktop resize breakpoints', () => {
    const composer = source('ComposerDrawer.qml')
    const shell = source('AppShell.qml')
    assert.match(composer, /property bool requestedCompact: false/)
    assert.match(composer, /property bool compact: false/)
    assert.match(composer, /onAboutToShow: root\.compact = root\.requestedCompact/)
    assert.doesNotMatch(composer, /readonly property bool compact: parent \? parent\.width/)
    assert.match(shell, /hostItem: root\s+requestedCompact: root\.mobile/)
    assert.doesNotMatch(shell, /hostItem: root\.mobile \? root : readingPane/)
    assert.match(composer, /fullScreen: root\.compact/)
    assert.doesNotMatch(composer, /fullScreen: .*parent\.height > parent\.width/)
    assert.match(composer, /visible: !root\.compact && !root\.keyboardVisible/)
    assert.match(composer, /visible: root\.compact && root\.editorMode === "visual"/)
})

test('drawer width, horizontal center and height are bounded across resize', () => {
    const composer = source('ComposerDrawer.qml')
    const expr = (before, after) => {
        const start = composer.indexOf(before)
        const end = composer.indexOf(after, start + before.length)
        assert.ok(start >= 0 && end > start, 'dimension expression exists')
        return composer.slice(start + before.length, end).trim()
    }
    const widthExpr = expr('    width: ', '\n    x: ')
    const xExpr = expr('    x: ', '\n    // Keep both ends')
    const heightExpr = expr('    height: ', '\n    // Composer content')
    const widthFor = new Function('parent','root','Theme','replyMode', 'return (' + widthExpr + ')')
    const xFor = new Function('parent','root','Theme','replyMode','width', 'return (' + xExpr + ')')
    const heightFor = new Function('parent','root','Theme','replyMode', 'return (' + heightExpr + ')')
    const theme = { space4: 16 }
    for (const openedAt of [390, 1440]) {
        const capturedCompact = openedAt < 600
        for (const viewportWidth of [320, 390, 599, 600, 680, 719, 720, 900, 1200, 1920]) {
            for (const viewportHeight of [350, 500, 640, 900]) {
                for (const replyMode of [true, false]) {
                    const parent = { width: viewportWidth, height: viewportHeight }
                    const root = { compact: capturedCompact }
                    const width = widthFor(parent,root,theme,replyMode)
                    const x = xFor(parent,root,theme,replyMode,width)
                    const height = heightFor(parent,root,theme,replyMode)
                    assert.ok(width > 0 && width <= viewportWidth)
                    assert.ok(x >= 0 && x + width <= viewportWidth + 0.01)
                    assert.ok(height >= 0 && height <= viewportHeight)
                    if (capturedCompact) {
                        assert.equal(width,viewportWidth)
                        assert.equal(height,viewportHeight)
                    } else {
                        assert.ok(width <= 720)
                        assert.ok(height <= 660)
                    }
                }
            }
        }
    }
    assert.doesNotMatch(composer,/Math\.min\(255, composerScroll\.height/)
    assert.match(composer,/Math\.max\(128, Math\.min\(255, root\.height \* 0\.4\)\)/)
})
