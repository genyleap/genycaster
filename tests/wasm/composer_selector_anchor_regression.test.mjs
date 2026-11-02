import test from 'node:test'
import assert from 'node:assert/strict'
import {readFileSync} from 'node:fs'

const qml = readFileSync(new URL('../../ui/components/ComposerDrawer.qml', import.meta.url), 'utf8')
const popupHeader = id => {
    const start = qml.indexOf('        id: ' + id + '\n')
    assert.ok(start > 0, id + ' popup exists')
    const end = qml.indexOf('        background: PopupSurface {', start)
    assert.ok(end > start)
    return qml.slice(start,end)
}
const body = popupHeader('channelPicker')
const topics = popupHeader('topicPicker')
const chromeStart = qml.indexOf('id: composerFixedChrome')
const footerStart = qml.indexOf('id: mobileComposerFooter')
const taxonomyStart = qml.indexOf('objectName: "mobileCastContext"')

test('mobile popups use the composer coordinate space; desktop stays trigger anchored', () => {
    for (const [header,maxCompact,maxDesktop] of [[body,350,390],[topics,304,330]]) {
        assert.match(header,/parent: root.compact \? root.contentItem : triggerItem/)
        assert.match(header,/popupType: Popup\.Item/)
        assert.doesNotMatch(header,/parent: Overlay\.overlay/)
        assert.match(header,/if \(root.compact\) return boundaryInset/)
        assert.match(header,/root.compact \? triggerOrigin.y : 0/)
        assert.match(header,/root.contentItem.width - boundaryInset \* 2/)
        assert.match(header,/mobileComposerFooter.implicitHeight/)
        assert.match(header,new RegExp('root.compact \\? '+maxCompact+' : '+maxDesktop))
    }
    assert.match(body,/selectedChannelChip : desktopCastChannelTrigger/)
    assert.match(topics,/selectedTopicChip : desktopCastTopicTrigger/)
})

test('compact mode selectors are above the editor, with sufficient space below', () => {
    assert.ok(chromeStart < taxonomyStart && taxonomyStart < footerStart)
    const fixedChromeEnd = qml.indexOf('        id: composerScroll')
    assert.ok(taxonomyStart < fixedChromeEnd)
    assert.equal(qml.split('objectName: "mobileCastContext"').length - 1,1)
    assert.equal(qml.split('id: selectedChannelChip').length - 1,1)
    assert.equal(qml.split('id: selectedTopicChip').length - 1,1)
})

test('mobile dropdowns fit within horizontal gutters, stay below controls and above footer',()=>{
  const inset=10,gap=8,footer=112
  for(const composerWidth of [300,320,375,390,430,600,720]){
    for(const composerHeight of [320,430,600,760,920]){
      const triggerY=116,triggerHeight=44
      const top=triggerY+triggerHeight+gap
      for(const maximumHeight of [304,350]){
        const width=Math.max(0,Math.min(410,composerWidth-inset*2))
        const height=Math.max(0,Math.min(maximumHeight,
                        composerHeight-top-footer-3*12))
        assert.ok(width+inset*2<=composerWidth)
        assert.ok(top>=triggerY+triggerHeight)
        assert.ok(height===0 || top+height+footer<=composerHeight)
      }
    }
  }
  assert.match(qml,/objectName: "mobileComposerSelectorSlot"/)
  assert.match(qml,/Layout.preferredHeight: visible[\s\S]*?channelPicker.visible \? channelPicker.height : topicPicker.height/)
  assert.match(qml,/id: topicSearch[\s\S]*?visible: !root.compact/)
  assert.match(qml,/objectName: "mobileComposerStickyFooter"[\s\S]*?!root.mobileSelectorOpen/)
})

test('mobile composer uses app canvas and neutral selector surfaces without removing desktop editor chrome',()=>{
  assert.match(qml,/background: PopupSurface \{[\s\S]*?fillColor: root.compact \? Theme.background : Theme.surfaceElevated/);
  assert.match(qml,/objectName: "mobileCastContext"[\s\S]*?Layout.topMargin: Theme.space1/);
  assert.match(qml,/id: selectedChannelChip[\s\S]*?Layout.preferredHeight: Theme.touchTarget/);
  assert.match(qml,/id: selectedTopicChip[\s\S]*?Layout.preferredHeight: Theme.touchTarget/);
  assert.match(qml,/id: selectedChannelChip[\s\S]*?borderColor: root.selectedParentUrl.length > 0[\s\S]*?Theme.accent : Theme.borderMuted/);
  assert.match(qml,/root.mobileSelectorOpen[\s\S]*?channelPicker.visible/);
  assert.match(qml,/onOpened: \{\s*topicPicker.close\(\)/);
  assert.match(qml,/onOpened: \{\s*channelPicker.close\(\)/);
  assert.match(qml,/if \(!root.opened \|\| !editor \|\| !editor.activeFocus[\s\S]*?root.mobileSelectorOpen/);
  assert.match(qml,/visible: root.compact && root.editorMode === "visual"\s*&& !root.mobileSelectorOpen/);
});
