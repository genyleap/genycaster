import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Channels is a real route, not a placeholder, with a working scoped feed',()=>{
  const shell=read('ui/components/shell/AppShell.qml');
  const cmake=read('ui/CMakeLists.txt');
  assert.match(shell,/id: channelsSurfaceLoader/);
  assert.match(shell,/active: root.shellRoute === "channels"/);
  assert.match(shell,/ChannelsSurface \{/);
  assert.match(shell,/channelSource: channelBrowser/);
  assert.match(shell,/presentationBridge.openChannel\(id\)/);
  assert.match(shell,/objectName: "channelReturnToDirectory"/);
  assert.match(shell,/objectName: "mobileExplorerChannelsEntry"/);
  assert.match(shell,/root.openShellRoute\("channels"\)/);
  assert.match(shell,/root.shellRoute !== "channels"/);
  assert.match(cmake,/components\/social\/ChannelsSurface\.qml/);
  assert.match(cmake,/components\/social\/ChannelRow\.qml/);
});

test('public discovery and joined composer selection have separate state owners',()=>{
  const main=read('ui/app/main.cpp');
  const cpp=read('ui/app/social/channels.cpp');
  const hpp=read('ui/app/social/channels.hpp');
  assert.match(main,/channelBrowserControllerOwner/);
  assert.match(main,/\{"channelDirectory", &channelPickerController\}/);
  assert.match(main,/\{"channelBrowser", &channelBrowser\}/);
  assert.match(main,/!isSmokeTest && !isVisualTest && !isRuntimeSmokeTest, true/);
  assert.match(hpp,/bool discoveryMode = false/);
  assert.match(hpp,/Q_INVOKABLE void refreshDiscovery\(\)/);
  assert.match(cpp,/if \(m_discoveryMode && m_trendingResults.isEmpty\(\) && !m_trendingBusy\)/);
  assert.match(cpp,/else if \(m_discoveryMode\) \{\s*insertRows\(m_trendingResults\)/);
  assert.match(cpp,/if \(m_viewerFid == 0\) return \{\}/);
  assert.match(cpp,/const auto it = m_members.constFind\(id\)/);
  assert.doesNotMatch(read('ui/components/social/ChannelsSurface.qml'),/chooseChannel\(/);
});

test('channel browser provides live search, clear, refresh, joined filter, and responsive cards',()=>{
  const page=read('ui/components/social/ChannelsSurface.qml');
  const row=read('ui/components/social/ChannelRow.qml');
  assert.match(page,/objectName: "channelsSearchField"/);
  assert.match(page,/root.channelSource.search\(text\)/);
  assert.match(page,/root.channelSource.refreshDiscovery\(\)/);
  assert.match(page,/objectName: "channelsJoinedTab"/);
  assert.match(page,/Boolean\(channel.isMember\)/);
  assert.match(page,/model: root.channelSource \? root.channelSource.channelModel : null/);
  assert.match(page,/reuseItems: true/);
  assert.match(page,/onOpenRequested: info => root.openChannel\(info\)/);
  assert.match(row,/forceAutomaticDownload: true/);
  assert.match(row,/Theme.rtl \? Theme.iconArrowLeft : Theme.iconArrowRight/);
  assert.match(row,/Keys.onReturnPressed: root.openRequested/);
  assert.match(page,/root.compact \? Theme.mobileGutter : Theme.contentGutter/);
});

test('public channel reads validate slug IDs and never assume membership',()=>{
  const page=read('ui/components/social/ChannelsSurface.qml');
  const start=page.indexOf('function openChannel(channel)');
  const end=page.indexOf('Component.onCompleted:',start);
  assert.ok(start>0 && end>start);
  const expression=page.slice(start,end).trim();
  const observed=[];
  const root={channelRequested:info=>observed.push(info.id)};
  const fn=vm.runInNewContext('('+expression+')',{root});
  fn({id:'art'});
  fn({id:'develop_ers-2'});
  fn({id:'https://not-a-channel.example'});
  fn({id:'../../admin'});
  fn({id:'bad name'});
  assert.deepEqual(observed,['art','develop_ers-2']);
});

test('channel copy is supplied in every supported language',()=>{
  const langs=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
               'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const lang of langs){
    const ts=read('ui/i18n/qml_'+lang+'.ts');
    for(const key of ['intro','discover','joined_empty','sign_in_hint','search_hint','read_note'])
      assert.equal(ts.split('id="genycaster.channels.'+key+'"').length-1,1,lang+':'+key);
  }
});
