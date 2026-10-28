import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {resolve, dirname} from 'node:path';
import {runInNewContext} from 'node:vm';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const qml=readFileSync(resolve(root,'ui/components/GenyPulsePanel.qml'),'utf8');
const profile=readFileSync(resolve(root,'ui/components/ProfileSurface.qml'),'utf8');
const source=readFileSync(resolve(root,'ui/mobile/genypulse_activity_controller.cpp'),'utf8');
const header=readFileSync(resolve(root,'ui/mobile/genypulse_activity_controller.hpp'),'utf8');

test('activity is read independently from profile timeline and reacts to FID',()=>{
  assert.ok(!qml.includes('root.rows') && !qml.includes('profileFeed'));
  assert.match(qml,/onProfileFidChanged: refreshTimer.restart/);
  assert.match(qml,/genyPulseActivity.requestFarcaster/);
  assert.match(qml,/genyPulseActivity.requestGitHub/);
  assert.match(qml,/genyPulseActivity.setVerifiedWallet/);
  assert.ok(!/XMLHttpRequest|fetch\(/.test(qml));
  assert.match(profile,/verifiedGitHub:\s*\{/);
  assert.match(profile,/verifiedGithubActivityAccount/);
  assert.match(profile,/githubLinked: root.verifiedGithubActivityAccount.linked/);
  assert.match(profile,/verifiedEthAddresses/);
});

test('Farcaster and GitHub have separate pagination, state, deduplication and time bounds',()=>{
  assert.match(source,/kFarcasterWindowDays = 91/);
  assert.match(source,/kGithubWindowDays = 30/);
  assert.match(source,/kCastPages = 12/);
  assert.match(source,/kGithubPages = 3/);
  assert.match(source,/m_castHashes.contains\(hash\)/);
  assert.match(source,/m_githubEvents.contains\(id\)/);
  assert.match(source,/publishFarcaster\(false\)/);
  assert.match(source,/publishGitHub\(false\)/);
  assert.match(source,/QNetworkRequest::ManualRedirectPolicy/);
  assert.match(source,/kMaxResponseBytes/);
  assert.match(header,/Q_PROPERTY\(QVariantMap farcaster/);
  assert.match(header,/Q_PROPERTY\(QVariantMap github/);
  assert.match(header,/Q_PROPERTY\(QVariantMap onchain/);
});

test('Onchain verified wallet cannot be confused with transactions',()=>{
  assert.match(source,/not-indexed/);
  assert.ok(!/m_onchain\.insert\(QStringLiteral\("count"\)/.test(source));
  assert.match(qml,/wallet_pending/);
});

test('compact cells and status positioning prevent oversized heatmap',()=>{
  assert.match(qml,/readonly property int cellSize:/);
  assert.match(qml,/model: root.weeks/);
  assert.match(qml,/readonly property int weeks: 13/);
  assert.match(qml,/Theme.iconChevronDown/);
  assert.ok(!qml.includes('genycaster.activity.preview'));
  assert.ok(!qml.includes('genycaster.activity.no_history'));
  assert.match(qml,/elide: Text.ElideRight/);
});

test('localization covers every activity string across twelve languages',()=>{
  const keys=[...qml.matchAll(/genycaster\.activity\.[a-z_]+/g)].map(m=>m[0]);
  for(const lang of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']){
    const ts=readFileSync(resolve(root,'ui/i18n/qml_'+lang+'.ts'),'utf8');
    for(const key of new Set(keys))
      assert.ok(ts.includes('id="'+key+'"'),lang+' missing '+key);
  }
});

test('normalization and query stay in native adapter, not UI',()=>{
  assert.match(source,/cast\.value\(QStringLiteral\("author"\)\)/);
  assert.match(source,/QDateTime::fromString/);
  assert.match(source,/m_farcasterReply->abort\(\)/);
  assert.match(source,/m_githubReply->abort\(\)/);
  assert.match(source,/statusMap\(QStringLiteral\("error"\)/);
  assert.match(source,/https:\/\/api\.github\.com\/users/);
});

test('GitHub reads canonical OpenProof snapshots even without an active grant',()=>{
  const openproof=readFileSync(resolve(root,'ui/mobile/openproof_connections_controller.cpp'),'utf8');
  assert.match(profile,/openProofConnections.connections/);
  const idSection=profile.slice(profile.indexOf('readonly property var verifiedGithubActivityAccount:'),
      profile.indexOf('function socialIcon(provider)'));
  assert.doesNotMatch(idSection,/openProofConnections.authorized/);
  assert.match(idSection,/entry.profileUrl/);
  assert.match(idSection,/entry.subject/);
  assert.match(openproof,/server-confirmed:/);
  assert.match(openproof,/for\(const auto& confirmed:current\)/);
  assert.match(openproof,/retained.push_back\(value\)/);
  assert.match(qml,/genyPulseActivity.requestGitHubId\(githubAccountId\)/);
});

test('GitHub numeric-id fallback stays provider-verified and bounded',()=>{
  assert.match(source,/requestGitHubId/);
  assert.match(source,/github.com\/user/); // Guarded source URL or documentation
  assert.match(source,/confirmedId!=id/);
  assert.match(source,/m_githubAccountId!=id/);
  assert.match(source,/kCacheMs/);
});
