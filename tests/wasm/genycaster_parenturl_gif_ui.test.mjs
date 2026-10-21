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
test('channel selection uses a bottom drawer and real Hypersnap search results',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const codec=read('src/providers/hypersnap/codec.cpp');
 const runtime=read('ui/mobile/runtime_ui_bindings.cpp');
 const shell=read('ui/components/AppShell.qml');
 assert.match(composer,/id: channelPicker[\s\S]*?bottomSheet: true/);
 assert.match(composer,/objectName: "composerChannelResults"/);
 assert.match(composer,/presentationBridge.requestSearchSuggestions\(query\)/);
 assert.match(composer,/channelRow.channelParentUrl/);
 assert.match(codec,/suggestion.parentUrl = optionalString\(channel, "parent_url"\)/);
 assert.match(runtime, /QStringLiteral\("parentUrl"\), channel.parentUrl/);
 assert.match(composer,/qsTrId\("genycaster.composer.category"\)/);
 assert.doesNotMatch(composer,/contextPicker|tokenNetwork|qsTrId\("genycaster.composer.ticker"\)/);
 assert.match(composer,/signal publishRequested\(string text, var mentionFids, var mentionPositions, var embeds, string parentUrl\)/);
 assert.match(shell,/publishPreparedCast\(text, mentionFids, mentionPositions, embeds, parentUrl\)/);
});
test('mobile composer uses an eagerly created editable TextArea for iOS gesture focus',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const shell=read('ui/components/AppShell.qml');
 assert.match(composer,/id: mobilePlainEditor[\s\S]*?anchors.fill: parent/);
 assert.match(composer,/function focusVisualEditorNow\(\)[\s\S]*?mobilePlainEditor.forceActiveFocus\(\)/);
 assert.match(composer,/richComposer.setBlockText\(0, mobilePlainEditor.text\)/);
 assert.match(shell,/composerDrawer.open\(\)\s*composerDrawer.focusVisualEditorNow\(\)/);
});
test('reply counter has safe trailing spacing and scroll jump stays near bottom edge',()=>{
 const detail=read('ui/components/DetailSurface.qml');
 const profile=read('ui/components/ProfileSurface.qml');
 assert.match(detail,/id: repliesContainer[\s\S]*?Item \{ Layout.preferredHeight: Theme.space4 \}/);
 assert.match(detail,/anchors.bottomMargin: root.compact \? Theme.space2 \+ 12 : Theme.space4/);
 assert.match(profile,/anchors.bottomMargin: root.compact \? Theme.space2 \+ 12 : Theme.space4/);
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
