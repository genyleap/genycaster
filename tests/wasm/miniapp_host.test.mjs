import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const hostSource = readFileSync(
    new URL('../../ui/wasm/miniapp-host-pre.js', import.meta.url), 'utf8');

function browser(width=1024) {
    const nodes = [];
    class Element {
        constructor(tagName) {
            this.tagName = tagName;
            this.style = {};
            this.attributes = {};
            this.children = [];
            this.isConnected = false;
            this.handlers = {};
            nodes.push(this);
        }
        setAttribute(key, value) { this.attributes[key] = value; }
        append(...children) { this.children.push(...children); }
        get firstChild() { return this.children[0] || null; }
        replaceChildren(...children) { this.children = [...children]; }
        remove() { this.isConnected = false; }
        addEventListener(event, handler) { this.handlers[event] = handler; }
        focus() { this.focused = true; }
    }
    const document = {
        createElement: name => new Element(name),
        getElementById: id => nodes.find(node => node.id === id) || null,
        head: { append(node) { node.isConnected = true; } },
        body: { append(node) { node.isConnected = true; } },
        documentElement: { getAttribute: () => 'dark' }
    };
    const window = { location: { origin: 'https://app.genycaster.example' }, innerWidth: width };
    vm.runInNewContext(hostSource, { window, document, URL,
        navigator: { language: 'en-US' },
        setTimeout: () => 1, clearTimeout: () => {} }, {
        filename: 'miniapp-host-pre.js',
    });
    return { controller: window.__genycasterMiniAppOverlay, nodes };
}

test('Mini App opens inside a sandboxed viewport, with no external tab', () => {
    const { controller, nodes } = browser();
    assert.equal(controller.open('https://gig-bot-2-0.vercel.app/', 'Gig Bot', 'بستن'), true);
    assert.equal(controller.active(), true);
    const frame = nodes.find(node => node.tagName === 'iframe');
    assert.equal(frame.src, 'https://gig-bot-2-0.vercel.app/');
    assert.equal(frame.attributes.referrerpolicy, 'no-referrer');
    assert.match(frame.attributes.sandbox, /allow-scripts/);
    assert.doesNotMatch(frame.attributes.sandbox, /allow-top-navigation/);
    assert.doesNotMatch(frame.attributes.sandbox, /allow-downloads/);
    // The in-app frame is the only launch surface; no unrelated provider choices.
    assert.doesNotMatch(hostSource, /window\.open\(url\.href/);
    assert.equal(nodes.find(node => node.tagName === 'strong').textContent, 'Gig Bot');
    const close = nodes.find(node => node.tagName === 'button' && node.attributes['aria-label'] === 'بستن');
    assert.equal(close.focused, true);
    assert.equal(close.attributes['aria-label'], 'بستن');
    controller.close();
    assert.equal(controller.active(), false);
});

test('private, malformed and insecure URL origins are rejected before embedding', () => {
    const { controller } = browser();
    for (const candidate of [
        'http://gig-bot-2-0.vercel.app', 'javascript:alert(1)',
        'https://localhost', 'https://127.0.0.1/app',
        'https://[::1]/', 'https://foo.internal/', 'https://foo.test',
        'https://example.com:8443', 'https://user:secret@example.com',
        'https://app.genycaster.example/embedded/',
    ]) assert.equal(controller.open(candidate, 'bad'), false, candidate);
    assert.equal(controller.active(), false);
});

test('one active Mini App at a time and close button releases the frame', () => {
    const { controller, nodes } = browser();
    assert.equal(controller.open('https://first.example.com/', 'First'), true);
    const first = nodes.find(node => node.tagName === 'iframe');
    assert.equal(controller.open('https://second.example.com/', 'Second'), true);
    assert.equal(first.parent, undefined);
    assert.equal(controller.active(), true);
    const button = nodes.find(node => node.tagName === 'button'
        && node.attributes['aria-label'] === 'Close');
    assert.ok(button);
    button.handlers.click();
    assert.equal(controller.active(), false);
});

test('Web Mini App follows presentation mode and provides in-place Full Screen',async()=>{
 const {controller,nodes}=browser();
 assert.equal(controller.open('https://tasks.miniapps.genyleap.com','GenyTask','Close','right'),true);
 assert.equal(controller.mode(),'right');
 const sheet=nodes.find(node=>node.tagName==='section');
 assert.equal(sheet.style.inset,'0 0 0 auto');
 assert.match(sheet.style.width,/424px/);
 assert.equal(nodes.some(n=>n.textContent==='Open in Farcaster'
     || n.textContent==='Open in browser'),false);
 assert.equal(nodes.filter(n=>n.tagName==='iframe').length,1);
 const button=nodes.find(n=>n.tagName==='button'
     && n.attributes['aria-label']==='Full screen');
 assert.ok(button);
 await controller.toggleFullscreen();
 assert.equal(controller.fullscreen(),true);
 assert.equal(sheet.style.width,'100%');
 await controller.toggleFullscreen();
 assert.equal(controller.fullscreen(),false);
 assert.equal(sheet.style.inset,'0 0 0 auto');
 controller.setMode('bottom');
 assert.equal(controller.mode(),'bottom');
 assert.match(sheet.style.height,/755px/);
 assert.equal(nodes.filter(n=>n.tagName==='iframe').length,1);
 controller.close();
 assert.equal(controller.active(),false);
});

test('mobile Mini Apps occupy the viewport regardless of the selected desktop mode',()=>{
 const {controller,nodes}=browser(390);
 assert.equal(controller.open('https://game.example.org/','Game','Close','right'),true);
 const sheet=nodes.find(n=>n.tagName==='section');
 assert.equal(sheet.style.inset,'0');
 assert.equal(sheet.style.width,'100%');
 assert.equal(sheet.style.height,'100%');
 assert.match(nodes.find(n=>n.tagName==='header').style.paddingTop,/safe-area-inset-top/);
 controller.setMode('bottom');
 assert.equal(sheet.style.height,'100%');
 controller.setMode('window');
 assert.equal(sheet.style.width,'100%');
 controller.close();
});

test('desktop window Mini Apps match native content width without stretching the iframe',()=>{
 const {controller,nodes}=browser(1440);
 assert.equal(controller.open('https://game.example.org/','Game','Close','window'),true);
 const sheet=nodes.find(n=>n.tagName==='section');
 assert.match(sheet.style.width,/424px/);
 assert.match(sheet.style.height,/755px/);
 assert.equal(nodes.filter(n=>n.tagName==='iframe').length,1);
 controller.close();
});

test('verified Pro supports four isolated web Mini App iframes and fails closed on a fifth',()=>{
 const {controller,nodes}=browser();
 controller.setLimit(4);
 const names=['alpha','beta','gamma','delta'];
 for(const name of names)
  assert.equal(controller.open('https://'+name+'.example.org/',name),true);
 const frames=nodes.filter(node=>node.tagName==='iframe');
 assert.equal(frames.length,4);
 assert.equal(frames.filter(x=>x.style.display==='block').length,1);
 assert.equal(frames.filter(x=>x.style.display==='none').length,3);
 assert.equal(controller.open('https://fifth.example.org/','fifth'),false);
 assert.equal(nodes.filter(node=>node.tagName==='iframe').length,4);
 controller.setLimit(1);
 assert.equal(controller.active(),true);
 assert.equal(frames[3].style.display,'block');
 assert.equal(controller.open('https://other.example.org/','Other'),true);
 assert.equal(controller.active(),true);
 controller.close();
 assert.equal(controller.active(),false);
});
