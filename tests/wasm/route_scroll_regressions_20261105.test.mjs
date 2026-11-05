import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('profile launched from Notifications returns to its actual shell origin',()=>{
    const shell=read('ui/components/AppShell.qml');
    assert.match(shell,/function rememberShellRouteForReturn\(\)/);
    assert.match(shell,/function returnToPendingShellRoute\(\)/);
    assert.match(shell,/function returnFromProfileSurface\(\)\s*\{\s*presentationBridge\.returnFromDetail\(\)\s*root\.returnToPendingShellRoute\(\)/);
    const notification=shell.slice(shell.indexOf('NotificationSurface {'),shell.indexOf('NotificationSurface {')+3200);
    assert.match(notification,/onProfileRequested: fid => \{\s*root\.rememberShellRouteForReturn\(\)\s*presentationBridge\.clearDetailHistory\(\)/);
    assert.match(notification,/\+\+root\.profileTopRevision/);
});

test('navigating into a profile resets its scroll without losing regular timeline continuity',()=>{
    const shell=read('ui/components/AppShell.qml');
    const profile=read('ui/components/ProfileSurface.qml');
    assert.match(shell,/navigationRevision: root\.profileTopRevision/);
    assert.match(profile,/onNavigationRevisionChanged:\s*\{\s*if \(root\.visible\) root\.scrollToBeginning\(\)/);
    assert.match(profile,/onVisibleChanged:[\s\S]*root\.scrollToBeginning\(\)/);
    assert.match(profile,/text: qsTrId\("genycaster\.setup\.back"\)/);
    assert.doesNotMatch(profile,/text: qsTrId\("genycaster\.profile\.back_back_to_feed"\)/);
});

test('Explorer desktop stories scroll with the cast list, same as mobile',()=>{
    const feed=read('ui/components/FeedSurface.qml');
    assert.match(feed,/headerPositioning: ListView\.InlineHeader/);
    assert.doesNotMatch(feed,/headerPositioning:\s*root\.compact[\s\S]*?ListView\.OverlayHeader/);
    assert.match(feed,/property Component headerContent: null/);
    assert.match(feed,/onScrollRequestRevisionChanged: primaryScrollRequestTimer\.restart\(\)/);
});
