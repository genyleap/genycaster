import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const shell = readFileSync(new URL('../../ui/components/shell/AppShell.qml',import.meta.url),'utf8');

test('recovery word input guards popup teardown before reading activeWordCount', () => {
    assert.match(shell, /enabled:\s*!!interactionAuthorizationDialog\s*&&\s*index < interactionAuthorizationDialog\.activeWordCount/);
    assert.match(shell, /opacity:\s*interactionAuthorizationDialog\s*&&\s*index < interactionAuthorizationDialog\.activeWordCount\s*\?\s*1\s*:\s*0\.38/);
    assert.match(shell, /Keys\.onPressed:\s*function\(event\)\s*\{\s*if\s*\(!interactionAuthorizationDialog\)\s*return/);
    assert.match(shell, /property int activeWordCount:\s*24/);
    assert.match(shell, /acceptsLegacy12 && !force24Words[\s\S]*?first12 && !trailing \? 12 : 24/);
});
