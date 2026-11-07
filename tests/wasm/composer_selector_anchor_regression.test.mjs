import test from 'node:test'
import assert from 'node:assert/strict'
import {readFileSync} from 'node:fs'

const qml = readFileSync(new URL('../../ui/components/ComposerDrawer.qml', import.meta.url), 'utf8')
function popupHeader(id) {
    const start = qml.indexOf('        id: ' + id + '\n')
    assert.ok(start > 0, id + ' popup exists')
    const end = qml.indexOf('        background: PopupSurface {', start)
    assert.ok(end > start)
    return qml.slice(start, end)
}
const channel = popupHeader('channelPicker')
const topic = popupHeader('topicPicker')

test('phone taxonomy uses bounded, viewport-bottom Qt popups; desktop anchors to its trigger', () => {
    for (const [source, cap, fraction, desktopCap] of [
        [channel, 520, '0.72', 390],
        [topic, 460, '0.62', 330],
    ]) {
        assert.match(source, /parent: root\.compact \? root\.contentItem : triggerItem/)
        assert.match(source, /popupType: Popup\.Item/)
        assert.doesNotMatch(source, /parent: Overlay\.overlay/)
        assert.match(source, /modal: root\.compact/)
        assert.match(source, /if \(root\.compact\) return 0/)
        assert.match(source, /width: root\.compact \? \(root\.contentItem \? root\.contentItem\.width : root\.width\)/)
        assert.match(source, /y: root\.compact\s*\? Math\.max\(0, \(root\.contentItem \? root\.contentItem\.height : root\.height\) - height\)/)
        assert.match(source, new RegExp('height: root\\.compact\\s*\\? Math\\.max\\(0, Math\\.min\\(' + cap))
        assert.match(source, new RegExp('\\* ' + fraction.replace('.', '\\.') + '\\)'))
        assert.match(source, new RegExp('Math\\.min\\(' + desktopCap + ', root\\.contentItem'))
        assert.match(source, /Math\.max\(Theme\.space3, root\.bottomSafeInset\)/)
    }
    assert.match(channel, /selectedChannelChip : desktopCastChannelTrigger/)
    assert.match(topic, /selectedTopicChip : desktopCastTopicTrigger/)
})

test('compact taxonomy follows the editor and precedes the sticky media/actions footer', () => {
    const fixed = qml.indexOf('id: composerFixedChrome')
    const scroll = qml.indexOf('id: composerScroll')
    const taxonomy = qml.indexOf('objectName: "mobileCastContext"')
    const footer = qml.indexOf('id: mobileComposerFooter')
    assert.ok(fixed > 0 && fixed < scroll && scroll < taxonomy && taxonomy < footer)
    assert.equal(qml.split('objectName: "mobileCastContext"').length - 1, 1)
    assert.equal(qml.split('id: selectedChannelChip').length - 1, 1)
    assert.equal(qml.split('id: selectedTopicChip').length - 1, 1)
    assert.match(qml, /readonly property bool mobileSelectorOpen: root\.compact\s*&& \(channelPicker\.visible \|\| topicPicker\.visible\)/)
})

test('bottom sheets never exceed the mobile composer viewport', () => {
    for (const h of [320, 430, 600, 760, 920]) {
        for (const [cap, ratio] of [[520, 0.72], [460, 0.62]]) {
            const panelHeight = Math.max(0, Math.min(cap, h * ratio))
            const y = Math.max(0, h - panelHeight)
            assert.ok(panelHeight >= 0 && y >= 0 && panelHeight + y <= h)
        }
    }
    assert.match(qml, /id: topicSearch[\s\S]*?visible: !root\.compact/)
    assert.match(qml, /id: mobileComposerFooter[\s\S]*?visible: root\.compact && root\.editorMode === "visual"/)
    assert.match(qml, /Layout\.bottomMargin: root\.keyboardVisible \? Theme\.space2/)
})

test('selector surfaces use GenyCaster theme and mutually exclusive sheets without hiding desktop controls', () => {
    assert.match(qml, /background: PopupSurface \{[\s\S]*?fillColor: root\.compact \? Theme\.background : Theme\.surfaceElevated/)
    assert.match(qml, /objectName: "mobileCastContext"[\s\S]*?Layout\.topMargin: Theme\.space2/)
    assert.match(qml, /id: selectedChannelChip[\s\S]*?Layout\.preferredHeight: Theme\.touchTarget/)
    assert.match(qml, /id: selectedTopicChip[\s\S]*?Layout\.preferredHeight: Theme\.touchTarget/)
    assert.match(qml, /id: selectedChannelChip[\s\S]*?borderColor: root\.selectedParentUrl\.length > 0[\s\S]*?Theme\.accent : Theme\.borderMuted/)
    assert.match(qml, /onOpened: \{\s*if \(root\.compact\) Qt\.inputMethod\.hide\(\)\s*topicPicker\.close\(\)/)
    assert.match(qml, /onOpened: \{\s*if \(root\.compact\) Qt\.inputMethod\.hide\(\)\s*channelPicker\.close\(\)/)
    assert.match(qml, /visible: root\.compact && root\.editorMode === "visual"/)
})
