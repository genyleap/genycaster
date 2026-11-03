import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const hostSource = readFileSync(
    new URL('../../ui/wasm/miniapp-host-pre.js', import.meta.url), 'utf8');

function browser() {
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
        remove() { this.isConnected = false; }
        addEventListener(event, handler) { this.handlers[event] = handler; }
        focus() { this.focused = true; }
    }
    const document = {
        createElement: name => new Element(name),
        body: { append(node) { node.isConnected = true; } },
    };
    const window = { location: { origin: 'https://app.genycaster.example' } };
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
    // The in-app frame is primary; a user-initiated external fallback is allowed.
    assert.match(hostSource, /window\.open\(url\.href/);
    assert.equal(nodes.find(node => node.tagName === 'strong').textContent, 'Gig Bot');
    const close = nodes.filter(node => node.tagName === 'button').at(-1);
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
    const button = nodes.filter(node => node.tagName === 'button').at(-1);
    button.handlers.click();
    assert.equal(controller.active(), false);
});

test('SDK-dependent Mini Apps expose an explicit real Farcaster host fallback',()=>{
 const opened=[];
 const nodes=[];
 class Element{
  constructor(tagName){
   this.tagName=tagName;this.style={};this.attributes={};this.children=[];
   this.isConnected=false;this.handlers={};this.textContent='';
   nodes.push(this);
  }
  setAttribute(key,value){this.attributes[key]=value;}
  append(...args){this.children.push(...args);}
  remove(){this.isConnected=false;}
  addEventListener(type,handler){this.handlers[type]=handler;}
  focus(){}
 }
 const win={
  location:{origin:'https://caster.genyleap.com'},
  open(...args){opened.push(args);}
 };
 const dom={
  createElement:name=>new Element(name),
  body:{append:node=>{node.isConnected=true;}}
 };
 vm.runInNewContext(hostSource,{window:win,document:dom,URL,
  navigator:{language:'en'},setTimeout:()=>1,clearTimeout:()=>{}});
 assert.equal(win.__genycasterMiniAppOverlay.open('https://tasks.miniapps.genyleap.com','GenyTask','Close'),true);
 const launch=nodes.find(n=>n.tagName==='button'&&n.textContent==='Open in Farcaster');
 assert.ok(launch,'real host must be discoverable without SDK emulation');
 launch.handlers.click();
 assert.match(opened[0][0],/^https:\/\/farcaster\.xyz\/\~\/mini-apps\/launch\?domain=tasks\.miniapps\.genyleap\.com$/);
 assert.equal(opened[0][2],'noopener,noreferrer');
 assert.doesNotMatch(hostSource,/window\.__genycasterFakeSDK/);
 assert.doesNotMatch(hostSource,/onload.*clearTimeout/);
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
