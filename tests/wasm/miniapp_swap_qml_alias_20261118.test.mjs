import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('Swap preflight instantiates registered public QML type, never physical file name', () => {
    const qml = read('ui/components/apps/MiniAppSwapPreflight.qml');
    const aliases = JSON.parse(read('ui/components/source_paths.json'));
    const cmake = read('ui/CMakeLists.txt');

    // The physical file is BidiTextField.qml but its QML registration and
    // resource alias are DirectionAwareTextField.qml (all platforms).
    assert.equal(aliases['DirectionAwareTextField.qml'], 'controls/BidiTextField.qml');
    assert.match(cmake, /set_source_files_properties\("components\/controls\/BidiTextField\.qml" PROPERTIES QT_RESOURCE_ALIAS "components\/DirectionAwareTextField\.qml"\)/);
    assert.match(cmake, /QT_QML_SOURCE_TYPENAME/);
    assert.equal((qml.match(/\bDirectionAwareTextField\s*\{/g) || []).length, 4);
    assert.doesNotMatch(qml, /\bBidiTextField\s*\{/);
    assert.match(qml, /^import Farcaster\.Client\s*$/m);
});

test('Swap dialog and its canonical public QML types are part of the Qt module', () => {
    const cmake = read('ui/CMakeLists.txt');
    const aliases = JSON.parse(read('ui/components/source_paths.json'));
    for (const type of ['MiniAppSwapPreflight', 'GenyAdaptiveDialog',
                        'DirectionAwareLabel', 'DirectionAwareTextField',
                        'ActionButton']) {
        assert.ok(aliases[type + '.qml'], type + ' public alias is missing');
        assert.ok(cmake.includes('components/' + aliases[type + '.qml']),
                  type + ' physical source is not in Qt module');
    }
    assert.match(cmake, /components\/apps\/MiniAppSwapPreflight\.qml/);
});

test('Swap title uses an existing theme typography token', () => {
    const dialog = read('ui/components/apps/MiniAppSwapPreflight.qml');
    const theme = read('ui/theme/Theme.qml');
    assert.match(dialog, /font\.pixelSize: Theme\.typeTitle3Size/);
    assert.match(theme, /property int typeTitle3Size:/);
    assert.doesNotMatch(dialog, /Theme\.typeTitleSize\b/);
});
