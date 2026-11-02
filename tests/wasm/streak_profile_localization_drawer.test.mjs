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
  'activity_rule','badges','show_less','see_all','badge_days'];
const qmlIds=[...new Set([...profile.matchAll(/qsTrId\("(genycaster\.streak\.[\w_]+)"\)/g),
  ...card.matchAll(/qsTrId\("(genycaster\.streak\.[\w_]+)"\)/g)].map(m=>m[1]))];

test('Streak no longer occupies the full profile timeline',()=>{
 assert.doesNotMatch(profile,/StreakCard\s*\{\s*objectName:\s*"ownerProfileStreak"/);
 assert.match(profile,/objectName:\s*"profileStreakBadge"/);
 assert.match(profile,/visible:\s*root\.isOwnProfile[\s\S]{0,360}onClicked:\s*streakDrawer\.open\(\)/);
 assert.match(profile,/id:\s*streakDrawer[\s\S]*?objectName:\s*"profileStreakDrawer"/);
 assert.match(profile,/bottomSheet:\s*mobilePresentation/);
 assert.match(profile,/StreakCard\s*\{\s*id:\s*streakDetails/);
 assert.match(profile,/onCloseRequested:\s*streakDrawer\.close\(\)/);
 assert.match(profile,/streakDrawer\.close\(\)/);
});

test('every Streak label is translated by stable translation ID',()=>{
 assert.equal(qmlIds.length,keys.length,'all Streak labels must be used through qsTrId');
 for(const key of keys)assert.ok(qmlIds.includes('genycaster.streak.'+key),key);
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
