import test from 'node:test';
import assert from 'node:assert/strict';
import fs, { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = relative => fs.readFileSync(path.join(root, relative), 'utf8');

function qmlCanonicalizer(relative) {
    const source = read(relative);
    const start = source.indexOf('    function canonicalUrl(value) {');
    assert.ok(start >= 0, relative);
    const end = source.indexOf('\n    function ', start + 15);
    assert.ok(end > start, relative);
    return vm.runInNewContext('(' + source.slice(start, end).trim() + ')');
}

test('duplicate link cards ignore referral/tracking variants, but not meaningful paths or query IDs', () => {
    for (const file of ['ui/components/CastEmbedsPanel.qml', 'ui/components/RichCastRenderer.qml']) {
        const canonical = qmlCanonicalizer(file);
        assert.equal(canonical('https://gm.lens.xyz?sharedBy=doporg'),
                     canonical('https://gm.lens.xyz/?sharedBy=someone&utm_source=feed#view'));
        assert.equal(canonical('http://EXAMPLE.COM/post/?utm_medium=social&id=7'),
                     canonical('https://example.com/post?id=7'));
        assert.notEqual(canonical('https://example.com/post?id=7'),
                        canonical('https://example.com/post?id=8'));
        assert.notEqual(canonical('https://example.com/post?id=7'),
                        canonical('https://example.com/other?id=7'));
    }
    const panel = read('ui/components/CastEmbedsPanel.qml');
    assert.match(panel, /const key = root\.embedKey\(value\)/);
    assert.match(panel, /if \(key\.length > 0 && seen\[key\]\)/);
});

test('following stories use verified cast timestamps to sort follows while retaining older accounts', () => {
    const bridge = read('src/presentation/qt/presentation_bridge.cpp');
    const h = read('src/presentation/qt/presentation_bridge.hpp');
    const runtime = readGenycasterMain();
    const shell = read('ui/components/AppShell.qml');
    const strip = read('ui/components/StoryStrip.qml');
    assert.match(h, /followingStoryActivitiesRequested\(qulonglong viewerFid\)/);
    assert.match(bridge, /m_followingStoryActivity\.constFind\(fid\)/);
    assert.match(bridge, /latestCastId/);
    assert.match(bridge, /std::ranges::sort\(following,/);
    assert.match(bridge, /following\.prepend\(std::move\(own\)\)/);
    assert.match(bridge, /createdAtMs/);
    assert.match(runtime, /followingStoryActivitiesRequested/);
    assert.match(runtime, /FeedQuery::Kind::Following/);
    assert.match(runtime, /keepFollowingActivityRequest/);
    assert.match(shell, /recentProfiles: presentationBridge\.followingStoryProfiles/);
    assert.match(shell, /presentationBridge\.openCast\(castId\)/);
    assert.doesNotMatch(shell, /onRecentProfileRequested: profile => \{[\s\S]{0,300}openSuggestedProfile/);
    assert.match(strip, /Number\(modelData\.createdAtMs \|\| 0\) > 0/);
    assert.match(strip, /Date\.now\(\) - 86400000/);
});

test('following story results are isolated by viewer and never join unrelated public authors', () => {
    const cpp = read('src/presentation/qt/presentation_bridge.cpp');
    const start = cpp.indexOf('QVariantList PresentationBridge::followingStoryProfiles() const');
    const end = cpp.indexOf('bool PresentationBridge::explorerArtBusy()', start);
    const getter = cpp.slice(start, end);
    assert.match(getter, /m_accountFid != m_followingStoriesViewerFid/);
    assert.match(getter, /for \(const auto& candidate : m_followingStoryProfiles\)/);
    assert.match(getter, /const auto activity = m_followingStoryActivity\.constFind\(fid\)/);
    assert.match(getter, /if \(activity != m_followingStoryActivity\.cend\(\)\)/);
    assert.match(getter, /seen\.insert\(m_accountFid\)/);
    assert.match(cpp, /m_followingStoryActivity\.clear\(\)/);
});
