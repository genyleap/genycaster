import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
test('Plain, readable studio credits precede actual WASM progress without decoration',()=>{
  const html=source('ui/wasm/genycaster-shell.html');
  const js=source('ui/wasm/genycaster-loader.js');
  const pack=source('ci/package_wasm_assets.py');
  const cmake=source('ui/CMakeLists.txt');
  for(const brand of ['genycaster-boot-icon.svg','intro-base.svg','intro-farcaster.svg','intro-hypersnap.svg','intro-openproof.svg']) {
    assert.match(html,new RegExp(brand.replace('.','\\.')));
    assert.match(pack,new RegExp(brand.replace('.','\\.')));
    assert.match(cmake,new RegExp(brand.replace('.','\\.')));
    assert.match(source('ui/wasm/'+brand),/<(?:[a-z0-9]+:)?svg/);
  }
  for(const label of ['Genyleap Software Revolution','presents','HyperSnap','Farcaster','Base','OpenProof'])
    assert.ok(html.includes(label),label);
  for(const removed of ['studio-node','studio-hairline','studio-mark','studio-ecosystem-title',
    'studioBreathe','Built for an open ecosystem','genyleap-mark-dark.svg'])
    assert.ok(!html.includes(removed),removed);
  const intro=html.split('<div id="genycaster-studio-intro"')[1].split('<div class="boot-content">')[0];
  assert.equal((intro.match(/class="studio-brand-logo"/g)||[]).length,4);
  assert.match(html,/grid-template-columns:repeat\(4,minmax\(0,1fr\)\)/);
  assert.doesNotMatch(intro,/<small\b|studio-node|studio-card/);
  assert.ok(!intro.includes('Blockchain'));
  assert.match(html,/#genycaster-studio-intro\{[^}]*background:#0b0c0f/);
  assert.match(js,/const studioSceneMs=2700/);
  assert.match(js,/const studioPartnersMs=5500/);
  assert.match(js,/studioSceneMs\+studioPartnersMs-elapsed/);
  assert.match(js,/splashMinimumReached=true/);
  assert.match(js,/!splashMinimumReached\)return/);
  assert.match(js,/const splashSceneMs=2600/);
  assert.match(js,/Updating Software…/);
  assert.match(js,/Initializing Qt and WebAssembly/);
  assert.match(js,/received\*100\/total/);
  assert.match(html,/__genycasterIntroStartedAt=performance.now\(\)/);
  assert.ok(html.indexOf('src="genycaster-assets.js"')<html.indexOf('src="qtloader.js"'));
  assert.doesNotMatch(source('ui/wasm/genycaster-boot-icon.svg'),/fill="#101010"/);
  assert.match(source('ui/wasm/genycaster-boot-icon.svg'),/fill="#989898"/);
  assert.doesNotMatch(html,/if \(seen && !\(current && previous && current!==previous\)\) return/);
});

test('Intro uses the Base network emblem from the project icon set, never the four-block app mark',()=>{
  const intro=source('ui/wasm/intro-base.svg');
  const canonical=source('ui/assets/icons/genyleap/icons/crypto/base.svg');
  const html=source('ui/wasm/genycaster-shell.html');
  assert.match(html,/<img class="studio-brand-logo" src="\.\/intro-base\.svg\?v=e9013740d52a5f1811aa"/);
  for(const geometry of ['viewBox="0 0 24 24"','<circle cx="12" cy="12" r="8"/>','<path d="M5 12h10.5"/>']) {
    assert.ok(canonical.includes(geometry),'expected Base geometry in the project icon set: '+geometry);
    assert.ok(intro.includes(geometry),'Intro must use the project Base emblem: '+geometry);
  }
  assert.match(intro,/stroke="#fff"/);
  assert.doesNotMatch(intro,/1280 417\.43|cls-1|<rect/);
});
test('Cloud schema isolates all intended owner features, not speculative leaderboards',()=>{
  const first=source('services/account-sync/migrations/001_bookmarks.sql');
  const second=source('services/account-sync/migrations/002_cloud_state.sql');
  const create=source('services/account-sync/migrations/000_create_database.sql');
  assert.match(create, /CREATE DATABASE %I/);
  assert.match(create, /:'target_database'/);
  assert.match(create, /\\gexec/);
  assert.doesNotMatch(create, /CREATE DATABASE [a-z][a-z0-9_]* WITH/);
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
  const cpp=source('ui/app/identity/connections.cpp');
  const qml=source('ui/components/profile/ConnectionsPopup.qml');
  const profile=source('ui/components/profile/ProfileSurface.qml');
  assert.match(cpp,/m_accountSubject\.toUtf8\(\)/);
  assert.match(cpp,/QCryptographicHash::Sha256/);
  assert.match(cpp,/!m_verified \|\| !isConnected\(QStringLiteral\("linkedin"\)\)/);
  assert.match(qml,/onClicked: picker\.manualLinkedInRequested/);
  assert.match(qml,/genycaster\.connections\.copy_add_your_public_linkedin_profile_url/);
  assert.match(profile,/genycaster\.profile\.network_manual_url/);
});
