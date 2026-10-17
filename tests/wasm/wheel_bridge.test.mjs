import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import vm from 'node:vm';

const main = readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8');
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
    const execute = (x, y, axis) => vm.runInNewContext(
        '(function($0, $1, $2) {' + js + '})(' + [x,y,axis].join(',') + ')',
        { document, HTMLCanvasElement },
    );
    return { execute, canvas, host, select: element => { selected = element; } };
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

test('other page targets never forward to the Qt event loop', () => {
    const { execute, select } = environment();
    select({ tagName: 'DIV' });
    assert.equal(execute(500, 250, 0), -1);
});
