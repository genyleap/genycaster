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
