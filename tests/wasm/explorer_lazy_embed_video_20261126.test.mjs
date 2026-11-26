import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
test('quoted cast and image cards do not construct a hidden video player',()=>{
    const card=read('ui/components/media/EmbedCard.qml');
    const preview=read('ui/components/media/EmbedPreview.qml');
    assert.match(preview,/id: castReferenceComponent\s+EmbedCard \{/);
    assert.doesNotMatch(card,/\b(?:GenyVideoPlayer|MediaPlayer|VideoOutput)\s*\{/);
    assert.match(card,/id: embeddedVideoLoader[\s\S]*?active: root.isExternal && root.isVideo[\s\S]*?sourceComponent: VideoPreview \{/);
    assert.match(card,/sourceComponent: VideoPreview \{\s*embed: root.embed\s*compact: true\s*onOpenRequested: url => root.openRequested\(url\)/);
});
test('shared feed player is created only after explicit Play and unloaded on source reuse',()=>{
    const video=read('ui/components/media/VideoPreview.qml');
    assert.match(video,/function requestPlayback\(\) \{[\s\S]*?root.playbackRequested = true/);
    assert.match(video,/onClicked: root.requestPlayback\(\)/);
    assert.match(video,/id: playerLoader[\s\S]*?active: root.playbackRequested[\s\S]*?sourceComponent: Component \{\s*GenyVideoPlayer \{/);
    assert.match(video,/onTargetUrlChanged: \{\s*root.playbackRequested = false/);
    assert.match(video,/onAdaptiveSourceChanged: \{\s*root.playbackRequested = false/);
    assert.match(video,/primeFrame: false\s*eager: false/);
});
