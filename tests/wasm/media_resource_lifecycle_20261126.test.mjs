import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path, import.meta.url),'utf8');
const media = name => read('ui/components/media/'+name+'.qml');

test('playback loaders require a foreground viewport lease', () => {
    const scope = media('MediaResourceScope');
    assert.match(scope,/Qt\.ApplicationSuspended/);
    assert.match(scope,/mapToItem\(parent, 0, 0\)/);
    for (const name of ['VideoPreview','MediaPreview','AudioPreview']) {
        const source = media(name);
        assert.match(source,/MediaResourceScope\s*\{/);
        assert.match(source,/active: [^\n]*resourceScope\.eligible/);
        assert.match(source,/onLoaded:\s*\{[\s\S]*?resourceScope\.eligible/);
    }
});
test('fullscreen reuses a single video output', () => {
    const player = media('GenyVideoPlayer');
    assert.equal((player.match(/\bVideoOutput\s*\{/g)||[]).length,1);
    assert.match(player,/stage\.parent = fullscreenHost/);
    assert.match(player,/function suspendFromPlaybackCoordinator\(\) \{[\s\S]*?root\.resetPlayback\(\)[\s\S]*?root\.resourceRevoked\(\)/);
});
test('link preview branches are mutually exclusive', () => {
    const link = media('LinkPreview');
    assert.match(link,/id: miniAppPreviewLoader\s+active: root\.layoutMiniApp/);
    assert.match(link,/id: webPreviewLoader\s+active: !root\.layoutMiniApp/);
});
test('browser media closes on lifecycle transitions', () => {
    const browser = read('ui/wasm/browser-hls-pre.js');
    assert.match(browser,/addEventListener\("pagehide", close\)/);
    assert.match(browser,/if \(document\.hidden\) close\(\)/);
});

test('pending playback is reserved before asynchronous player creation', () => {
    const scope = media('MediaResourceScope');
    assert.match(scope,/session\.claimPrimary\(root\)/);
    assert.match(scope,/session\.isPrimary\(root\)/);
    for (const name of ['VideoPreview','MediaPreview']) {
        const preview = media(name);
        assert.match(preview,/resourceScope\.reservePlayback\(\)/);
        assert.match(preview,/resourceScope\.handoffPlayback\(\)/);
    }
});
test('abandoned image request cancels its transport without holding the response mutex', () => {
    const image = read('ui/app/media/remote_image.cpp');
    assert.match(image,/m_control->cancel\(\)/);
    assert.match(image,/operation = std::move\(activeOperation\);\s*\}\s*operation\.cancel\(\)/);
    assert.match(image,/control->setOperation\(std::move\(operation\)\)/);
});
