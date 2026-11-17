import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('active setup stage Continue is brand-primary, not flat white', () => {
    const setup = read('ui/components/setup/InitialSetup.qml');
    const action = read('ui/components/controls/ActionButton.qml');
    const primary = read('ui/components/setup/SetupPrimaryButton.qml');
    const stage = setup.slice(setup.indexOf('SetupPrimaryButton {\n            id: stageAction'),
                              setup.indexOf('\n        }', setup.indexOf('SetupPrimaryButton {\n            id: stageAction')));
    assert.match(stage, /neutralPrimary: false/);
    assert.match(stage, /text: root\.actionText\(\)/);
    assert.match(stage, /onClicked: root\.nextStep\(\)/);
    assert.match(primary, /^ActionButton \{/m);
    assert.match(primary, /prominent: true/);
    assert.match(action, /neonColor: control\.neutralPrimary[\s\S]*?: control\.prominent\s*\?\s*Qt\.lighter\(Theme\.primaryFill, 1\.28\)/);
    assert.match(action, /if \(control\.prominent\)\s*return control\.down \? Theme\.primaryPressed/);
    assert.match(action, /control\.prominent\s*\?\s*Qt\.rgba\(Theme\.primaryFill\.r/);
});
