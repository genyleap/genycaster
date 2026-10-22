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
test('channel picker relies exclusively on the C++ HyperSnap controller',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const backend=read('ui/mobile/channel_picker_controller.cpp');
 const header=read('ui/mobile/channel_picker_controller.hpp');
 const shell=read('ui/components/AppShell.qml');
 const main=read('ui/mobile/main.cpp');
 assert.match(composer,/id: channelPicker[\s\S]*?bottomSheet: true/);
 assert.match(composer,/model: channelDirectory.rows/);
 assert.match(composer,/channelDirectory.openForViewer\(/);
 assert.match(composer,/channelDirectory.search\(text\)/);
 assert.match(backend,/user\/memberships\/list/);
 assert.match(backend,/channel\/search/);
 assert.match(backend,/channel\/trending/);
 assert.match(main,/setContextProperty\(QStringLiteral\("channelDirectory"\)/);
 assert.match(header,/Q_INVOKABLE QVariantMap chooseChannel/);
 assert.match(composer,/root.selectedParentUrl = String\(selection.parentUrl \|\| ""\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.category"\)/);
 assert.match(shell,/publishPreparedCast\(text, mentionFids, mentionPositions, embeds, parentUrl\)/);
 assert.doesNotMatch(composer,/XMLHttpRequest|channelIconIndex|channelImageFor|searchFor\("dev"\)/);
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
test('channel avatars come from HyperSnap, and static image catalogs are removed',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 const backend=read('ui/mobile/channel_picker_controller.cpp');
 const packaging=read('tools/package_wasm_assets.py');
 const sw=read('ui/wasm/genycaster-sw.js');
 const cmake=read('ui/CMakeLists.txt');
 assert.match(backend,/firstString\(channel,\s*\{"image_url", "imageUrl", "icon_url", "iconUrl"\}\)/);
 assert.match(backend,/image.startsWith\(QStringLiteral\("https:\/\/"\)\)/);
 assert.match(composer,/sourceUrl: String\(channelRow.modelData.imageUrl \|\| ""\)/);
 assert.match(composer,/channelDirectory.chooseChannel\(/);
 assert.doesNotMatch(composer,/XMLHttpRequest|https:\/\/caster.genyleap.com\/channel-icons.json/);
 for(const file of [packaging,sw,cmake])
   assert.doesNotMatch(file,/channel-icons\.json/);
});
test('membership is verified in C++, Home always appears first, and non-members are disabled',()=>{
 const backend=read('ui/mobile/channel_picker_controller.cpp');
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(backend,/QVariantList list\{homeRow\(\)\}/);
 assert.match(backend,/m_membershipReady && m_members.contains\(id\)/);
 assert.match(backend,/row.insert\(QStringLiteral\("selectable"\), isJoined\)/);
 assert.match(backend,/if \(!m_membershipReady\) return \{\}/);
 assert.match(backend,/if \(it == m_members.cend\(\)\) return \{\}/);
 assert.match(backend,/m_members.clear\(\);\s*m_membershipReady = false; \/\/ fail closed/);
 assert.match(composer,/enabled: channelRow.selectable/);
 assert.match(composer,/qsTrId\("genycaster.composer.home_channel"\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.not_joined_channel"\)/);
});
test('search prioritizes exact or near matches, and remembers frequent choices per account',()=>{
 const backend=read('ui/mobile/channel_picker_controller.cpp');
 assert.match(backend,/if \(q == id \|\| q == name\) return 0/);
 assert.match(backend,/if \(id.startsWith\(q\) \|\| name.startsWith\(q\)\) return 1/);
 assert.match(backend,/nearDistance\(q, id\) <= 2/);
 assert.match(backend,/countA != countB\) return countA > countB/);
 assert.match(backend,/channels\/composeUsage\/%1/);
 assert.match(backend,/m_settings.setValue\(usageSettingsKey\(m_viewerFid\), counts\)/);
 assert.match(backend,/constexpr int searchTtlMs/);
 assert.match(backend,/m_searchCache.insert\(/);
});
test('channel results show live HyperSnap member counts, not fabricated follower values',()=>{
 const backend=read('ui/mobile/channel_picker_controller.cpp');
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(backend,/channel.value\(QStringLiteral\("member_count"\)\).toInteger\(-1\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.channel_member_count"\)/);
 assert.match(composer,/LocaleFormat.formatInteger\(channelRow.modelData.memberCount\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.channel_follower_count"\)/);
});

test('composer context uses outline pills before selection and fixed footer above the keyboard',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(composer,/objectName: "mobileComposerStickyFooter"/);
 assert.match(composer,/contentItem: ColumnLayout \{[\s\S]*?ScrollView \{/);
 assert.match(composer,/Layout\.fillHeight: true/);
 const outline=read("ui/components/DashedCapsuleOutline.qml");
 assert.match(outline,/strokeStyle: ShapePath.DashLine/);
 assert.match(outline,/dashPattern: \[4, 3\]/);
 assert.match(composer,/DashedCapsuleOutline \{/);
 assert.doesNotMatch(composer,/ctx\.roundedRect/);
 assert.match(composer,/Layout\.bottomMargin: root\.keyboardVisible \? Theme.space4 : Theme.space5/);
 assert.match(composer,/fillColor: root\.selectedParentUrl\.length > 0[\s\S]*?Theme\.primarySurface : "transparent"/);
 assert.match(composer,/objectName: "mobileCastContext"/);
 assert.match(composer,/objectName: "mobileComposerMediaBar"/);
 assert.match(composer,/Layout.preferredHeight: 36/);
 assert.match(composer,/Math.max\(118, Math.min\(188, composerScroll.height \* 0.36\)\)/);
 assert.match(composer,/const fitsFully = panelBottom - panelTop <= flick.height - margin \* 2/);
 assert.match(composer,/qsTrId\("genycaster\.composer\.category"\)/);
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
