import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const profile=read('ui/components/ProfileSurface.qml');
const combo=read('ui/components/GenyComboBox.qml');
const backend=read('services/account-sync/cloud_state.mjs');
const store=read('services/account-sync/pg_store.mjs');
test('profile picker is compact, flat and has theme-accent selection',()=>{
 assert.match(combo,/height: 42/);
 assert.match(combo,/Math\.min\(contentItem\.implicitHeight \+ 2, 286\)/);
 assert.match(combo,/color: Theme\.accent/);
 assert.doesNotMatch(combo,/anchors\.margins: 2\s+fillColor: optionDelegate/);
 assert.match(profile,/objectName: "profileProtocolSave"[\s\S]{0,100}prominent: true/);
});
test('cover and avatar are composed as a single live preview',()=>{
 assert.match(profile,/objectName: "profileLivePreview"/);
 assert.match(profile,/source: linkPreviewResolver \? linkPreviewResolver\.imageSourceForUrl\(bannerProtocolEditor\.text\.trim\(\)\) : ""/);
 assert.match(profile,/sourceUrl: avatarProtocolEditor\.text\.trim\(\)/);
 assert.match(profile,/displayName: displayProtocolEditor\.text/);
 assert.match(profile,/root\.chooseProfileImage\(-1001\)/);
 assert.match(profile,/root\.chooseProfileImage\(-1002\)/);
});
test('trading plus at most three account-owned secondary categories are persisted',()=>{
 assert.match(profile,/"Sports", "Trading"/);
 assert.match(profile,/objectName: "profileSecondaryCategoryPicker"/);
 assert.match(profile,/maximumSelection: 3/);
 assert.match(profile,/secondary_categories: root\.profileSequence\(saved\.secondaryCategories, 3\)/);
 assert.match(profile,/secondaryCategories: root\.profileSequence\(server\.secondary_categories, 3\)/);
 assert.match(backend,/'Trading'/);
 assert.match(backend,/secondary\.length>3/);
 assert.match(store,/secondary_categories=EXCLUDED\.secondary_categories/);
 assert.match(read('services/account-sync/migrations/008_profile_secondary_categories.sql'),/jsonb_array_length\(secondary_categories\) <= 3/);
});
