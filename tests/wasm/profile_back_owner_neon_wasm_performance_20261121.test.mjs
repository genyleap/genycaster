import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const bridge = read('src/presentation/qt/presentation_bridge.cpp');
const header = read('src/presentation/qt/presentation_bridge.hpp');
const profile = read('ui/components/profile/ProfileSurface.qml');
const source = read('ui/components/visual/NeonReflectionSource.qml');
const reflection = read('ui/components/visual/NeonReflection.qml');
const dock = read('ui/components/navigation/BottomNavigation.qml');
const appHeader = read('ui/components/shell/AppHeader.qml');
const shell = read('ui/components/shell/AppShell.qml');

const section = (src, before, after) =>
    src.split(before)[1]?.split(after)[0] ?? '';
const activeDetailBody = section(profile,
    '    function resolveActiveDetail() {',
    '    function synchronizeActiveDetail()').trim().replace(/\}\s*$/, '');
const resolveProfile = new Function('root', 'presentationBridge', activeDetailBody);

test('Back preserves authenticated ownership in the native breadcrumb, not just FID and tab', () => {
    const remember = section(bridge,
        'void PresentationBridge::rememberDetailLocation()',
        'void PresentationBridge::clearDetailHistory()');
    const back = section(bridge,
        'void PresentationBridge::returnFromDetail()',
        'void PresentationBridge::loadMore()');
    assert.match(header, /bool ownProfile\{false\};/);
    assert.match(remember, /location\.ownProfile = ownProfileNavigation\(\);/);
    assert.match(back, /previous\.ownProfile && m_isAuthenticated/);
    assert.match(back, /previous\.profileFid == m_accountFid && m_accountFid != 0/);
    assert.match(back, /openMyProfile\(\);[\s\S]*?else if \(previous\.profileFid != 0\)[\s\S]*?openProfileFid\(previous\.profileFid\)/);
    assert.match(back, /m_profileFeed = previous\.profileFeed/);
    assert.match(back, /m_restoringDetailHistory = true/);
    assert.match(back, /m_restoringDetailHistory = false/);
});

test('own profile entry pins authenticated protocol before any public request or state change', () => {
    const own = section(bridge,
        'void PresentationBridge::openMyProfile()',
        'void PresentationBridge::openProfileUsername(');
    assert.match(own, /m_profileIsOwnAccount = true/);
    assert.match(own, /m_profileFid = m_accountFid/);
    assert.match(own, /m_currentProfileUsername\.clear\(\)/);
    assert.match(own, /m_detail\.clear\(\);\s*m_rows\.clear\(\);\s*m_next\.clear\(\);/);
    assert.match(own, /if \(!\(m_activeView == QStringLiteral\("profile"\)[\s\S]*?m_profileFid == m_accountFid\)\)\s*rememberDetailLocation\(\)/);
    const affinity = own.indexOf('emit ownProfileReadAffinityRequested(true, nativeCustodyAccount())');
    const request = own.indexOf('emit profileRequested(m_accountFid, false)');
    const state = own.indexOf('beginIntent(');
    assert.ok(affinity >= 0 && affinity < state && state < request);
    assert.doesNotMatch(own, /openProfileFid\(m_accountFid\)/);
    assert.match(header, /Q_PROPERTY\(QString activeProfileFidText READ activeProfileFidText NOTIFY stateChanged\)/);
});

test('out-of-order replies cannot retarget requested profile after Back', () => {
    const apply = section(bridge,
        'void PresentationBridge::applyDetail(',
        'void PresentationBridge::applySearchFollowing(');
    const guard = apply.indexOf('receivedFid != m_profileFid');
    const mutate = apply.indexOf('m_activeView = std::move(view)');
    assert.ok(guard >= 0 && mutate > guard);
    assert.match(apply, /view == QStringLiteral\("profile"\) && m_activeView == QStringLiteral\("profile"\)/);
});

test('reused QML ProfileSurface never reveals previous identity or cross-protocol metadata during loading', () => {
    const root = {
        ownProfileNavigation: false,
        accountFidText: '42',
        accountFid: 42,
        accountNetwork: 'farcaster',
        activeReadNetwork: 'farcaster',
        profileNetwork: 'farcaster',
        detail: {}
    };
    const pending = {activeProfileFidText: '73'};
    assert.deepEqual(resolveProfile(root, pending), {});
    const restored = {activeProfileFidText: '42'};
    // Returning to a profile without a current bridge snapshot is an
    // intentional empty/loading header, not the stale previous identity.
    assert.deepEqual(resolveProfile(root, restored), {});
    root.detail = {fid: 42, fidString: '42', displayName: 'Restored profile'};
    assert.equal(resolveProfile(root, restored).displayName, 'Restored profile');

    root.profileNetwork = 'hypersnap';
    root.detail = {};
    assert.deepEqual(resolveProfile(root, restored), {});
    root.detail = {fid: 81, fidString: '81', displayName: 'Late reply'};
    assert.deepEqual(resolveProfile(root, {activeProfileFidText: '42'}), {});

    root.ownProfileNavigation = true;
    root.accountNetwork = 'farcaster';
    root.activeReadNetwork = 'hypersnap';
    const account = {fid: 42, fidString: '42', displayName: 'Authenticated'};
    const own = resolveProfile(root, {
        activeProfileFidText: '42', ownProfileReadReady: true,
        accountProfile: account
    });
    assert.equal(own.displayName, 'Authenticated');
    assert.notEqual(own.displayName, 'Late reply');
    assert.doesNotMatch(profile, /retainedDetailNetwork === root\.profileNetwork/);
    assert.match(profile, /presentationBridge\.activeProfileFidText/);
    const target = profile.split('function resolveProfileFidText() {')[1]
        .split('readonly property string activeReadNetwork:')[0];
    assert.doesNotMatch(target, /root\.activeDetail/);
});

test('WASM reflections reuse one capture at reduced area and cadence; dock glass uses fewer blur taps', () => {
    assert.match(source, /interval: 240/);
    assert.match(source, /root\.browserRenderer\s*\? Math\.min\(0\.0625, 256 \//);
    assert.match(source, /: Math\.min\(0\.125, 512 \//);
    assert.match(source, /live: root\.capturing && !root\.browserRenderer/);
    assert.match(source, /sourceIsSafe: \{/);
    assert.match(reflection, /Math\.max\(0, Math\.min\(0\.42, strength\)\) \* Theme\.reflectionGain/);
    assert.match(appHeader, /strength: Theme\.reflectionStrength \* \(root\.compact \? 1\.50 : 1\.35\)/);
    assert.match(dock, /blurMax: Qt\.platform\.os === "wasm" \? 16 : 32/);
    assert.match(dock, /blurMultiplier: Qt\.platform\.os === "wasm" \? 1\.65 : 1\.3/);
    assert.match(dock, /reflectionSource: root\.reflectionSource/);
    assert.match(shell, /id: workspaceReflection[\s\S]*?sourceItem: readingBackdrop/);
    assert.equal((shell.match(/NeonReflectionSource\s*\{/g) || []).length, 1);
});
