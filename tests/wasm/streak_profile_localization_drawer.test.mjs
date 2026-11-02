import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=(path)=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const profile=read('ui/components/ProfileSurface.qml');
const card=read('ui/components/StreakCard.qml');
const langCodes=['en_US','fa_IR','de_DE','ja_JP','ko_KR','zh_CN',
  'hi_IN','ar_SA','es_ES','fr_FR','pt_BR','ru_RU'];
const keys=['title','subtitle','open','profile_badge','activity','creator',
  'loading','unavailable','day_in_row','days_in_row','best','next',
  'legendary','mon','tue','wed','thu','fri','sat','sun','creator_rule',
  'activity_rule','badges','show_less','see_all','badge_days',
  'leaderboard','top_100','leaderboard_description','join_leaderboard',
  'leave_leaderboard','leaderboard_empty','fid','leader_days'];
const qmlIds=[...new Set([...profile.matchAll(/qsTrId\("(genycaster\.streak\.[\w_]+)"\)/g),
  ...card.matchAll(/qsTrId\("(genycaster\.streak\.[\w_]+)"\)/g)].map(m=>m[1]))];

test('Streak no longer occupies the full profile timeline',()=>{
 assert.doesNotMatch(profile,/StreakCard\s*\{\s*objectName:\s*"ownerProfileStreak"/);
 assert.match(profile,/objectName:\s*"profileStreakBadge"/);
 const badge=profile.slice(profile.indexOf("id: profileStreakBadge"),
   profile.indexOf('objectName: "profileOwnerActions"'));
 assert.match(badge,/visible:\s*root\.isOwnProfile/);
 assert.match(badge,/onClicked:\s*\{[\s\S]*?streakDrawer\.open\(\)/);
 assert.match(profile,/id:\s*streakDrawer[\s\S]*?objectName:\s*"profileStreakDrawer"/);
 assert.match(profile,/bottomSheet:\s*mobilePresentation/);
 assert.match(profile,/StreakCard\s*\{\s*id:\s*streakDetails/);
 assert.match(profile,/onCloseRequested:\s*streakDrawer\.close\(\)/);
 assert.match(profile,/streakDrawer\.close\(\)/);
 assert.match(profile,/id:\s*profileStatsFlow/);
 assert.match(badge,/Theme\.warningSurface/);
 assert.match(badge,/Theme\.warning/);
 assert.match(profile,/Layout\.preferredHeight:\s*childrenRect\.height/);
 assert.match(profile,/streakDetails\.implicitHeight\s*\+\s*streakHeader\.implicitHeight/);
 assert.match(card,/layoutDirection:\s*Theme\.uiDirection/);
 assert.match(card,/objectName:\s*"streakWeeklyCalendar"/);
 assert.match(card,/week_saturday/);
 assert.match(card,/Theme\.warning/);
});

test('every Streak label is translated by stable translation ID',()=>{
 assert.ok(qmlIds.length>=26,'original Streak labels must remain translated');
 for(const key of keys.filter(k=>k!=='profile_badge'))
   assert.ok(qmlIds.includes('genycaster.streak.'+key),key);
 assert.doesNotMatch(card,/text:\s*"(?:Streak|Activity|Creator|Show less|See all|Badges)/);
 assert.match(card,/LocaleFormat\.formatInteger/);
});

test('all 12 shipping locale catalogs contain complete nonempty translated Streak strings',()=>{
 const sourceMap=new Map();
 for(const lang of langCodes){
  const ts=read('ui/i18n/qml_'+lang+'.ts');
  const matches=[...ts.matchAll(/<message id="genycaster\.streak\.([\w_]+)">\s*<source>([\s\S]*?)<\/source>\s*<translation>([\s\S]*?)<\/translation>\s*<\/message>/g)];
  assert.equal(matches.length,keys.length,lang+' must contain all translations');
  for(const [,key,source,translated] of matches){
   assert.ok(keys.includes(key),lang+' unknown key '+key);
   assert.ok(translated.trim().length>0,lang+' empty translation for '+key);
   const placeholders=(value)=>[...value.matchAll(/%\d+/g)].map(m=>m[0]).sort().join(',');
   assert.equal(placeholders(source),placeholders(translated),lang+' placeholder mismatch '+key);
   if(sourceMap.has(key))assert.equal(sourceMap.get(key),source,lang+' source differs '+key);
   else sourceMap.set(key,source);
  }
 }
});

test('Streak leaderboard renders public social identities and uses only the outer drawer scroll',()=>{
  const card=read('ui/components/StreakCard.qml');
  const adapter=read('ui/mobile/openproof_connections_controller.cpp');
  const cloud=read('services/account-sync/cloud_state.mjs');
  const notice=read('ui/components/UpdateNoticeDrawer.qml');
  const profile=read('ui/components/ProfileSurface.qml');
  assert.match(card,/Avatar\s*\{[\s\S]*?sourceUrl: String\(modelData\.avatar_url/);
  assert.match(card,/modelData\.display_name/);
  assert.match(card,/modelData\.username/);
  assert.match(card,/modelData\.longest/);
  assert.match(card,/root\.leaderboardEntries\.slice\(0, 3\)/);
  assert.doesNotMatch(card,/objectName: "streakTop100"[\s\S]*?ListView\s*\{/);
  assert.match(profile,/objectName: "profileStreakScroll"/);
  assert.match(profile,/interactive: contentHeight > height/);
  assert.match(notice,/objectName: "updateNoticeStickyActions"/);
  assert.match(notice,/Layout\.minimumHeight: 0/);
  const block=adapter.slice(adapter.indexOf('void OpenProofConnectionsController::fetchStreakLeaderboard'),
    adapter.indexOf('void OpenProofConnectionsController::fetchStreak()',adapter.indexOf('void OpenProofConnectionsController::fetchStreakLeaderboard')));
  assert.doesNotMatch(block,/cloudAuthorizationReady\(\)/);
  assert.doesNotMatch(block,/authorizeCloudRequest\(request\)/);
  assert.ok(cloud.indexOf("if(entity==='streak/leaderboard')")<
     cloud.indexOf('let owner;'));
});

test('Desktop Streak drawer spans overlay height and social rows follow a shared gutter',()=>{
 const source=read('ui/components/ProfileSurface.qml');
 const panel=source.slice(source.indexOf('id: streakDrawer'),source.indexOf('id: profileExtrasEditor'));
 assert.ok(panel.includes(': parent.height) : 600'));
 assert.ok(source.includes('Layout.topMargin: root.compact ? Theme.space1 : Theme.space3'));
 assert.ok(source.includes('profileStatsFlow.spacing * 3) : 0'));
});
test('Leaderboard separates latin handles and Persian FIDs with direction-aware layout',()=>{
 const source=read('ui/components/StreakCard.qml');
 assert.ok(source.includes('objectName: "streakLeaderboardIdentityRow"'));
 assert.ok(source.includes('forceLeftToRight: true'));
 assert.ok(source.includes('forceRightToLeft: Theme.rtl'));
 assert.ok(source.includes('Layout.alignment: Theme.rtl ? Qt.AlignLeft : Qt.AlignRight'));
 assert.ok(source.includes('LayoutMirroring.enabled: false'));
});

test('Streak mobile spacing, fire orb and expandable server-owned milestone grid',()=>{
 const source=read('ui/components/ProfileSurface.qml');
 const card=read('ui/components/StreakCard.qml');
 const theme=read('ui/theme/Theme.qml');
 const backend=read('services/account-sync/streaks.mjs');
 assert.match(source,/objectName: "profileOwnerActions"[\s\S]*?Layout.topMargin: Theme.space3/);
 assert.match(source,/profileStatsFlow.spacing \* 3\) : 0/);
 assert.match(source,/text: Theme.iconStreakFlame/);
 assert.match(card,/objectName: "streakFlameOrb"/);
 assert.match(card,/SequentialAnimation on scale/);
 assert.match(card,/!Theme.reduceMotion && !Theme.powerSave/);
 assert.match(card,/milestone_definitions/);
 assert.match(card,/objectName: "streakBadgeGrid"/);
 assert.match(card,/model: root.showAllBadges/);
 assert.match(card,/modelData.progress_days/);
 assert.match(card,/root.inspectedMilestone/);
 assert.match(theme,/iconStreakFlame: "system\/flame"/);
 assert.match(backend,/reward_kind:'none'/);
 assert.match(backend,/STREAK_BADGE_CATALOG/);
});
