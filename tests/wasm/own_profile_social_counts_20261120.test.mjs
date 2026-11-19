import assert from 'node:assert/strict';
import test from 'node:test';
import { readFileSync } from 'node:fs';

const read = p => readFileSync(new URL('../../' + p, import.meta.url), 'utf8');
const qml = read('ui/components/profile/ProfileSurface.qml');
const bridge = read('src/presentation/qt/presentation_bridge.cpp');
const bridgeHeader = read('src/presentation/qt/presentation_bridge.hpp');
const bindings = read('ui/app/runtime/bindings.cpp');
const property = qml.slice(
  qml.indexOf('    readonly property var activeDetail: {') +
    '    readonly property var activeDetail: {'.length,
  qml.indexOf('    property var rows', qml.indexOf('readonly property var activeDetail: {'))
).trim().replace(/\}\s*$/, '');
const evaluateDetail = new Function('root','presentationBridge',property);

test('authenticated Farcaster own profile retains real follower and following counts under HyperSnap read selection', () => {
  const owned = {fid:42,fidString:'42',followerCount:913,followingCount:157,
    socialCountsKnown:true,displayName:'Verified Farcaster',
    bio:'Complete profile biography',bannerUrl:'https://cdn.example.test/cover.png',
    location:'New York',profileUrl:'https://example.test'};
  const root = {ownProfileNavigation:true,accountFidText:'42',
    accountFid:42,accountNetwork:'farcaster',activeReadNetwork:'hypersnap',
    detail:{fid:42,fidString:'42',followerCount:1,followingCount:1,
      socialCountsKnown:true,displayName:'Foreign HyperSnap'},
    retainedDetail:{fid:42,followerCount:9}};
  const pending = evaluateDetail(root,{ownProfileReadReady:false,accountProfile:owned,
    accountName:'Verified Farcaster',accountUsername:'actual',accountAvatarUrl:''});
  assert.equal(pending.socialCountsKnown,false);
  assert.equal(pending.followerCount,undefined);
  const verified = evaluateDetail(root,{ownProfileReadReady:true,accountProfile:owned});
  assert.equal(verified.followerCount,913);
  assert.equal(verified.followingCount,157);
  assert.equal(verified.displayName,'Verified Farcaster');
  assert.equal(verified.bio,'Complete profile biography');
  assert.equal(verified.location,'New York');
  assert.equal(verified.bannerUrl,'https://cdn.example.test/cover.png');
  assert.equal(verified.profileUrl,'https://example.test');
  assert.notEqual(verified.displayName,root.detail.displayName);
});

test('HyperSnap uint64 identity stays decimal-exact across an independently selected Farcaster reader', () => {
  const fid='18446744073709551615';
  const owned={fidString:fid,fid,followerCount:1024,followingCount:512,
    socialCountsKnown:true,displayName:'HyperSnap Account'};
  const root={ownProfileNavigation:true,accountFidText:fid,
    accountFid:Number(fid),accountNetwork:'hypersnap',activeReadNetwork:'farcaster',
    detail:{fid:7,fidString:'7',displayName:'Foreign'},
    retainedDetail:{}};
  const result=evaluateDetail(root,{ownProfileReadReady:true,accountProfile:owned});
  assert.equal(result.fidString,fid);
  assert.equal(result.followerCount,1024);
  assert.equal(result.followingCount,512);
});

test('visiting another profile never reuses the authenticated account projection', () => {
  const foreign = {fid:42,fidString:'42',followerCount:2,followingCount:3,
    displayName:'Other protocol'};
  const root={ownProfileNavigation:false,detail:foreign,retainedDetail:{},
    accountFidText:'42',accountNetwork:'farcaster',activeReadNetwork:'hypersnap'};
  const detail=evaluateDetail(root,{ownProfileReadReady:true,accountProfile:{
    fid:42,fidString:'42',followerCount:9000,followingCount:9000
  }});
  assert.equal(detail.displayName,'Other protocol');
  assert.equal(detail.followerCount,2);
  assert.equal(detail.followingCount,3);
});

test('own-profile request is scoped to signed-in protocol without changing feed selection', () => {
  assert.match(bridgeHeader,/void ownProfileReadAffinityRequested\(bool ownProfile, bool nativeCustody\)/);
  assert.match(bridge,/void PresentationBridge::openMyProfile\(\)[\s\S]*?ownProfileReadAffinityRequested\(true, nativeCustodyAccount\(\)\);[\s\S]*?emit profileRequested\(m_accountFid, false\);[\s\S]*?emit feedRequested\(m_profileFeed, m_accountFid, \{\}, \{\}, false\);/);
  assert.match(bindings,/setPreferredProviderForCapability\([\s\S]*?profileCapability, access, routing::router::ProviderPreference\{/);
  assert.match(bindings,/\.require = true/);
  assert.match(bindings,/clearPreferredProviderForCapability\(\s*profileCapability, access\)/);
  assert.match(bindings,/runtime->independentReadProviderId : runtime->hostedReadProviderId/);
  assert.doesNotMatch(bindings.slice(bindings.indexOf('ownProfileReadAffinityRequested'),bindings.indexOf('preferredNetworkChanged',bindings.indexOf('ownProfileReadAffinityRequested'))),/setPreferredNetwork\(/);
});

test('same FID from another network does not overwrite account snapshot or return route',()=>{
  assert.match(bridge,/m_profileFid == m_accountFid\s*&& m_profileIsOwnAccount && m_ownProfileReadReady/);
  assert.match(bridge,/m_socialReturnOwnProfile && m_isAuthenticated && m_accountFid == m_profileFid/);
  assert.match(bridge,/previousAccountIssuer != identityIssuer/);
  assert.match(bindings,/bridge.accountIdentityIssuer\(\) != expectedIssuer/);
  assert.match(bridge,/!busy && !stale && !sourceUnavailable/);
  assert.match(bridge,/m_detail.value\(QStringLiteral\("socialCountsKnown"\)\).toBool\(\)/);
  assert.match(bindings,/state.profile->fid.value != static_cast<std::uint64_t>\(fid\)/);
  assert.match(bridgeHeader,/Q_PROPERTY\(bool ownProfileReadReady READ ownProfileReadReady NOTIFY stateChanged\)/);
});
