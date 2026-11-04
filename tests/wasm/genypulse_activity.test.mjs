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
  assert.match(profile,/verifiedGitHub: root.isOwnProfile/);
  assert.match(profile,/verifiedGithubActivityAccount/);
  assert.match(profile,/githubLinked: root.isOwnProfile/);
  assert.match(profile,/verifiedEthAddresses/);
});

test('Farcaster and GitHub have separate pagination, state, deduplication and time bounds',()=>{
  assert.match(source,/kFarcasterWindowDays = 365/);
  assert.match(source,/kGithubWindowDays = 30/);
  assert.match(source,/kCastPages = 40/);
  assert.match(source,/kGithubPages = 3/);
  assert.match(source,/m_castHashes.contains\(hash\)/);
  assert.match(source,/m_githubEvents.contains\(id\)/);
  assert.match(source,/publishFarcaster\(false\)/);
  assert.match(source,/publishGitHub\(false\)/);
  assert.match(source,/QNetworkRequest::ManualRedirectPolicy/);
  // Qt WASM may not author User-Agent; HyperSnap CORS does not allow it.
  assert.match(source,/#if !defined\(__EMSCRIPTEN__\)[\s\S]*?setRawHeader\("User-Agent"/);
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

test('both activity calendars use a dense, seven-row annual layout',()=>{
  assert.match(qml,/readonly property int githubYearStart:/);
  assert.match(qml,/readonly property int githubYearEnd:/);
  assert.match(qml,/githubGridStart: githubYearStart-/);
  assert.match(qml,/githubWeeks: Math.ceil\(\(githubYearEnd-githubGridStart\+1\)\/7\)/);
  assert.match(qml,/readonly property int castsYearStartDay: todayDay - 364/);
  assert.match(qml,/readonly property int firstDay: castsYearStartDay-/);
  assert.match(qml,/readonly property int weeks: Math.ceil\(\(todayDay-firstDay\+1\)\/7\)/);
  assert.match(qml,/genycaster.activity.range_year/);
  assert.match(qml,/genycaster.activity.casts/);
  assert.match(qml,/Theme.iconCompose/);
});

test('year-long Casts are paginated independently and partial history is identified',()=>{
  assert.match(source,/kFarcasterWindowDays = 365/);
  assert.match(source,/kCastPages = 40/);
  assert.match(source,/m_castOldestObservedAt/);
  assert.match(source,/knownFromDay/);
  assert.match(source,/if\(\(page\+1\)%4==0\)publishFarcaster\(false\)/);
  assert.match(qml,/root.farcasterData.status==="partial"/);
  assert.match(qml,/unobserved \? \(Theme.dark/);
  assert.match(qml,/qsTrId\("genycaster.activity.partial"\)/);
});

test('small SQUARE tiles are consistently bounded on both annual grids',()=>{
  assert.match(qml,/readonly property int githubGapSize: 3/);
  assert.match(qml,/readonly property int githubTileSize: Math.max\(root.compact \? 10 : 11,/);
  assert.match(qml,/readonly property int cellSize: Math.max\(4,Math.min\(heatmapMaxTile,/);
  assert.match(qml,/readonly property int githubTileWidth: githubTileSize/);
  assert.match(qml,/readonly property int githubTileHeight: githubTileSize/);
  assert.match(qml,/readonly property int cellHeight: cellSize/);
  assert.match(qml,/width: root.cellSize/);
  assert.match(qml,/height: root.cellHeight/);
  assert.match(qml,/width: root.githubTileWidth/);
  assert.match(qml,/height: root.githubTileHeight/);
  for(const width of [310,330,345,400,720,980]){
    for(const weeks of [43,44,52,53]){
      const maxTile=width>=600?18:10;
      const px=Math.max(4,Math.min(maxTile,Math.floor((width-(weeks-1))/weeks)));
      assert.ok(px>=4 && px<=18);
      assert.equal(Math.trunc(px),px);
      assert.ok(weeks*px+(weeks-1)<=width,'annual heatmap should not stretch '+width+'/'+weeks);
    }
  }
});

test('all GitHub time filters preserve the same COLORED annual calendar',()=>{
  const qmlCalendar=qml.slice(qml.indexOf('id: githubCalendarViewport'));
  assert.match(qmlCalendar,/model: root.githubWeeks/);
  assert.match(qmlCalendar,/inYear\s*\? root.githubTileColor/);
  assert.match(qmlCalendar,/opacity: root.githubRange==="1y" \|\| inRange \? 1 : 0.7/);
  assert.match(qmlCalendar,/border.width: root.githubRange!=="1y" && inRange \? 1 : 0/);
  assert.match(qmlCalendar,/enabled: parent.inYear/);
  assert.ok(!qmlCalendar.includes('color: inRange ? root.githubTileColor'));
  assert.ok(!qml.includes('githubGridStart: githubStartRangeDay-'));
  // Each filter changes the selected range and total, but never grid weeks.
  const gridDef=qml.slice(qml.indexOf('readonly property int githubYearStart:'),
    qml.indexOf('readonly property int githubVisibleTotal:'));
  assert.ok(!gridDef.includes('githubRange'));
  assert.match(qml,/githubStartRangeDay/);
  assert.match(qml,/githubLastRangeDay/);
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
  assert.match(qml,/visible: root.detailMode \|\| root.previewSource !== "github"/);
  assert.match(qml,/visible: root.detailMode \|\| \(root.previewSource === "github"/);
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
  assert.match(source,/cancelReply\(m_farcasterReply, this\)/);
  assert.match(source,/cancelReply\(m_githubReply, this\)/);
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
  assert.match(qml,/githubContributionData: \(genyPulseActivity \? genyPulseActivity.githubContributions/);
  assert.match(qml,/model: \["1m", "3m", "1y"\]/);
  assert.match(qml,/githubYear--/);
  assert.match(qml,/githubYear\+\+/);
  assert.match(qml,/Flickable \{/);
  assert.match(qml,/githubTileColor/);
  assert.ok(!qml.includes('genycaster.activity.events'));
  assert.ok(!qml.includes('genycaster.activity.github_window'));
});

test('cancelling an in-flight request is safe when abort emits finished synchronously',()=>{
  const cancel=source.slice(source.indexOf('void cancelReply('),source.indexOf('QVariantMap statusMap('));
  assert.ok(cancel.length>100);
  const assign=cancel.indexOf('tracked.clear()');
  const disconnect=cancel.indexOf('QObject::disconnect(');
  const abort=cancel.indexOf('pending->abort()');
  const deletion=cancel.indexOf('if (pending) pending->deleteLater()');
  assert.ok(assign>0 && disconnect>assign && abort>disconnect && deletion>abort,
    'clear pointer and disconnect finished before calling potentially reentrant abort');
  for(const reply of ['m_farcasterReply','m_githubReply',
                      'm_githubResolveReply','m_githubContributionsReply']) {
    assert.ok(source.includes('cancelReply('+reply+', this)'), 'missing safe cancel for '+reply);
    assert.ok(!source.includes(reply+'->abort('),
      'do not call abort on a tracked member pointer');
  }
  assert.match(source,/if \(pending\) pending->deleteLater\(\)/);
});

test('network completions never read from an already closed QNetworkReply',()=>{
  const reads=[...source.matchAll(/reply->readAll\(\)/g)].length;
  const guarded=[...source.matchAll(/reply->isReadable\(\)/g)].length;
  assert.equal(reads,4,'Farcaster, GitHub events, GitHub ID and contribution reads');
  assert.equal(guarded,reads,
    'every network response must check QIODevice::isReadable before readAll');
});
