import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const composer=read('ui/components/ComposerDrawer.qml');
const profile=read('ui/components/ProfileSurface.qml');
const store=read('services/account-sync/pg_store.mjs');
const validator=read('services/account-sync/cloud_state.mjs');
test('desktop has separately actionable Farcaster channel and cast topic selectors',()=>{
 assert.match(composer,/objectName: "desktopCastTopics"[\s\S]*?objectName: "desktopCastChannelTrigger"[\s\S]*?onClicked: channelPicker\.open\(\)[\s\S]*?onClicked: topicPicker\.open\(\)/);
 assert.match(composer,/visible: !root\.compact && root\.showTaxonomyControls/);
 assert.match(composer,/root\.selectedParentUrl\.length > 0/);
 assert.match(composer,/case "trading": return qsTrId\("genycaster\.topic\.trading"\)/);
});
test('secondary category picker never presents a blank trigger',()=>{
 assert.match(profile,/property var secondarySelection: \[\]/);
 assert.match(profile,/profileExtrasEditor\.secondarySelection = root\.profileSequence\(root\.profileExtras\.secondaryCategories, 3\)/);
 assert.match(profile,/objectName: "profileSecondaryCategoryPicker"[\s\S]{0,650}placeholderText: qsTrId\("genycaster\.profile\.add_secondary"\)/);
});
test('multiple roles are additive owner scoped with migration and legacy compatibility',()=>{
 assert.match(profile,/property var roleSelection: \[\]/);
 assert.match(profile,/maximumSelection: 5/);
 assert.match(profile,/profileExtrasEditor\.roleSelection = nextValues/);
 const multi=read('ui/components/GenyMultiSelect.qml');
 assert.match(multi,/next\.push\(canonical\)/);
 assert.match(multi,/next\.splice\(existing, 1\)/);
 assert.match(profile,/roles: root\.profileSequence\(saved\.roles, 5\)/);
 assert.match(store,/multipleRolesReady=versions\.has\(9\)/);
 assert.match(store,/roles=EXCLUDED\.roles/);
 assert.match(validator,/new Set\(roles\)\.size!==roles\.length/);
 const sql=read('services/account-sync/migrations/009_profile_roles_cast_trading.sql');
 assert.match(sql,/ADD COLUMN IF NOT EXISTS roles jsonb/);
 assert.match(sql,/jsonb_array_length\(roles\) <= 5/);
 assert.match(sql,/INSERT INTO topics\(slug,sort_order,is_active\) VALUES \('trading',105,true\)/);
});
test('Trading is localized and discoverable through the topic catalog presentation',()=>{
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const content=read('ui/i18n/qml_'+locale+'.ts');
  for(const id of ['genycaster.topic.trading','genycaster.profile.add_role','genycaster.profile.roles_hint'])
   assert.ok(content.includes('id="'+id+'"'),locale+': '+id);
 }
 for(const path of ['ui/components/ComposerDrawer.qml','ui/components/TopicFilterPicker.qml','ui/components/AppShell.qml','ui/components/ProfileSurface.qml','ui/components/TopicBrowseSurface.qml'])
  assert.match(read(path),/genycaster\.topic\.trading/);
});
