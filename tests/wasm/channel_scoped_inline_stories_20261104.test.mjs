import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=name=>readFileSync(new URL('../../ui/components/'+name+'.qml',import.meta.url),'utf8');

test('channel has its own inline header and never inherits unrelated Following stories',()=>{
 const shell=read('AppShell');
 assert.match(shell,/id: channelFeedHeaderComponent/);
 assert.match(shell,/activeFeed: "channel"/);
 assert.match(shell,/id: explorerFeedExperienceHeaderComponent/);
 assert.match(shell,/presentationBridge\.activeFeed === "channel"\s*\? channelFeedHeaderComponent/);
 assert.match(shell,/presentationBridge\.activeFeed === "channel"\s*\? "for-you"/);
 assert.match(shell,/presentationBridge\.activeFeed === "channel"\s*\? "all"/);
 assert.match(shell,/model: channelHighlightRail\.channelItems/);
 assert.match(shell,/onTapped: presentationBridge\.openCast\(String\(modelData\.castId/);
});
test('scoped highlights require matching channel id and deduplicate authors',()=>{
 const shell=read('AppShell');
 const fn=shell.slice(shell.indexOf('function channelHighlights()'),shell.indexOf('id: channelFeedHeaderComponent'));
 assert.match(fn,/if \(presentationBridge\.activeFeed !== "channel"\) return \[\]/);
 assert.match(fn,/rowChannel !== channelId/);
 assert.match(fn,/parentUrl\.includes\("\/channel\/" \+ channelId\)/);
 assert.match(fn,/authors\.has\(fid\)/);
 assert.match(fn,/result\.length < 10/);
 assert.doesNotMatch(fn,/followingStoryProfiles/);
});
test('channel header scrolls with casts and returns to top when a channel is opened',()=>{
 const shell=read('AppShell');
 const feed=read('FeedSurface');
 assert.doesNotMatch(shell,/FeedHeader \{\s*visible: root\.shellRoute\.length/);
 assert.match(feed,/headerPositioning: root\.compact \|\| root\.activeFeed === "channel"/);
 assert.match(feed,/root\.scrollToBeginning\(\)/);
 assert.equal((shell.match(/presentationBridge\.openChannel\(channelId\)\s*\+\+root\.explorerTopRevision/g)||[]).length,2);
 assert.match(shell,/scrollRequestRevision: root\.primaryDestination === "explorer"/);
});
