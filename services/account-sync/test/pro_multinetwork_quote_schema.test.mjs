import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';

const migration=readFileSync(new URL('../migrations/016_pro_hypersnap_quotes.sql',import.meta.url),'utf8');
const original=readFileSync(new URL('../migrations/012_pro_billing.sql',import.meta.url),'utf8');
const store=readFileSync(new URL('../pg_store.mjs',import.meta.url),'utf8');
const api=readFileSync(new URL('../cloud_state.mjs',import.meta.url),'utf8');

test('v16 preserves the Farcaster check and permits only canonical HyperSnap native identity',()=>{
  assert.match(original,/subject text NOT NULL CHECK \(subject ~ '\^farcaster:/);
  assert.match(migration,/DROP CONSTRAINT pro_quotes_subject_check/);
  assert.match(migration,/ADD CONSTRAINT pro_quotes_subject_check CHECK/);
  assert.match(migration,/subject ~ '\^farcaster:\[1-9\]/);
  assert.match(migration,/issuer = 'hypersnap:native-registry'/);
  assert.match(migration,/subject ~ '\^hypersnap:\[1-9\]/);
  assert.match(migration,/BETWEEN 4294967296 AND 9007199254740991/);
  assert.match(migration,/SET LOCAL lock_timeout = '5s'/);
  assert.match(migration,/INSERT INTO schema_migrations\(version\) VALUES\(16\)/);
  assert.match(migration,/\bBEGIN;[\s\S]*COMMIT;\s*$/);
  assert.doesNotMatch(migration,/DROP TABLE|DELETE FROM|TRUNCATE|GRANT .* TO PUBLIC/);
});

test('quote routes gate the native subject schema; never alias HyperSnap to Farcaster',()=>{
  assert.match(store,/const proMultiProtocolQuoteReady=versions\.has\(16\)/);
  assert.match(store,/proMultiProtocolQuoteReady:\(\)=>proMultiProtocolQuoteReady/);
  assert.match(api,/owner\.issuer==='hypersnap:native-registry'[\s\S]{0,160}store\.proMultiProtocolQuoteReady\?\.\(\)===false/);
  assert.match(api,/billing_schema_upgrade_required/);
});
