import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {unsignedBlock, unsignedFollow, hex} from '../../ui/wasm/farcaster_protocol.mjs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const key='000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f';

test('signed Block and Unblock are canonical LinkAdd/LinkRemove with type block',()=>{
    const params={fid:'42',publicKeyHex:key,targetFid:'7',timestamp:123000};
    const blocked=unsignedBlock({...params,active:true});
    const unblocked=unsignedBlock({...params,active:false});
    const followed=unsignedFollow({...params,active:true});
    assert.equal(blocked.data[1],5);
    assert.equal(unblocked.data[1],6);
    assert.ok(hex(blocked.data).includes('0a05626c6f636b'));
    assert.ok(hex(unblocked.data).includes('0a05626c6f636b'));
    assert.notDeepEqual(blocked.hash,followed.hash);
    assert.throws(()=>unsignedBlock({...params,targetFid:'42'}),/self/);
});

test('Block is routed through durable sign-and-submit on the selected writer',()=>{
    const protocol=read('src/providers/snapchain/write.cpp');
    const native=read('ui/mobile/runtime_ui_bindings.cpp');
    const main=read('ui/mobile/main.cpp');
    assert.match(protocol,/OperationKind::Block: return "MESSAGE_TYPE_LINK_ADD"/);
    assert.match(protocol,/OperationKind::Unblock: return "MESSAGE_TYPE_LINK_REMOVE"/);
    assert.match(protocol,/link_type=.*"block"/s);
    assert.match(native,/blockingMutationRequested/);
    assert.match(native,/writeService->setBlocked/);
    assert.match(native,/browserSigner->start\(QStringLiteral\("block"\)/);
    assert.match(main,/snapchainWriteBaseUrlText = selectedProtocolOrigin/);
    assert.match(main,/snapchainBaseUrlText = selectedProtocolOrigin/);
});

test('Block UI requires verified directions and includes mobile options and translations',()=>{
    const profile=read('ui/components/ProfileSurface.qml');
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    assert.match(profile,/objectName: "profileOptionsButton"\s*\/\/ The same profile actions[\s\S]*visible: true/);
    assert.match(profile,/onBlockingMutationRequested|blockingMutationRequested\(/);
    assert.match(profile,/blockedByMeKnown === true/);
    assert.match(profile,/blocksMeKnown === true/);
    assert.match(bridge,/applyProfileBlockRelation/);
    for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
        const source=read('ui/i18n/qml_'+locale+'.ts');
        for(const id of ['block_user','unblock_user','you_blocked_user','user_blocked_you'])
            assert.ok(source.includes('id="genycaster.profile.'+id+'"'),locale+':'+id);
    }
});
