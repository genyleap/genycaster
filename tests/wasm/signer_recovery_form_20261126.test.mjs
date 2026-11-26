import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
test('recovery input uses a consistent filled password mask',()=>{
    const field=read('ui/components/controls/BidiTextField.qml');
    assert.match(field,/passwordCharacter: "\\u2022"/);
    assert.match(field,/passwordMaskDelay: 0/);
});
test('signer recovery provides protocol-aware instructions and word count',()=>{
    const shell=read('ui/components/shell/AppShell.qml');
    assert.match(shell,/objectName: "signerRecoveryWordCount"/);
    assert.match(shell,/acceptsLegacy12\s*\? qsTrId\("genycaster.setup.farcaster_recovery_placeholder"\)/);
    assert.match(shell,/objectName: "signerToggleRecoveryVisibility"/);
    assert.match(shell,/words.length === 24\s*\|\| \(acceptsLegacy12 && words.length === 12\)/);
});
