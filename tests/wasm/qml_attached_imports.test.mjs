import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync, readdirSync} from 'node:fs';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';

const ui = fileURLToPath(new URL('../../ui/', import.meta.url));

function qmlFiles(directory) {
    const files = [];
    for (const entry of readdirSync(directory, {withFileTypes: true})) {
        const target = join(directory, entry.name);
        if (entry.isDirectory()) files.push(...qmlFiles(target));
        else if (entry.isFile() && entry.name.endsWith('.qml')) files.push(target);
    }
    return files;
}

test('QML modules that use Layout attached properties import QtQuick.Layouts explicitly', () => {
    const missing = [];
    for (const file of qmlFiles(ui)) {
        const source = readFileSync(file, 'utf8');
        if (/\bLayout\.(?:fillWidth|fillHeight|minimumWidth|minimumHeight|preferredWidth|preferredHeight|maximumWidth|maximumHeight)/.test(source)
            && !/^\s*import\s+QtQuick\.Layouts(?:\s|$)/m.test(source)) {
            missing.push(file.slice(ui.length));
        }
    }
    assert.deepEqual(missing, [], 'Qt Quick will fail with Non-existent attached object');
});

test('EmbedPreview is constrained without breaking the native QML loading contract', () => {
    const file = readFileSync(join(ui, 'components/media/EmbedPreview.qml'), 'utf8');
    assert.match(file, /^import QtQuick\s*\nimport QtQuick\.Layouts\s*\n/);
    assert.match(file, /Layout\.minimumWidth: 0/);
});
