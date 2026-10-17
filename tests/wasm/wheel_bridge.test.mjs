import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import { spawnSync } from 'node:child_process';

// Extract and execute the actual browser callback embedded in the WASM
// composition root. A copied approximation would not catch broken selectors.
const main = readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8');
const marker = '    EM_ASM({\n        // Qt 6.12 creates canvas nodes dynamically';
const start = main.indexOf(marker);
assert.ok(start > 0, 'browser bridge is present');
const begin = start + '    EM_ASM({'.length;
const end = main.indexOf('\n    });\n}\n}\n#endif', begin);
assert.ok(end > begin, 'browser bridge has a closing boundary');
const code = main.slice(begin, end);

// Emscripten's EM_ASM macro treats commas outside parentheses as new
// C++ macro arguments. A JS engine happily executes an object literal that
// breaks em++ preprocessing; compile a host-side macro-arity probe as well.
test('EM_ASM JavaScript is exactly one C preprocessor argument', () => {
    const compiler = process.env.CXX || 'c++';
    const sourceFor = (javascript) => [
        '#define EM_ASM(code, ...) static_assert(sizeof(#__VA_ARGS__) == 1, "stray EM_ASM argument");',
        'void verify(){',
        '    EM_ASM({' + javascript + '\n    });',
        '}',
        '',
    ].join('\n');
    const compile = javascript => spawnSync(compiler,
        ['-std=c++17', '-x', 'c++', '-fsyntax-only', '-'],
        { input: sourceFor(javascript), encoding: 'utf8' });
    const actual = compile(code);
    assert.ifError(actual.error);
    assert.equal(actual.status, 0,
        'wheel bridge must survive C++ preprocessing:\n' + actual.stderr);

    // Prove the guard detects the exact regression seen on Qt 6.12 WASM.
    const badJs = code.replace('Object.create(null)',
        '{ received: 0, onCanvas: 0, forwarded: 0 }');
    assert.notEqual(badJs, code, 'negative fixture must inject a bare object literal');
    const bad = compile(badJs);
    assert.notEqual(bad.status, 0,
        'a bare object literal must be rejected before being passed to em++');
});

class HTMLCanvasElement {
    getBoundingClientRect() {
        return { left: 100, top: 50, width: 800, height: 400 };
    }
}

function fixture() {
    const canvas = new HTMLCanvasElement();
    const forwarded = [];
    const window = {
        addEventListener(name, handler, options) {
            assert.equal(name, 'wheel');
            assert.equal(options.capture, true);
            assert.equal(options.passive, false);
            this.handler = handler;
        },
    };
    const Module = { _genycaster_browser_wheel: (...args) => forwarded.push(args) };
    const context = vm.createContext({ window, Module, HTMLCanvasElement, console });
    const install = () => vm.runInContext('(function(){' + code + '\n})()', context);
    install();
    return { window, canvas, forwarded, install };
}

function wheel(path, options = {}) {
    const state = { prevented: 0, stopped: 0 };
    return {
        clientX: 500, clientY: 250,
        deltaX: 0, deltaY: 60, deltaMode: 0, ctrlKey: false,
        shiftKey: false, altKey: false, metaKey: false, defaultPrevented: false,
        composedPath: () => path,
        preventDefault() { ++state.prevented; },
        stopImmediatePropagation() { ++state.stopped; },
        get state() { return state; },
        ...options,
    };
}

test('Qt-owned canvas receives a single normalized wheel input', () => {
    const { window, canvas, forwarded, install } = fixture();
    const e = wheel([canvas, { id: 'some-qt-screen' }, window]);
    window.handler(e);
    assert.equal(e.state.prevented, 1);
    assert.equal(e.state.stopped, 1);
    assert.deepEqual(forwarded, [[0.5, 0.5, 0, 60, 0]]);
    assert.deepEqual({ ...window.__genycasterWheelDiagnostics },
        { received: 1, onCanvas: 1, forwarded: 1 });
    install();
    assert.equal(forwarded.length, 1, 'reinstall must not create duplicate handlers');
});

test('shadow DOM retargeting preserves canvas hit testing', () => {
    const { window, canvas, forwarded } = fixture();
    // Event.target is a shadow host; composedPath still points to the canvas.
    window.handler(wheel([canvas, { tagName: 'DIV' }, window], {
        target: { tagName: 'GENYCASTER-SCREEN' },
        deltaY: -3, deltaMode: 1, shiftKey: true,
    }));
    assert.deepEqual(forwarded, [[0.5, 0.5, 0, -48, 1]]);
});

test('browser zoom and unrelated document wheel remain unaffected', () => {
    const { window, canvas, forwarded } = fixture();
    const zoom = wheel([canvas, window], { ctrlKey: true });
    window.handler(zoom);
    const outside = wheel([{ tagName: 'DIV' }, window]);
    window.handler(outside);
    assert.equal(zoom.state.prevented, 0);
    assert.equal(outside.state.prevented, 0);
    assert.equal(forwarded.length, 0);
});
