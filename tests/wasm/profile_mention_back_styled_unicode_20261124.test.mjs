import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const shell=read('ui/components/shell/AppShell.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');
const fontBootstrap=read('ui/app/bootstrap/fonts.cpp');
const cmake=read('ui/CMakeLists.txt');

function navigator({returnBehavior='stuck',route='notifications',target=601}={}) {
  const cast='0x'+'a'.repeat(40);
  const calls=[];
  const root={
    shellRoute:'',pendingReturnShellRoute:route,
    pendingMentionOriginCastId:'',pendingMentionTargetFid:'',
    rememberShellRouteForReturn(){calls.push('remember-route');},
    returnToPendingShellRoute(){calls.push('return-shell-route');}
  };
  const bridge={
    activeView:'cast',activeProfileFidText:'0',detail:{castId:cast},
    returnFromDetail(){
      calls.push('bridge-back');
      if(returnBehavior==='success')this.activeView='cast';
      if(returnBehavior==='nested')this.activeProfileFidText=String(target);
      if(returnBehavior==='feed')this.activeView='feed';
    },
    clearDetailHistory(){calls.push('clear-detail-history')},
    openCast(id){calls.push(['restore-cast',id]);this.activeView='cast';this.detail={castId:id};},
    openContentProfileFid(fid,contract,fromNotifications){
      calls.push(['open-profile',fid,contract,fromNotifications]);
      this.activeView='profile';this.activeProfileFidText=String(fid);
    }
  };
  const section=shell.slice(shell.indexOf('    function returnFromCastDetail() {'),
    shell.indexOf('    property bool profileEditorRequestedFromSettings:'));
  const functions=new Function('root','presentationBridge','providerProfiles',
    section+';return {openProfileFromContent,returnFromProfileSurface,restoreMentionCastWhenBackDidNotNavigate};')(
      root,bridge,{activeReadContract:'snapchain-v1'});
  root.restoreMentionCastWhenBackDidNotNavigate=functions.restoreMentionCastWhenBackDidNotNavigate;
  return {root,bridge,calls,cast,functions};
}

test('foreign mentioned profile Back recovers its originating Cast only when native history is stuck',()=>{
  const env=navigator();
  env.functions.openProfileFromContent(601);
  assert.equal(env.root.pendingMentionOriginCastId,env.cast);
  assert.deepEqual(env.calls[0],['open-profile',601,'snapchain-v1',true]);
  env.functions.returnFromProfileSurface();
  assert.equal(env.bridge.activeView,'cast');
  assert.deepEqual(env.calls.at(-1),['restore-cast',env.cast]);
  assert.equal(env.root.pendingMentionOriginCastId,'');
});

test('a successful native Cast breadcrumb remains authoritative and never doubles navigation',()=>{
  const env=navigator({returnBehavior:'success'});
  env.functions.openProfileFromContent(601);
  env.functions.returnFromProfileSurface();
  assert.equal(env.bridge.activeView,'cast');
  assert.equal(env.calls.filter(row=>Array.isArray(row)&&row[0]==='restore-cast').length,0);
});

test('nested public profile Back does not accidentally skip its intermediate profile',()=>{
  const env=navigator({returnBehavior:'nested',target:601});
  env.functions.openProfileFromContent(601);
  env.functions.openProfileFromContent(602);
  assert.equal(env.root.pendingMentionTargetFid,'602');
  env.functions.returnFromProfileSurface();
  assert.equal(env.bridge.activeProfileFidText,'601');
  assert.equal(env.bridge.activeView,'profile');
  assert.equal(env.calls.filter(row=>Array.isArray(row)&&row[0]==='restore-cast').length,0);
});

