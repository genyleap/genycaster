import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import vm from 'node:vm';

// Wheel input now belongs to the dedicated C++26 web wheel module.
// Exercise the embedded DOM-coordinate mapping without assuming main.cpp owns it.
const main = readFileSync(new URL('../../ui/app/platform/wheel.cpp', import.meta.url), 'utf8');
const beginMarker = '    return EM_ASM_DOUBLE({';
const begin = main.indexOf(beginMarker);
const end = main.indexOf('    }, clientX, clientY, axis);', begin);
assert.ok(begin !== -1 && end > begin, 'native Emscripten position mapping exists');
const js = main.slice(begin + beginMarker.length, end);
const compiledMacro = main.slice(begin, end + '    }, clientX, clientY, axis);'.length);
assert.match(main, /emscripten_set_wheel_callback\(/);
assert.match(main, /EMSCRIPTEN_EVENT_TARGET_WINDOW/);
assert.doesNotMatch(main, /Module\['_genycaster_browser_wheel'\]/);

const sourceFor = body => [
    '#define EM_ASM_DOUBLE(code, ...) 0.0',
    'double wheelPosition(int clientX, int clientY, int axis) {',
    body,
    '}',
].join('\n');

test('Emscripten native callback code passes the C++ preprocessor', () => {
    const compiler = process.env.CXX || 'c++';
    const result = spawnSync(compiler, ['-std=c++23', '-x', 'c++', '-fsyntax-only', '-'], {
        input: sourceFor(compiledMacro), encoding: 'utf8',
    });
    assert.ifError(result.error);
    assert.equal(result.status, 0, result.stderr);
});

function environment() {
    class HTMLCanvasElement {
        getBoundingClientRect() {
            return { left: 100, top: 50, width: 800, height: 400 };
        }
    }
    const canvas = new HTMLCanvasElement();
    let selected = canvas;
    const host = { shadowRoot: { elementFromPoint: () => canvas } };
    const document = { elementFromPoint: () => selected };
    const window = {};
    const execute = (x, y, axis) => vm.runInNewContext(
        '(function($0, $1, $2) {' + js + '})(' + [x,y,axis].join(',') + ')',
        { window, document, HTMLCanvasElement },
    );
    return { execute, canvas, host, window, select: element => { selected = element; } };
}

test('mouse coordinates are normalized to actual canvas CSS bounds', () => {
    const { execute } = environment();
    assert.equal(execute(500, 250, 0), 0.5);
    assert.equal(execute(500, 250, 1), 0.5);
    assert.equal(execute(100, 50, 0), 0);
    assert.equal(execute(900, 450, 1), 1);
});

test('shadow-root Qt screen coordinates resolve to its canvas', () => {
    const { execute, host, select } = environment();
    select(host);
    assert.equal(execute(500, 250, 0), 0.5);
});

test('Qt window wrappers resolve a canvas that ignores pointer events', () => {
    const { execute, canvas, select } = environment();
    const owner = { querySelector: selector => selector === 'canvas.qt-window-canvas' ? canvas : null };
    select({closest: selector => selector === '.qt-window' ? owner : null});
    assert.equal(execute(500, 250, 0), 0.5);
    assert.equal(execute(500, 250, 1), 0.5);
});

test('other page targets never forward to the Qt event loop', () => {
    const { execute, select } = environment();
    select({ tagName: 'DIV' });
    assert.equal(execute(500, 250, 0), -1);
});

test('captured composed-path canvas coordinates win over retargeted document hit', () => {
    const { execute, select, window } = environment();
    select({ tagName: 'QT-SCREEN-HOST' });
    window.__genycasterWheelHit = {
        clientX: 500, clientY: 250, x: 0.5, y: 0.5
    };
    assert.equal(execute(500, 250, 0), 0.5);
    assert.equal(execute(500, 250, 1), 0.5);
    assert.equal(execute(600, 250, 0), -1, 'stale cached coordinates are rejected');
});

const consumeStart = main.indexOf('void consumeBrowserWheelEvent(');
const consumeBody = main.indexOf('    EM_ASM({', consumeStart);
const consumeEnd = main.indexOf('    }, clientX, clientY);', consumeBody);
assert.ok(consumeStart >= 0 && consumeEnd > consumeBody);
const consumeJs = main.slice(consumeBody + '    EM_ASM({'.length, consumeEnd);

test('owned DOM tick stops native redelivery and is cleared exactly once', () => {
    for (const cancelable of [true, false]) {
        const calls = {stopped: 0, prevented: 0};
        const event = {clientX: 500, clientY: 250, cancelable,
            stopImmediatePropagation() { ++calls.stopped; },
            preventDefault() { ++calls.prevented; }};
        const window = {__genycasterWheelEvent: event,
            __genycasterWheelDiagnostics: {forwarded: 0}};
        const consume = () => vm.runInNewContext(consumeJs, {window, $0: 500, $1: 250});
        consume();
        consume();
        assert.equal(calls.stopped, 1);
        assert.equal(calls.prevented, 0, "the passive Emscripten callback never prevents default");
        assert.equal(window.__genycasterWheelEvent, null);
    }
});

test('fractional ticks are owned before the zero-pixel return and never synthesize a second scroll', () => {
    const callback = main.slice(main.indexOf('EM_BOOL browserWheelCallback('),
        main.indexOf('void installBrowserWheelBridge('));
    const consume = callback.indexOf('consumeBrowserWheelEvent(');
    const zero = callback.indexOf('if (pixels.isNull())');
    const queue = callback.indexOf('pendingWheelInput.pixels += pixels');
    assert.ok(consume > 0 && zero > consume && queue > zero);
    assert.match(callback.slice(zero, queue), /return EM_FALSE;/);
    const flush = main.slice(main.indexOf('void flushBrowserWheelInput('),
        main.indexOf('EM_BOOL deliverBrowserWheelFrame('));
    const synthetic = flush.indexOf('QCoreApplication::sendEvent(');
    assert.ok(synthetic > 0);
    assert.doesNotMatch(flush.slice(synthetic), /return EM_FALSE|wheel.isAccepted/);
    assert.ok(callback.indexOf('event->mouse.ctrlKey) return EM_FALSE') < consume,
        'browser zoom stays outside the application-owned path');
});


test('the active DOM observer cancels canvas scrolling while preserving zoom and outside input', () => {
    const start = main.indexOf("window.addEventListener('wheel', function(event) {");
    const end = main.indexOf('}, {capture: true, passive: false});', start);
    assert.ok(start >= 0 && end > start);
    const listener = main.slice(start, end + '}, {capture: true, passive: false});'.length);
    class HTMLCanvasElement {
        getBoundingClientRect() { return {left: 0, top: 0, width: 800, height: 600}; }
    }
    let observed;
    let options;
    const window = {addEventListener(type, callback, config) { observed = callback; options = config; }};
    vm.runInNewContext(listener, {window, HTMLCanvasElement});
    assert.equal(options.capture, true);
    assert.equal(options.passive, false);
    for (const [canvas, ctrlKey, deltaY, expected] of [
        [true, false, .25, true], [true, true, 20, false],
        [false, false, 20, false], [true, false, NaN, false],
    ]) {
        let prevented = false;
        observed({clientX: 100, clientY: 100, deltaX: 0, deltaY, ctrlKey, cancelable: true,
            composedPath: () => canvas ? [new HTMLCanvasElement()] : [{}],
            preventDefault() { prevented = true; }});
        assert.equal(prevented, expected);
    }
    let prevented = false;
    const owner = {classList: {contains: name => name === 'qt-window'},
        querySelector: selector => selector === 'canvas.qt-window-canvas' ? new HTMLCanvasElement() : null};
    observed({clientX: 100, clientY: 100, deltaX: 0, deltaY: .25, ctrlKey: false, cancelable: true,
        composedPath: () => [{}, owner, {}],
        preventDefault() { prevented = true; }});
    assert.equal(prevented, true, 'Qt wrapper input is owned even when the canvas has pointer-events:none');
});
