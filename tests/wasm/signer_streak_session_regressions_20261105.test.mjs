import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Like/Recast cannot reopen signer approval while signer verification or a signed write is in flight',()=>{
    const reactions=read('ui/components/EngagementBar.qml');
    const shell=read('ui/components/AppShell.qml');
    assert.match(reactions,/presentationBridge\.signerPhase === "checking"/);
    assert.match(reactions,/presentationBridge\.writeBusy \|\| presentationBridge\.signerBusy/);
    assert.match(reactions,/enabled: !presentationBridge\.isAuthenticated \|\| !presentationBridge\.writeBusy/);
    assert.match(shell,/interactionsAvailable: presentationBridge\.isAuthenticated\s*&& presentationBridge\.writeReady/);
});

test('Streak snapshots are stored per FID only after successful authorized server responses',()=>{
    const controller=read('ui/mobile/openproof_connections_controller.cpp');
    assert.match(controller,/status==200 && doc\.isObject\(\)\s*&& doc\.object\(\)\.value\(QStringLiteral\("streak"\)\)\.isObject\(\)/);
    assert.match(controller,/setValue\(QStringLiteral\("streak\/snapshot\/%1"\)\.arg\(viewer\), m_streak\)/);
    assert.match(controller,/m_streak = m_settings && fid\s*\? m_settings->value\(QStringLiteral\("streak\/snapshot\/%1"\)\.arg\(fid\)\)\.toMap\(\)/);
    assert.match(controller,/m_cloudSessionVerified=true;[\s\S]*?fetchStreak\(\)/);
    assert.match(controller,/if \(!m_viewerFid \|\| !cloudAuthorizationReady\(\)/);
});

test('Streak endpoint remains authenticated and never fabricates a local streak write',()=>{
    const controller=read('ui/mobile/openproof_connections_controller.cpp');
    assert.match(controller,/authorizeCloudRequest\(request\)/);
    assert.match(controller,/m_streakError=QStringLiteral\("Streak is temporarily unavailable\."\)/);
    assert.doesNotMatch(controller,/m_streak\[.*\]=.*Math\.random/);
});
