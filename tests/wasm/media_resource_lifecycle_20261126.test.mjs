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
