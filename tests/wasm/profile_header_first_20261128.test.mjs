import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../../'+p, import.meta.url),'utf8');
const profile = read('ui/components/profile/ProfileSurface.qml');
const shell = read('ui/components/shell/AppShell.qml');
const avatar = read('ui/components/content/Avatar.qml');
test('profile chrome incubates asynchronously while its posts remain header-gated',()=>{
    assert.match(shell,/id: profileSurfaceLoader[\s\S]*?asynchronous: true\s+diagnosticRoute: "profile"/);
    assert.match(profile,/id: profileCastLoader[\s\S]*?asynchronous: true/);
    assert.match(profile,/property bool profileRowsReleased: false/);
    assert.match(profile,/headerConstructed: false/);
    assert.match(profile,/Component.onCompleted: headerConstructed = true/);
    assert.match(profile,/root\.readList\.headerItem\.headerConstructed/);
    assert.match(profile,/id: profileHeaderDeadline\s+interval: 450/);
    assert.match(profile,/id: profileHeaderReleaseTimer\s+interval: 16/);
    assert.match(profile,/root\.profileRowsReleased = true\s+profileHeaderDeadline\.stop\(\)\s+stableModelSyncTimer\.restart\(\)/);
});
test('identity change resets admission before model clearing; same identity does not',()=>{
    const section=profile.slice(profile.indexOf('function reconcileProfileRowsIdentity()'),profile.indexOf('onProfileFidTextChanged:'));
    assert.ok(section.indexOf('key === root.rowsIdentityKey') < section.indexOf('root.beginProfileHeaderPriority()'));
    assert.ok(section.indexOf('root.beginProfileHeaderPriority()') < section.indexOf('profileRowsModel.clear()'));
    const sync=profile.slice(profile.indexOf('function requestStableModelSync()'),profile.indexOf('function isNearEnd()'));
    assert.match(sync,/root\.reconcileProfileRowsIdentity\(\)\s+if \(!root\.profileRowsReleased\)/);
    assert.match(profile,/root\.profileHeaderDeadlineExpired\s*\|\|/);
    assert.match(profile,/!bannerImage\.ready && !bannerImage\.loadFailed && !bannerImage\.policyBlocked/);
    assert.match(avatar,/!avatarImage\.loadFailed && !avatarImage\.policyBlocked/);
});
