import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const preview=readFileSync(new URL('../../ui/components/media/MediaPreview.qml',import.meta.url),'utf8');
test('still images and idle previews never construct a native video player',()=>{
 assert.match(preview,/id: videoLoader[\s\S]*?active: root.isVideo && root.hasSource && root.userPlayback/);
 assert.equal((preview.match(/GenyVideoPlayer \{/g)||[]).length,1);
 assert.match(preview,/sourceComponent: GenyVideoPlayer \{/);
 assert.match(preview,/onSourceChanged: \{\s*root.userPlayback = false/);
 assert.match(preview,/onIsVideoChanged: \{\s*if \(!isVideo\) \{\s*root.userPlayback = false/);
 assert.doesNotMatch(preview,/\bid: videoCore\b/);
});
test('video playback starts once after explicit interaction, then uses the existing player',()=>{
 assert.match(preview,/onLoaded: \{\s*if \(resourceScope.eligible && root.userPlayback && root.isVideo && root.hasSource && item\s*&& resourceScope.handoffPlayback\(\)\)\s*item.resumePlayback\(\)/);
 assert.match(preview,/if \(!videoLoader.item\) \{[\s\S]*?root.userPlayback = true\s*return/);
 assert.match(preview,/if \(videoLoader.item.playing\)\s*videoLoader.item.pausePlayback\(\)\s*else\s*videoLoader.item.resumePlayback\(\)/);
 assert.match(preview,/videoLoader.item !== null && videoLoader.item.playing/);
 assert.match(preview,/videoLoader.item !== null && videoLoader.item.failed/);
 assert.match(preview,/eager: false\s*primeFrame: false/);
});
