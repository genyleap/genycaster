import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
test('Genyleap studio ident precedes measured WASM loading without faked percent',()=>{
  const html=source('ui/wasm/genycaster-shell.html');
  const js=source('ui/wasm/genycaster-loader.js');
  const pack=source('tools/package_wasm_assets.py');
  const cmake=source('ui/CMakeLists.txt');
  for(const brand of ['genyleap-mark-dark.svg','genyleap-mark-light.svg']) {
    assert.match(html,new RegExp(brand.replace('.','\\.')));
    assert.match(pack,new RegExp(brand.replace('.','\\.')));
    assert.match(cmake,new RegExp(brand.replace('.','\\.')));
    assert.match(source('ui/wasm/'+brand),/<svg/);
  }
  for(const label of ['Genyleap Software Revolution','presents','Blockchain','Farcaster','Base','OpenProof'])
    assert.ok(html.includes(label),label);
  assert.match(js,/installing &&/);
  assert.match(js,/prefers-reduced-motion: reduce/);
  assert.match(js,/studioIntro\.dataset\.scene='partners'/);
  assert.match(js,/Updating Software…/);
  assert.match(js,/Initializing Qt and WebAssembly/);
  assert.match(js,/received\*100\/total/);
  assert.match(js,/if\(studioIntro\)studioIntro\.hidden=true/);
});
test('Cloud schema isolates all intended owner features, not speculative leaderboards',()=>{
  const first=source('services/account-sync/migrations/001_bookmarks.sql');
  const second=source('services/account-sync/migrations/002_cloud_state.sql');
  const create=source('services/account-sync/migrations/000_create_database.sql');
  assert.match(create,/CREATE DATABASE genycaster_db/);
  assert.match(first,/CREATE TABLE IF NOT EXISTS bookmarks/);
  for(const table of ['connections','notifications','app_updates','tasks','drafts'])
    assert.match(second,new RegExp('CREATE TABLE IF NOT EXISTS '+table));
  assert.doesNotMatch(second,/CREATE TABLE IF NOT EXISTS (users|streaks|leaderboards)/);
  assert.match(second,/manual_profile_url/);
  assert.match(second,/provider_profile_url/);
  assert.match(second,/idempotency_key/);
  assert.match(second,/revision bigint/);
  assert.match(second,/published_at/);
});
test('Manual LinkedIn edits are owner-gated local state, not a provider verification claim',()=>{
  const cpp=source('ui/mobile/openproof_connections_controller.cpp');
  const qml=source('ui/components/AccountConnectionsPopup.qml');
  const profile=source('ui/components/ProfileSurface.qml');
  assert.match(cpp,/m_accountSubject\.toUtf8\(\)/);
  assert.match(cpp,/QCryptographicHash::Sha256/);
  assert.match(cpp,/!m_verified \|\| !isConnected\(QStringLiteral\("linkedin"\)\)/);
  assert.match(qml,/onClicked: picker\.manualLinkedInRequested/);
  assert.match(qml,/saved on this device/);
  assert.match(profile,/URL entered by you \(not LinkedIn-verified\)/);
});