test('ProfileSurface never clears already-visible posts when late banner or bio metadata arrives',()=>{
  const handler=profile.slice(profile.indexOf('    onDetailChanged: {'),
    profile.indexOf('    onRowsChanged: stableModelSyncTimer.restart()'));
  assert.match(handler,/root\.synchronizeActiveDetail\(\)/);
  assert.doesNotMatch(handler,/profileRowsModel\.clear/);
  assert.match(profile,/onProfileFidTextChanged: Qt\.callLater\(root\.reconcileProfileRowsIdentity\)/);
  assert.match(profile,/onProfileNetworkChanged: Qt\.callLater\(root\.reconcileProfileRowsIdentity\)/);
  const switchBody=profile.slice(profile.indexOf('function reconcileProfileRowsIdentity()'),
    profile.indexOf('onProfileFidTextChanged:',profile.indexOf('function reconcileProfileRowsIdentity()')));
  assert.match(switchBody,/const key = root\.profileNetwork \+ ":" \+ root\.profileFidText/);
  assert.match(switchBody,/profileRowsModel\.clear\(\)/);
  assert.match(switchBody,/stableModelSyncTimer\.restart\(\)/);
  assert.match(profile,/smoothReveal: true/);
  const banner=profile.slice(profile.indexOf("id: bannerFrame"),profile.indexOf("id: bannerImage"));
  assert.match(banner,/height: Math\.max\(root\.compact \? 132 : 156/);
  assert.doesNotMatch(banner,/height: Boolean\(root\.activeDetail\.bannerUrl\)/);
  assert.match(profile,/retainPreviousWhileLoading: false/);
});

test('temporarily empty same-FID metadata preserves the previous cover without leaking other profiles',()=>{
  const src=profile.slice(profile.indexOf('    function resolveActiveDetail() {'),
    profile.indexOf('    function synchronizeActiveDetail()'));
  const resolve=new Function('root','presentationBridge',src+'return resolveActiveDetail();');
  const prior={fid:601,displayName:'Fancy𝙠𝙖𝙢𝙗𝙞𝙯',bannerUrl:'https://example.com/cover.jpg',bio:'𝓓𝓪𝓲𝓵𝔂'};
  const root={
    ownProfileNavigation:false,detail:{},activeDetail:prior,activeDetailNetwork:'farcaster',
    profileNetwork:'farcaster',accountFidText:'1',accountFid:1,accountNetwork:'farcaster',
    activeReadNetwork:'farcaster'
  };
  assert.deepEqual(resolve(root,{activeProfileFidText:'601'}),prior);
  assert.deepEqual(resolve(root,{activeProfileFidText:'602'}),{});
  root.profileNetwork='hypersnap';
  assert.deepEqual(resolve(root,{activeProfileFidText:'601'}),{});
  root.profileNetwork='farcaster';root.detail={fid:601,displayName:'Edited',bannerUrl:''};
  assert.equal(resolve(root,{activeProfileFidText:'601'}).bannerUrl,'');
});

test('WASM embeds pinned Unicode mathematical font and uses only missing-glyph fallback',()=>{
  assert.match(cmake,/genycaster_styled_unicode_font/);
  assert.match(cmake,/NotoSansMath-Regular\.ttf/);
  assert.match(cmake,/3f495fe933c06786e4d5f6d86b8ee70b6753a68ee3b9d87528726de0f6e2c47d/);
  assert.match(fontBootstrap,/QFontDatabase::addApplicationFallbackFontFamily\(\s*QChar::Script_Common, mathFamilies\.front\(\)/);
  assert.match(fontBootstrap,/QFontDatabase::addApplicationFallbackFontFamily\(\s*QChar::Script_Latin, mathFamilies\.front\(\)/);
  assert.doesNotMatch(fontBootstrap,/normalize\(.*NFKC/);
  assert.ok([...'𝙠𝙖𝙢𝙗𝙞𝙯'].every(c=>c.codePointAt(0)>=0x1d400));
});

test('missing history that falls through to Feed still restores the mentioned Conversation first',()=>{
  const env=navigator({returnBehavior:'feed'});
  env.functions.openProfileFromContent(601);
  env.functions.returnFromProfileSurface();
  assert.equal(env.bridge.activeView,'cast');
  assert.deepEqual(env.calls.at(-1),['restore-cast',env.cast]);
});

test('hardware/mobile Back uses the same profile return handling as the visible Back button',()=>{
  const platform=shell.slice(shell.indexOf('    function handlePlatformBack() {'),
    shell.indexOf('    GenyModal {',shell.indexOf('    function handlePlatformBack() {')));
  assert.match(platform,/if \(presentationBridge\.activeView === "profile"\) \{\s*root\.returnFromProfileSurface\(\)/);
  assert.match(platform,/if \(presentationBridge\.activeView === "cast"\) \{\s*root\.returnFromCastDetail\(\)/);
});
