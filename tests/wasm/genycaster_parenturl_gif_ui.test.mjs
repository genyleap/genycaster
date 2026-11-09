import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = file => readFileSync(new URL('../../'+file,import.meta.url),'utf8');

test('Home has GENY promo carousel and Wallet instead of Trends',()=>{
 const home=read('ui/components/home/HomeDashboard.qml');
 for(const key of ['explorer','search','apps','wallet'])
   assert.match(home,new RegExp('destination: "'+key+'"'));
 assert.doesNotMatch(home,/destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(home,/component GenyTokenBanner: Superellipse/);
 assert.match(home,/geny_token_slide_two/);
 assert.match(home,/enabled: !shortcut.locked/);
});
test('reply rail and count use aligned, solid geometry and incoming highlight',()=>{
 const detail=read('ui/components/feed/DetailSurface.qml');
 const reply=read('ui/components/feed/ThreadReplyItem.qml');
 const rail=read('ui/components/feed/GenyThreadRail.qml');
 assert.match(detail,/parentAuthoredByMe: Number\(root.detail.authorFid/);
 assert.match(reply,/readonly property bool replyToMyCast:/);
 assert.match(reply, /borderWidth: 0/);
 assert.match(reply, /borderColor: "transparent"/);
 assert.match(reply,/spacing: Theme.space1/);
 assert.match(rail,/implicitWidth: compact \? 17 : 22/);
 assert.match(rail,/anchors.horizontalCenter: parent.horizontalCenter/);
 assert.match(detail,/Math.max\(26, replyCountLabel.implicitWidth \+ Theme.space3\)/);
});
test('channel picker relies exclusively on the C++ HyperSnap controller',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 const backend=read('ui/app/social/channels.cpp');
 const header=read('ui/app/social/channels.hpp');
 const shell=read('ui/components/shell/AppShell.qml');
 const main=read('ui/app/main.cpp');
 assert.match(composer,/id: channelPicker[\s\S]*?objectName: "composerChannelDropdown"/);
 assert.match(composer,/readonly property Item triggerItem: root\.compact \? selectedChannelChip : desktopCastChannelTrigger/);
 assert.match(composer,/parent: Overlay\.overlay/);
 assert.match(composer,/model: channelDirectory.channelModel/);
 assert.match(header,/Q_PROPERTY\(QAbstractItemModel\* channelModel READ channelModel CONSTANT FINAL\)/);
 assert.match(composer,/channelDirectory.openForViewer\(/);
 assert.match(composer,/channelDirectory.search\(text\)/);
 assert.match(backend,/user\/memberships\/list/);
 assert.match(backend,/channel\/search/);
 assert.match(backend,/channel\/trending/);
 assert.match(main,/\{"channelDirectory", &channelPickerController\}/);
 assert.match(header,/Q_INVOKABLE QVariantMap chooseChannel/);
 assert.match(composer,/root.selectedParentUrl = String\(selection.parentUrl \|\| ""\)/);
 assert.match(composer,/qsTrId\("genycaster.topics.label"\)/);
 assert.match(shell,/publishPreparedCast\(text, mentionFids, mentionPositions, embeds, parentUrl\)/);
 assert.doesNotMatch(composer,/XMLHttpRequest|channelIconIndex|channelImageFor|searchFor\("dev"\)/);
});

test('channel avatars stay mounted while C++ metadata updates and initial records are batched',()=>{
 const backend=read('ui/app/social/channels.cpp');
 const header=read('ui/app/social/channels.hpp');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(header,/class ChannelListModel final : public QAbstractListModel/);
 assert.match(header,/Q_PROPERTY\(QAbstractItemModel\* channelModel READ channelModel CONSTANT FINAL\)/);
 assert.match(composer,/model: channelDirectory.channelModel/);
 assert.match(backend,/beginInsertRows\(/);
 assert.match(backend,/beginMoveRows\(/);
 assert.match(backend,/emit dataChanged\(/);
 assert.doesNotMatch(backend,/beginResetModel\(|endResetModel\(/);
 assert.match(backend,/m_initialPresentation.setInterval\(950\)/);
 assert.match(backend,/m_membershipBusy \|\| m_initialPresentationPending/);
 assert.match(backend,/m_initialPresentationPending = true/);
 assert.match(backend,/m_presentationOrder.value\(idA\) < m_presentationOrder.value\(idB\)/);
 assert.match(composer,/LocaleFormat.formatInteger\(channelRow.modelData.memberCount\)/);
 assert.doesNotMatch(composer,/\? "—"/);
});
test('mobile composer uses an eagerly created editable TextArea for iOS gesture focus',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 const shell=read('ui/components/shell/AppShell.qml');
 assert.match(composer,/id: mobilePlainEditor[\s\S]*?anchors.fill: parent/);
 assert.match(composer,/function focusVisualEditorNow\(\)[\s\S]*?mobilePlainEditor.forceActiveFocus\(\)/);
 assert.match(composer,/richComposer.setBlockText\(0, mobilePlainEditor.text\)/);
 assert.match(shell,/composerDrawer.open\(\)\s*composerDrawer.focusVisualEditorNow\(\)/);
});
test('reply counter has safe trailing spacing and scroll jump stays near bottom edge',()=>{
 const detail=read('ui/components/feed/DetailSurface.qml');
 const profile=read('ui/components/profile/ProfileSurface.qml');
 assert.match(detail,/id: repliesContainer[\s\S]*?Item \{ Layout.preferredHeight: Theme.space4 \}/);
 assert.match(detail,/anchors\.bottomMargin: Math\.max\(Theme\.space4, root\.bottomOverlayInset \+ Theme\.space3\)/);
 assert.match(profile,/anchors.bottomMargin: Math\.max\(Theme\.space4, root\.bottomOverlayInset/);
});
test('channel metadata is optional and bound to the current deployment origin, not a hardcoded read node',()=>{
 const cpp=read('ui/app/main.cpp');
 const controller=read('ui/app/social/channels.cpp');
 const view=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(cpp,/GENYCASTER_CHANNEL_DIRECTORY_BASE_URL/);
 assert.match(cpp,/channelDirectoryBaseUrl = mediaGatewayBaseUrlText/);
 assert.match(cpp,/channelDirectoryBaseUrl \+= QStringLiteral\("\/v1\/browser\/channels"\)/);
 assert.match(cpp,/GENYCASTER_CHANNEL_DIRECTORY_BASE_URL/);
 assert.match(controller,/requestDirectory\(QStringLiteral\("search-channels"\)/);
 assert.match(controller,/m_baseUrl = m_baseUrl\.trimmed\(\)/);
 assert.match(controller,/m_directoryBaseUrl = m_directoryBaseUrl\.trimmed\(\)/);
 assert.match(view,/channelDirectory.chooseChannel/);
 assert.doesNotMatch(view,/XMLHttpRequest|channel-icons\.json/);
});
test('ordinary labels and cast bodies expose selectable text with explicit copy',()=>{
 const label=read('ui/components/controls/BidiLabel.qml');
 const qml=read('ui/components/content/RichInlineText.qml');
 const cpp=read('src/presentation/qt/rich_inline_text_item.cpp');
 assert.match(label,/property bool selectableText: true/);
 assert.match(label,/Loader \{[\s\S]*?active: control.selectionActive/);
 assert.match(label,/Component\.onCompleted: selectAll\(\)/);
 assert.match(label,/presentationBridge.copyText\(/);
 assert.doesNotMatch(label,/^\s*textInteractionFlags\s*:/m,
   'Qt Quick Controls Label has no textInteractionFlags; invalid QML prevents app startup');
 assert.match(cpp,/QAbstractTextDocumentLayout::PaintContext/);
 assert.match(cpp,/RichInlineTextItem::selectWordAt/);
 assert.match(cpp,/RichInlineTextItem::mouseMoveEvent/);
 assert.match(cpp,/m_longPressSelected/);
 assert.match(cpp,/m_pressHoldTimer\.start\(\)/);
 assert.match(cpp,/void RichInlineTextItem::mouseUngrabEvent\(\)/);
 assert.match(qml,/onSelectionCompleted:/);
 assert.match(qml,/parent: Overlay.overlay/);
 assert.match(qml,/textLayout.selectionMenuPosition/);
 assert.doesNotMatch(qml,/id: selectionCopyAction/);
 assert.match(qml,/presentationBridge\.copyText\(textLayout\.selectedText\)/);
});
test('node switching preserves the source-provider URL and exposes no UI hard-coded node',()=>{
 const main=read('ui/app/main.cpp');
 const picker=read('ui/app/social/channels.cpp');
 const node=read('src/providers/hypersnap/node_selection_controller.cpp');
 assert.match(main,/farcaster::provider::hypersnap::config::Config\{\}\.baseUrl/);
 assert.match(main,/settings\.value\(\s*QStringLiteral\("network\/channelDirectoryBaseUrl"\)/);
 assert.match(picker,/m_baseUrl = m_baseUrl\.trimmed\(\)/);
 assert.match(picker,/m_directoryBaseUrl = m_directoryBaseUrl\.trimmed\(\)/);
 assert.doesNotMatch(node,/candidateEndpoints\.prepend\(QStringLiteral\("https:\/\/haatz\.quilibrium\.com"\)\)/);
 assert.doesNotMatch(picker,/m_baseUrl\s*=\s*QStringLiteral\("https:\/\/haatz\.quilibrium\.com"\)/);
});
test('channel avatars come from HyperSnap, and static image catalogs are removed',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 const backend=read('ui/app/social/channels.cpp');
 const packaging=read('ci/package_wasm_assets.py');
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
 const backend=read('ui/app/social/channels.cpp');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(backend,/QVariantList list\{homeRow\(\)\}/);
 assert.match(backend,/m_viewerFid > 0 && m_members.contains\(id\)/);
 assert.match(backend,/row.insert\(QStringLiteral\("selectable"\), isJoined[\s\S]*?validParentUrl/);
 assert.match(backend,/if \(m_viewerFid == 0\) return \{\}/);
 assert.match(backend,/if \(it == m_members.cend\(\)\) return \{\}/);
 assert.match(backend,/m_members.clear\(\);\s*m_membershipReady = false; \/\/ fail closed/);
 assert.match(composer,/enabled: channelRow.selectable/);
 assert.match(composer,/qsTrId\("genycaster.composer.home_channel"\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.not_joined_channel"\)/);
});
test('search prioritizes exact or near matches, and remembers frequent choices per account',()=>{
 const backend=read('ui/app/social/channels.cpp');
 assert.match(backend,/if \(q == id \|\| q == name\) return 0/);
 assert.match(backend,/if \(id.startsWith\(q\) \|\| name.startsWith\(q\)\) return 1/);
 assert.match(backend,/nearDistance\(q, id\) <= 2/);
 assert.match(backend,/countA != countB\) return countA > countB/);
 assert.match(backend,/channels\/composeUsage\/%1/);
 assert.match(backend,/m_settings.setValue\(usageSettingsKey\(m_viewerFid\), counts\)/);
 assert.match(backend,/constexpr int searchTtlMs/);
 assert.match(backend,/m_searchCache.insert\(/);
});
test('channel rows report only verified follower counts, not HyperSnap membership counts',()=>{
 const backend=read('ui/app/social/channels.cpp');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(backend,/channel.value\(QStringLiteral\("follower_count"\)\).toInteger\(-1\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.channel_follower_count"\)/);
 assert.match(composer,/LocaleFormat.formatInteger\(channelRow.modelData.followerCount\)/);
 assert.match(composer,/qsTrId\("genycaster.composer.channel_member_count"\)/);
 assert.match(composer,/LocaleFormat.formatInteger\(channelRow.modelData.memberCount\)/);
 assert.doesNotMatch(composer,/\? "—"/);
});

test('channel selection emits a deferred C++ confirmation instead of invalidating its own delegate',()=>{
 const backend=read('ui/app/social/channels.cpp');
 const header=read('ui/app/social/channels.hpp');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(header,/void channelChosen\(const QVariantMap& selection\)/);
 assert.match(backend,/QTimer::singleShot\(0, this, \[this, selected, generation\]/);
 assert.match(backend,/emit channelChosen\(selected\);\s*rebuildRows\(\)/);
 assert.match(composer,/function onChannelChosen\(selection\)/);
 assert.match(composer,/channelDirectory.chooseChannel\(String\(channelRow.modelData.id \|\| ""\)\)/);
 assert.doesNotMatch(composer,/const selection = channelDirectory.chooseChannel/);
});

test('composer editing shows compact clipboard actions beside the selection',()=>{
 const editor=read('ui/components/controls/BidiTextArea.qml');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(editor,/objectName: "editorSelectionActions"/);
 assert.match(editor,/root.mapToItem\(selectionBar.parent, start.x, start.y\)/);
 assert.match(editor,/onPressAndHold:/);
 assert.match(editor,/onSelectedTextChanged:/);
 assert.match(editor,/presentationBridge.copyText\(root.selectedText\)/);
 assert.match(editor,/focusPolicy: Qt.NoFocus/);
 assert.doesNotMatch(editor,/onActiveFocusChanged: if \(!activeFocus\) selectionBar.close\(\)/);
 assert.match(editor,/root.cut\(\)/);
 assert.match(editor,/root.selectAll\(\)/);
 assert.match(composer,/id: mobilePlainEditor/);
 assert.match(composer,/id: threadEditor/);
});

test('composer mobile selectors use solid matching controls and keyboard-safe footer',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(composer,/objectName: "mobileComposerStickyFooter"/);
 assert.match(composer,/contentItem: ColumnLayout \{[\s\S]*?ScrollView \{/);
 assert.match(composer,/Layout\.fillHeight: true/);
 assert.match(composer,/objectName: "mobileCastContext"/);
  assert.match(composer,/id: selectedChannelChip/);
 assert.doesNotMatch(composer,/DashedCapsuleOutline \{/);
 assert.match(composer,/fillColor: root.selectedParentUrl.length > 0[\s\S]*?Theme.primarySurface : Theme.surfaceMuted/);
 assert.doesNotMatch(composer,/ctx\.roundedRect/);
 assert.match(composer,/Layout\.bottomMargin: root\.keyboardVisible \? Theme.space2\s*: Math\.max\(Theme\.space3, root\.bottomSafeInset\)/);
 assert.match(composer,/fillColor: root\.selectedTopics\.length > 0[\s\S]*?Theme\.primarySurface : Theme.surfaceMuted/);
 assert.match(composer,/objectName: "mobileCastContext"/);
 assert.match(composer,/objectName: "mobileComposerMediaBar"/);
 assert.match(composer,/Layout.preferredHeight: Theme.touchTarget/);
 assert.match(composer,/Math.max\(118, Math.min\(188, composerScroll.height \* 0.36\)\)/);
 assert.match(composer,/const fitsFully = panelBottom - panelTop <= flick.height - margin \* 2/);
 assert.match(composer,/qsTrId\("genycaster\.topics\.label"\)/);
});
test('image picker and upload pipeline accept GIF without converting to PNG',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 const upload=read('ui/wasm/browser_media_upload.mjs');
 const qt=read('src/presentation/qt/media_upload_controller.cpp');
 const transport=read('src/transport/qt_network/media_upload.cpp');
 assert.match(composer,/text: "GIF"[\s\S]*?root.openMediaPicker\(\)/);
 assert.match(composer,/\*\.gif/);
 assert.match(upload,/\['gif','image\/gif'\]/);
 assert.match(qt,/SupportedMedia\{u"gif", u"image\/gif", u"image"\}/);
 assert.match(transport,/QStringLiteral\("image\/gif"\)/);
});
