import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {existsSync} from 'node:fs';

const labelPath=new URL('../../ui/components/DirectionAwareLabel.qml',import.meta.url);
const label=readFileSync(labelPath,'utf8');

test('shared QQC2 Label has no unsupported textInteractionFlags assignment',()=>{
  assert.match(label,/^Label\s*\{/m);
  assert.doesNotMatch(label,/^\s*textInteractionFlags\s*:/m);
});

test('Qt 6 QML property analyzer does not report unknown shared Label properties',()=>{
  const linter='/opt/genycaster/Qt/6.12.0/gcc_64/bin/qmllint';
  if (!existsSync(linter)) return; // source guard still runs on all platforms
  const result=spawnSync(linter,[new URL('../../ui/components/DirectionAwareLabel.qml',import.meta.url).pathname],{
    encoding:'utf8',timeout:20000
  });
  assert.equal(result.error,undefined, String(result.error));
  assert.notEqual(result.status,null,'qmllint timed out');
  assert.doesNotMatch((result.stderr||'')+'\n'+(result.stdout||''),/\[missing-property\]|Could not find property/,
    'missing QML properties are runtime-fatal even if qmlcachegen compiles them');
});
