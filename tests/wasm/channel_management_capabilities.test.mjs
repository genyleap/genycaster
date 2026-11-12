import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const cpp = source('ui/app/social/channel_administration.cpp');
const hpp = source('ui/app/social/channel_administration.hpp');
const shell = source('ui/components/shell/AppShell.qml');
const page = source('ui/components/social/ChannelsSurface.qml');
const manage = source('ui/components/social/ChannelManagementPanel.qml');
const browserCpp = source('ui/app/social/channels.cpp');

test('WASM build remains an explicit separate action; channels are native module resources',()=>{
  const cmake = source('ui/CMakeLists.txt');
  assert.match(cmake,/app\/social\/channel_administration\.cpp/);
  assert.match(cmake,/components\/social\/ChannelManagementPanel\.qml/);
  assert.match(shell,/adminSource: channelAdmin/);
  assert.match(shell,/readContract: providerProfiles\.activeReadContract/);
  assert.match(shell,/readEndpoint: providerProfiles\.activeReadEndpoint/);
  assert.match(page,/onAccountFidChanged: root\.configureProviders\(\)/);
  assert.match(page,/channelSource\.configureReadSource\(readContract, readEndpoint\)/);
});

test('management has Following / Membership / Moderate as separate scopes',()=>{
  for(const key of ['channelsDiscoverTab','channelsFollowingTab','channelsJoinedTab','channelsModerateTab'])
    assert.match(page,new RegExp('objectName: "'+key+'"'));
  assert.match(page,/root\.scope === "following"[\s\S]*?root\.adminSource\.following/);
  assert.match(page,/root\.scope === "joined" \|\| root\.scope === "moderate"/);
  assert.match(page,/objectName: "channelManagementPopup"/);
  for(const t of ['overview','members','moderate','invites'])
    assert.match(manage,new RegExp('key: "'+t+'"'));
  assert.match(manage,/root\.manager\.loadInvites\(\)/);
});

test('no clipped footer and public discovery promotes trending above already joined channels',()=>{
  assert.doesNotMatch(page,/genycaster\.channels\.read_note/);
  assert.match(browserCpp,/const bool featuredA = featuredIds\.contains/);
  assert.match(browserCpp,/if \(featuredA != featuredB\) return featuredA;/);
  assert.match(browserCpp,/m_discoveryMode && m_query\.isEmpty\(\)/);
});

test('HyperSnap 501 endpoints are not treated as valid management writes',()=>{
  assert.match(cpp,/HyperSnap: channel follow and invite REST writes return 501/);
  assert.match(cpp,/m_contract != QLatin1String\("neynar-v2"\)/);
  assert.match(cpp,/m_writeProviderId != QLatin1String\("neynar-managed"\)/);
  assert.match(cpp,/No approved Neynar managed signer for this account/);
  assert.doesNotMatch(cpp,/else if \(m_contract == QLatin1String\("hypersnap-v2"\)\)\s+submit/);
});

test('Neynar signer status is checked against the actual account and approved state',()=>{
  assert.match(cpp,/verifyManagedSigner\(m_generation\)/);
  assert.match(cpp,/QLatin1String\("approved"\)/);
  assert.match(cpp,/static_cast<qint64>\(m_viewerFid\)/);
  assert.match(cpp,/signer\.value\(QStringLiteral\("signer_uuid"\)\)/);
  assert.match(cpp,/origin\.host\(\) != QLatin1String\("api.neynar.com"\)/);
  assert.match(cpp,/RedirectPolicyAttribute/);
  assert.match(hpp,/Q_INVOKABLE void respondToInvite/);
  assert.match(manage,/GenyModal \{/);
  assert.match(manage,/onClicked: root\.commit\(\)/);
});

test('signed member actions reject unverified moderator roles and guessed invitations',()=>{
  assert.match(cpp,/m_roleVerified && m_canModerate/);
  assert.match(cpp,/targetFid != m_viewerFid/);
  assert.match(cpp,/No verified pending invitation for this channel/);
  assert.match(cpp,/result\.value\(QStringLiteral\("success"\)\)\.isBool\(\)/);
  assert.match(cpp,/Never optimistically grant channel roles or follows/);
  assert.match(cpp,/m_actionBusy = true/);
});

test('Neynar membership response is not confused with HyperSnap channels response',()=>{
  assert.match(browserCpp,/neynarBrowse \? body\.contains\(QStringLiteral\("members"\)\)/);
  assert.match(browserCpp,/value\(QStringLiteral\("channel"\)\)\.toObject\(\)/);
  assert.match(browserCpp,/if \(neynar\) request\.setRawHeader\("x-api-key"/);
  assert.match(browserCpp,/if \(neynar\) request\.setRawHeader/);
});

test('all new locale identifiers are defined across project languages',()=>{
  const keys = [...manage.matchAll(/qsTrId\("(genycaster\.channels\.[^"]+)"\)/g)]
    .map(match=>match[1]);
  const languages=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const lang of languages){
    const text=source('ui/i18n/qml_'+lang+'.ts');
    for(const key of keys)
      assert.equal(text.split('id="'+key+'"').length-1,1,lang+': '+key);
  }
});
