
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const bridge=readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
const dialog=readFileSync(new URL('../../ui/wasm/recovery-phrase-dialog.mjs',import.meta.url),'utf8');

test('recovery click gets a visible indicator before cold module loading',()=>{
  assert.match(bridge,/id='genycaster-recovery-opening'/);
  assert.match(bridge,/window\.requestAnimationFrame/);
  assert.match(bridge,/await import\('\.\/recovery-phrase-dialog\.mjs'\)/);
  assert.match(bridge,/layer\.remove\(\);\s*return await promptForFarcasterRecovery/);
  assert.match(bridge,/await openRecoveryDialog\(controller\.signal/);
});

test('HyperSnap Existing Account verifies native custody without mandatory signer KEY_ADD',()=>{
  const section=bridge.split('if(action==="recover-hypersnap"){')[1]
    ?.split('}else {')[0];
  assert.ok(section,'native recovery must exist');
  assert.match(section,/createBrowserNativeRegistry\(\)\.recover\(/);
  assert.doesNotMatch(section,/signer\.provision\(|current\.provision\(/);
  assert.match(bridge,/const verifiedFid=String\(confirmed\?\.fid\|\|''\)/);
  assert.match(bridge,/await device\.remember\(verifiedFid,protocol\)/);
});

test('native recovery errors are distinguished from signer and network availability',()=>{
  assert.match(dialog,/category==='recovery-unregistered'/);
  assert.match(dialog,/category==='recovery-network'/);
  assert.match(dialog,/HyperSnap registry lookup did not complete/);
  assert.match(dialog,/!category && \/does not control/);
});
