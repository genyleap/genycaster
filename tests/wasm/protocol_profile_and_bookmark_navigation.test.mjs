import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {unsignedUserDataAdd,hex} from '../../ui/wasm/farcaster_protocol.mjs';
const read=file=>readFileSync(new URL('../../'+file,import.meta.url),'utf8');
const pk='00'.repeat(32);

test('Farcaster UserDataAdd emits exact canonical protobuf type/body/slot bytes',()=>{
  const input={fid:'42',publicKeyHex:pk,timestamp:123000};
  const message=unsignedUserDataAdd({...input,type:'display',value:'GenyCaster'});
  assert.equal(hex(message.data),
    '080b102a18f8c0072001620e0802120a47656e79436173746572');
  assert.equal(message.hash.length,20);
  const banner=unsignedUserDataAdd({...input,type:'banner',value:'https://example.com/cover.png'});
  assert.equal(banner.data[1],11);
  assert.ok(hex(banner.data).includes('6221080a12'));
  assert.throws(()=>unsignedUserDataAdd({...input,type:'unsupported',value:'hi'}),/unsupported/);
  assert.throws(()=>unsignedUserDataAdd({...input,type:'display',value:'x'.repeat(33)}),/byte limit/);
  assert.throws(()=>unsignedUserDataAdd({...input,type:'bio',value:'é'.repeat(129)}),/byte limit/);
  assert.doesNotThrow(()=>unsignedUserDataAdd({...input,type:'bio',value:'line1\nline2'}));
  assert.throws(()=>unsignedUserDataAdd({...input,type:'pfp',value:'http://example.com/a.png'}),/HTTPS/);
});

test('Bookmarked cast navigates from whole card, never limited to text label',()=>{
  const ui=read('ui/components/BookmarksSurface.qml');
  const shell=read('ui/components/AppShell.qml');
  assert.match(ui,/id: bookmarkCard[\s\S]*?objectName: "bookmarkCardOpenHandler"/);
  assert.match(ui,/onTapped: root\.castRequested\(String\(bookmarkCard\.modelData\.castId \|\| ""\)\)/);
  assert.match(shell,/onCastRequested: castId => \{[\s\S]*?root\.rememberShellRouteForReturn\(\)[\s\S]*?presentationBridge\.openCast\(hash\)/);
});

test('Protocol profile editor sends only changed canonical slots with verified signer',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  const qt=read('src/presentation/qt/presentation_bridge.cpp');
  const bindings=read('ui/mobile/runtime_ui_bindings.cpp');
  const js=read('ui/wasm/browser_protocol_signer.mjs');
  const wasm=read('ui/wasm/protocol-signer-pre.js');
  assert.match(profile,/objectName: "profileProtocolSave"/);
  assert.match(profile,/presentationBridge\.updateProtocolProfile\(changes\)/);
  assert.match(profile,/mediaUpload\.browseBrowserImage\(afterIndex\)/);
  assert.match(qt,/emit protocolProfileUpdateRequested\(normalized\)/);
  assert.match(bindings,/browserSigner->start\(QStringLiteral\("profile"\)/);
  assert.match(js,/signedWrite\(fid,11,[\s\S]*?unsignedUserDataAdd/);
  assert.match(wasm,/case "profile":\s*hash = await current\.updateProfile/);
});

test('Channel image URLs allow only public immutable CID and forbid arbitrary path',()=>{
  const policy=read('services/media-gateway/browser_image_proxy.mjs');
  assert.match(policy,/ipfs\.decentralized-content\.com/);
  assert.match(policy,/hostname === 'ipfs\.decentralized-content\.com'/);
});

test('Farsi locale has new protocol profile strings; chrome uses UI direction',()=>{
  const catalog=read('ui/i18n/qml_fa_IR.ts');
  for(const key of ['genycaster.cast.in_channel','genycaster.profile.protocol_heading',
                    'genycaster.profile.update_farcaster','genycaster.profile.banner',
                    'genycaster.profile.upload_avatar'])
    assert.ok(catalog.includes('id="'+key+'"'),key);
  const cast=read('ui/components/CastItem.qml');
  const detail=read('ui/components/DetailSurface.qml');
  assert.match(cast,/id: messageRow[\s\S]*?layoutDirection: Theme\.uiDirection/);
  assert.match(detail,/id: detailBody[\s\S]*?layoutDirection: Theme\.uiDirection/);
});
