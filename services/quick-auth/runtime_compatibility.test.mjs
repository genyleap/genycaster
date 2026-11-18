import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createClient, HubError, InvalidTokenError} from '@farcasterorg/quick-auth';

const lock = JSON.parse(readFileSync(new URL('./package-lock.json', import.meta.url), 'utf8'));

test('official patched Quick Auth and Zod versions are pinned through the wrapper', () => {
  const upstream = lock.packages['node_modules/@farcaster/quick-auth'];
  const zod = lock.packages['node_modules/zod'];
  assert.equal(upstream?.version, '0.1.0');
  assert.equal(zod?.version, '4.6.7');
  assert.equal(upstream?.dependencies?.zod, '^4.6.6');
  assert.ok(!Object.keys(lock.packages).some(path =>
    path.endsWith('/node_modules/zod') && path !== 'node_modules/zod'),
    'the old Zod 3 instance must not re-enter the production dependency graph');
});

test('the protocol and legacy Quick Auth client contract remains available', async () => {
  const client = createClient({hubUrl: 'http://127.0.0.1:9'});
  assert.equal(typeof client.verifyToken, 'function');
  assert.ok(HubError.prototype instanceof Error);
  assert.ok(InvalidTokenError.prototype instanceof Error);
  await assert.rejects(
    client.verifyToken({token: 'not-a-token', domain: 'genycaster.com'}),
    error => error instanceof InvalidTokenError,
    'malformed input must fail before a protocol hub request');
});
