import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Muted users are account-scoped, fetched with authorized Qt client and cleared on account switch',()=>{
 const cpp=read('ui/app/openproof_connections_controller.cpp');
 const header=read('ui/app/openproof_connections_controller.hpp');
 assert.match(header,/Q_PROPERTY\(QStringList mutedProfileFids READ mutedProfileFids/);
 assert.match(header,/Q_INVOKABLE void setProfileMuted\(qulonglong targetFid, bool muted\)/);
 assert.match(cpp,/url\.setPath\(QStringLiteral\("\/api\/v1\/cloud\/muted_profiles"\)\)/);
 assert.match(cpp,/authorizeCloudRequest\(request\)/);
 assert.match(cpp,/const auto generation=m_grantGeneration/);
 assert.match(cpp,/m_mutedProfileFids\.clear\(\)/);
 assert.match(cpp,/m_mutedProfilesLoaded=false/);
 assert.match(cpp,/m_mutedProfilesRevision=revision/);
 assert.match(cpp,/fetchMutedProfiles\(\); \/\/ Refresh the canonical revision/);
});

test('Muted profiles never issue Farcaster Follow/Block messages',()=>{
 const app=read('ui/components/AppShell.qml');
 const ui=read('ui/components/ProfileSurface.qml');
 assert.match(ui,/objectName: "profileOptionsMenu"/);
 assert.match(ui,/signal muteRequested\(double targetFid, bool muted\)/);
 assert.match(ui,/root\.muteRequested\(Number\(root\.activeDetail\.fid\)/);
 assert.match(ui,/root\.muteSettingsReady && !root\.muteWriteBusy/);
 assert.match(app,/onMuteRequested: \(fid,muted\) =>\s*openProofConnections\.setProfileMuted/);
 assert.doesNotMatch(app,/onMuteRequested:[^\n]*setFollowing/);
});

test('Muted users are removed from main feed and notifications before aggregation',()=>{
 const app=read('ui/components/AppShell.qml');
 const notifications=read('ui/components/NotificationSurface.qml');
 assert.match(app,/function withoutMutedActors\(items\)/);
 assert.match(app,/rows: root\.withoutMutedActors\(/);
 assert.match(app,/mutedProfileFids: root\.mutedActorIds/);
 assert.match(notifications,/readonly property var displayRows: root\.groupNotificationRows\(root\.withoutMutedActors\(root\.rows\)\)/);
 assert.match(notifications,/copy\.aggregateCount=filtered\.length/);
 assert.match(notifications,/root\.mutedProfileFids\.indexOf/);
});

test('All twelve translations include Mute and Unmute affordances',()=>{
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
   const source=read('ui/i18n/qml_'+locale+'.ts');
   for(const id of ['genycaster.profile.mute','genycaster.profile.unmute']){
     assert.match(source,new RegExp('<message id="'+id.replaceAll('.','\\.')+'">'));
   }
 }
});
