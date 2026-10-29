import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

test('OpenProof recovery module belongs to full and incremental WebAssembly release manifests', () => {
  const pack = readFileSync(new URL('../../ci/package_wasm_assets.py', import.meta.url), 'utf8');
  const cmake = readFileSync(new URL('../../ui/CMakeLists.txt', import.meta.url), 'utf8');
  const filename = '"openproof_recovery.mjs"';
  assert.equal(pack.split(filename).length-1, 2,
    'required in both public asset staging and manifest list');
  const sourceAt = pack.indexOf('if args.source:');
  const manifestAt = pack.indexOf('public = [');
  assert.ok(sourceAt >= 0 && manifestAt > sourceAt);
  assert.ok(pack.slice(sourceAt,manifestAt).includes(filename));
  assert.ok(pack.slice(manifestAt).includes(filename));
  assert.ok(cmake.includes('wasm/openproof_recovery.mjs'));
  const bridge = readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
  assert.ok(bridge.includes("import('./openproof_recovery.mjs')"));
});
