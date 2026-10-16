import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Exercise the exact capture-phase DOM callback embedded in the Qt WASM
// composition root, not an independently reimplemented mock handler.
const main = readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8');
const marker = '    EM_ASM({\n        if (document.__genycasterWheelBridgeInstalled)';
const start = main.indexOf(marker);
assert.ok(start > 0, 'the WASM document wheel bridge exists');
const bodyStart = start + '    EM_ASM({'.length;
const bodyEnd = main.indexOf('\n    });\n}\n}\n#endif', bodyStart);
assert.ok(bodyEnd > bodyStart, 'the bridge has an intact closing boundary');
const handlerCode = main.slice(bodyStart, bodyEnd);

class Element {
  constructor(tagName, rect) {
    this.tagName = tagName;
    this.rect = rect;
  }
  closest(selector) { return selector === 'canvas' && this.tagName === 'canvas' ? this : null; }
  getBoundingClientRect() { return this.rect; }
}

function fixture() {
  const rect = { left: 40, top: 20, width: 1000, height: 500 };
  const canvas = new Element('canvas', rect);
  const document = {
    getElementById(id) { return id === 'qtcanvas' ? {
      contains: target => target === canvas,
      getBoundingClientRect: () => rect,
    } : null; },
    addEventListener(name, callback, options) {
      assert.equal(name, 'wheel');
      assert.deepEqual(Object.keys(options).sort(), ['capture', 'passive']);
      assert.equal(options.capture, true);
      assert.equal(options.passive, false);
      this.handler = callback;
    },
  };
  const forwarded = [];
  const Module = { _genycaster_browser_wheel: (...args) => forwarded.push(args) };
  const context = vm.createContext({ document, Module, Element });
  vm.runInContext('(function() {' + handlerCode + '\n})()', context);
  return { document, forwarded, canvas };
}

function event(target, overrides = {}) {
  let prevented = 0;
  let stopped = 0;
  return {
    target, clientX: 540, clientY: 270, deltaX: 0, deltaY: 60,
    deltaMode: 0, ctrlKey: false, shiftKey: false, altKey: false, metaKey: false,
    preventDefault() { prevented++; },
    stopImmediatePropagation() { stopped++; },
    get prevented() { return prevented; },
    get stopped() { return stopped; },
    ...overrides,
  };
}

test('DOM wheel is captured once, mapped to Qt coordinates, and not sent to native Qt twice', () => {
  const { document, forwarded, canvas } = fixture();
  const first = event(canvas);
  document.handler(first);
  assert.equal(first.prevented, 1);
  assert.equal(first.stopped, 1);
  assert.equal(forwarded.length, 1);
  assert.equal(forwarded[0].join(','), '0.5,0.5,0,60,0');
  vm.runInNewContext('(function() {' + handlerCode + '\n})()', {
    document, Module: { _genycaster_browser_wheel() { throw Error('double listener'); } }, Element,
  });
  assert.equal(forwarded.length, 1);
});

test('line-mode wheel and modifiers are normalized while browser Ctrl-wheel remains native', () => {
  const { document, forwarded, canvas } = fixture();
  const lines = event(canvas, { deltaY: -3, deltaMode: 1, shiftKey: true, altKey: true });
  document.handler(lines);
  assert.equal(forwarded[0][3], -48);
  assert.equal(forwarded[0][4], 3);
  const zoom = event(canvas, { ctrlKey: true });
  document.handler(zoom);
  assert.equal(zoom.prevented, 0);
  assert.equal(forwarded.length, 1);
});

test('ordinary page content outside the Qt canvas does not lose scrolling', () => {
  const { document, forwarded } = fixture();
  const outside = event(new Element('div', {left: 0, top: 0, width: 100, height: 100}));
  document.handler(outside);
  assert.equal(outside.prevented, 0);
  assert.equal(forwarded.length, 0);
});
