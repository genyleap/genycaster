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

test('both dropdowns are parented directly to their live trigger, not window overlay', () => {
    for (const [header,kind,limit] of [[body,'Channel',390],[topics,'Topic',330]]) {
        assert.match(header,/parent: triggerItem/)
        assert.match(header,/popupType: Popup\.Item/)
        assert.doesNotMatch(header,/parent: Overlay\.overlay/)
        assert.match(header,/y: triggerItem \? triggerItem\.height \+ Theme\.space1 : 0/)
        assert.match(header,/root\.contentItem\.width - boundaryInset \* 2/)
        assert.match(header,/root\.contentItem\.height - boundaryInset - triggerOrigin\.y - y/)
        assert.doesNotMatch(header,/below >= height|pt\.y - height/)
        assert.ok(header.includes('Math.min(' + limit + ','))
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

test('dropdown stays under trigger and wholly inside composer across sizes and RTL', () => {
    const inset=24, gap=4
    for(const composerWidth of [300,320,390,480,600,720]){
      for(const composerHeight of [310,420,600,720]){
        for(const triggerX of [24,120,210]){
          for(const rtl of [false,true]){
            const triggerWidth=94, triggerHeight=36, triggerY=140
            for(const maximumHeight of [330,390]){
              const width=Math.max(0,Math.min(410,composerWidth-inset*2))
              const leftLimit=inset-triggerX
              const rightLimit=composerWidth-inset-triggerX-width
              const preferred=rtl?triggerWidth-width:0
              const x=Math.max(leftLimit,Math.min(Math.max(leftLimit,rightLimit),preferred))
              const y=triggerHeight+gap
              const height=Math.max(0,Math.min(maximumHeight,composerHeight-inset-triggerY-y))
              const globalLeft=triggerX+x
              const globalTop=triggerY+y
              assert.ok(globalLeft>=inset-0.01)
              assert.ok(globalLeft+width<=composerWidth-inset+0.01)
              assert.ok(globalTop>=triggerY+triggerHeight)
              assert.ok(globalTop+height<=composerHeight-inset+0.01 || height===0)
            }
          }
        }
      }
    }
})
