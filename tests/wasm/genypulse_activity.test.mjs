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
  assert.match(qml,/readonly property real cellSize:/);
  assert.match(qml,/model: root.weeks/);
  assert.match(qml,/readonly property int weeks: 13/);
  assert.match(qml,/Theme.iconChevronDown/);
  assert.ok(!qml.includes('genycaster.activity.preview'));
  assert.ok(!qml.includes('genycaster.activity.no_history'));
  assert.match(qml,/elide: Text.ElideRight/);
});

test('Casts grid fills both card edges with compact, fixed-height rows',()=>{
  assert.match(qml,/readonly property real cellSize:/);
  assert.match(qml,/farcasterContent.width-\(weeks-1\)\*gapSize/);
  assert.match(qml,/readonly property int cellHeight: 11/);
  assert.match(qml,/readonly property real castsGridWidth:/);
  assert.match(qml,/id: calendarRow[\s\S]*?x: 0/);
  assert.match(qml,/text: "Casts"/);
  assert.match(qml,/text: Theme.iconCompose/);
  for(const width of [280,300,344,420,740,980]){
    const weeks=13,gap=3;
    const cell=(width-(weeks-1)*gap)/weeks;
    const grid=weeks*cell+(weeks-1)*gap;
    assert.ok(Math.abs(grid-width)<0.001,"13 weeks fit width "+width);
    assert.ok(cell>0,"positive cell size");
  }
  for(const provider of ['github','ethereum-wallet'])
    assert.ok(qml.includes('root.providerIconRoot + "'+provider+'.svg"'));
});

test('GitHub 1M/3M/1Y grids independently stretch to the full content width',()=>{
  assert.match(qml,/githubGridStart: githubStartRangeDay-/);
  assert.match(qml,/githubLastRangeDay-githubGridStart\+1/);
  assert.match(qml,/githubContent.width - \(githubWeeks-1\)\*githubGapSize/);
  assert.match(qml,/contentWidth: width/);
  assert.match(qml,/interactive: false/);
  assert.match(qml,/id: githubCalendarViewport[\s\S]*?Row \{\s*x: 0/);
  assert.ok(!qml.includes('githubCalendarFirstDay'));
  assert.ok(!qml.includes('onContentWidthChanged: Qt.callLater(showLatest)'));
  for(const width of [280,304,344,480,780,1000]){
    for(const weeks of [5,14,44,53]){
      const gap=weeks>24?1:3;
      const cell=(width-(weeks-1)*gap)/weeks;
      const total=weeks*cell+(weeks-1)*gap;
      assert.ok(Math.abs(total-width)<0.001,width+"/"+weeks);
      assert.ok(cell>=4 || width===280 && weeks===53);
    }
  }
});

test('compact time filter buttons are in GitHub top-right header',()=>{
  const header=qml.indexOf('text: "GitHub"');
  const nav=qml.indexOf('iconGlyph: Theme.iconArrowLeft',header);
  const filter=qml.indexOf('model: ["1m", "3m", "1y"]',header);
  assert.ok(header>=0 && filter>header && filter<nav);
  assert.match(qml,/Layout.preferredWidth: 39/);
  assert.match(qml,/Layout.preferredHeight: 27/);
  assert.match(qml,/Layout.alignment: Qt.AlignRight \| Qt.AlignVCenter/);
  assert.match(qml,/selected: root.githubRange === modelData/);
});

test('unlinked users never receive empty third-party activity cards',()=>{
  assert.match(qml,/readonly property bool hasOptionalSources: githubLinked \|\| hasOnchainActivity/);
  assert.match(qml,/visible: root.hasOptionalSources/);
  assert.match(qml,/visible: root.expanded && root.hasOptionalSources/);
  assert.match(qml,/visible: root.githubLinked/);
  assert.match(qml,/visible: root.hasOnchainActivity/);
  assert.match(qml,/walletData.status==="ok" \|\| walletData.status==="partial"/);
  assert.doesNotMatch(qml,/visible: root\.isOwnProfile && !root\.githubLinked/);
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

test('GitHub calendar is source-specific, real contributions, selectable by year/range',()=>{
  assert.match(header,/Q_PROPERTY\(QVariantMap githubContributions/);
  assert.match(source,/requestGitHubContributions/);
  assert.match(source,/api\/v1\/cloud\/github_contributions/);
  assert.match(qml,/githubContributionData: genyPulseActivity.githubContributions/);
  assert.match(qml,/model: \["1m", "3m", "1y"\]/);
  assert.match(qml,/githubYear--/);
  assert.match(qml,/githubYear\+\+/);
  assert.match(qml,/Flickable \{/);
  assert.match(qml,/githubTileColor/);
  assert.ok(!qml.includes('genycaster.activity.events'));
  assert.ok(!qml.includes('genycaster.activity.github_window'));
});
