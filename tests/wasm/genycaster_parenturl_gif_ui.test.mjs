import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = file => readFileSync(new URL('../../'+file,import.meta.url),'utf8');

test('Home has GENY promo carousel and Wallet instead of Trends',()=>{
 const home=read('ui/components/HomeDashboard.qml');
 for(const key of ['explorer','search','apps','wallet'])
   assert.match(home,new RegExp('destination: "'+key+'"'));
 assert.doesNotMatch(home,/destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(home,/component GenyTokenBanner: Superellipse/);
 assert.match(home,/geny_token_slide_two/);
 assert.match(home,/enabled: !shortcut.locked/);
});
test('reply rail and count use aligned, solid geometry and incoming highlight',()=>{
 const detail=read('ui/components/DetailSurface.qml');
 const reply=read('ui/components/ThreadReplyItem.qml');
 const rail=read('ui/components/GenyThreadRail.qml');
 assert.match(detail,/parentAuthoredByMe: Number\(root.detail.authorFid/);
 assert.match(reply,/readonly property bool replyToMyCast:/);
 assert.match(reply,/root.replyToMyCast \? Theme.primarySurface/);
 assert.match(reply,/spacing: Theme.space1/);
 assert.match(rail,/implicitWidth: compact \? 17 : 22/);
 assert.match(rail,/anchors.horizontalCenter: parent.horizontalCenter/);
 assert.match(detail,/Math.max\(26, replyCountLabel.implicitWidth \+ Theme.space3\)/);
});
test('channel/ticker context propagates to protocol without treating plain symbol as a contract',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const shell=read('ui/components/AppShell.qml');
 const bridge=read('src/presentation/qt/presentation_bridge.cpp');
 const runtime=read('ui/mobile/runtime_ui_bindings.cpp');
 assert.match(composer,/objectName: "mobileCastContext"/);
 assert.match(composer,/function validContextInput\(\)/);
 assert.match(composer,/\^0x\[0-9a-fA-F\]\{40\}\$/);
 assert.match(composer,/signal publishRequested\(string text, var mentionFids, var mentionPositions, var embeds, string parentUrl\)/);
 assert.match(shell,/publishPreparedCast\(text, mentionFids, mentionPositions, embeds, parentUrl\)/);
 assert.match(bridge,/emit publishCastRequested\(text, mentionFids, mentionPositions, embeds, target\)/);
 assert.match(runtime,/makeCastPayload\([\s\S]*?std::nullopt, parentUrl\)/);
});
test('image picker and upload pipeline accept GIF without converting to PNG',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const upload=read('ui/wasm/browser_media_upload.mjs');
 const qt=read('src/presentation/qt/media_upload_controller.cpp');
 const transport=read('src/transport/qt_network/media_upload.cpp');
 assert.match(composer,/text: "GIF"[\s\S]*?root.openMediaPicker\(\)/);
 assert.match(composer,/\*\.gif/);
 assert.match(upload,/\['gif','image\/gif'\]/);
 assert.match(qt,/SupportedMedia\{u"gif", u"image\/gif", u"image"\}/);
 assert.match(transport,/QStringLiteral\("image\/gif"\)/);
});
