import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Replace module-loading expressions with controllable fakes;
// execute the real browser pre-js state machine without a live wallet or keys.
const original = readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js', import.meta.url), 'utf8');
const source = original
  .replaceAll("await import('./browser_protocol_signer.mjs')", "await window.__testImportSigner()")
  .replaceAll("await import('./recovery-phrase-dialog.mjs')", "await window.__testImportPhrase()")
  .replace("await import('./farcaster_custody_recovery.mjs')", "await window.__testImportIdentity()")
  .replaceAll("await import('./hypersnap_native_account.mjs')", "await window.__testImportNativeRegistry()")
  .replaceAll("await import('./farcaster_recovery.mjs')", "await window.__testImportDerivation()")
  .replace("await import('./deployment_config.mjs')", "await window.__testImportDeployment()")
  .replace("await import('./secure_signer_vault.mjs')", "await window.__testImportVault()")
  .replace("import('./browser_device_session.mjs')", "window.__testImportDevice()");
assert.notEqual(source, original);
assert.ok(source.includes('window.__testImportPhrase()'));

const tick = () => new Promise(resolve => setTimeout(resolve, 55));
const nativeFid = '9007199254740993';
function harness(registered = false, initialRemembered = null) {
    const events = [];
    const calls = [];
    const storage=new Map();
    let remembered=initialRemembered;
    let releasePrompt;
    let delayPrompt = false;
    // Minimal DOM for the visible recovery-preparation layer; real phrase
    // form imports remain mocked, but the browser bridge executes unchanged.
    const fakeElement=()=>({
        style:{},setAttribute(){},append(){},remove(){},textContent:''
    });
    const fakeDocument={
        getElementById(){return null;},createElement(){return fakeElement();},
        documentElement:{dataset:{theme:'light'}},
        body:{appendChild(){},style:{}},
    };
    const fakeWindow = {
        document:fakeDocument,
        requestAnimationFrame:fn=>{fn();return 1;},
        localStorage:{getItem:key=>storage.get(key)||null,setItem:(key,value)=>storage.set(key,value),removeItem:key=>storage.delete(key)},
        __testImportDevice: async()=>({createDeviceSession:()=>({
            remember:async (fid,protocol='farcaster-snapchain')=>{
                remembered={fid,protocol};return fid;
            },
            restore:async()=>remembered?.fid??null,
            protocolFor:async()=>remembered?.protocol??'',
            clear:async()=>{remembered=null;}
        })}),
        __testImportDeployment:async()=>({deployment:{hypersnapOrigin:'https://hypersnap.example.test'}}),
        __testImportVault:async()=>({createBrowserSignerVault:()=>({})}),
        __testImportNativeRegistry:async()=>({createBrowserNativeRegistry:()=>({
            recover:async()=>({state:'confirmed',fid:nativeFid}),
            lookup:async()=>{calls.push('lookup-native-registry');return {state:'confirmed',fid:nativeFid};},
        })}),
        __testImportDerivation:async()=>({deriveFarcasterCustody:async()=>({
            address:'0x1111111111111111111111111111111111111111',
            dispose(){calls.push('custody-disposed');},
        })}),
        location: {protocol:'https:', hostname:'example.test', origin:'https://example.test'},
        isSecureContext:true, crypto:{subtle:{sign(){}}}, indexedDB:{open(){}},
        __testImportSigner: async () => ({
            createBrowserProtocolSigner: ({onState}) => ({
                signedOut() {calls.push('signed-out');},
                async restore(fid) {
                    calls.push('restore:'+fid);
                    onState({fid,phase:registered?'ready':'missing',ready:registered,busy:false});
                    return registered ? { fid } : null;
                },
                async provision(fid,provider) {
                    calls.push('authorize-native:'+fid);
                    const custody=await provider(fid);
                    assert.equal(custody.address,'0x1111111111111111111111111111111111111111');
                    custody.dispose();
                    onState({fid,phase:'ready',ready:true,busy:false});
                    return {fid};
                },
                async provisionFromRecovery(fid,phrase) {
                    calls.push('authorize:'+fid+':'+phrase);
                    onState({fid,phase:'ready',ready:true,busy:false});
                },
                async recoverAccountFromPhrase(phrase) {
                    calls.push('recover-account:'+phrase);
                    onState({fid:'101',phase:'ready',ready:true,busy:false});
                    return '101';
                }
            })
        }),
        __testImportIdentity: async () => ({
            verifyFarcasterRecovery: async phrase=>{
                calls.push('recover-account:'+phrase);
                return '101';
            },
        }),
        __testImportPhrase: async () => ({
            promptForFarcasterRecovery: async (browser,signal,onAuthorize) => {
                calls.push('phrase-dialog');
                if (delayPrompt) return new Promise((resolve,reject) => {
                    releasePrompt = {resolve,reject};
                    signal.addEventListener('abort',()=>reject(new Error('Recovery import cancelled')), {once:true});
                });
                return onAuthorize ? onAuthorize('test-vector-no-secrets') : 'test-vector-no-secrets';
            }
        }),
    };
    const context = vm.createContext({
        window:fakeWindow,document:fakeDocument,console,URL,AbortController,
    });
    vm.runInContext(source, context, {filename:'protocol-signer-pre.js'});
    const bridge=fakeWindow.__genycasterBrowserSigning;
    return {bridge,calls,events:bridge.events,storage,
        get remembered(){return remembered;},
        setDelayPrompt(){delayPrompt=true;},
        failSignerInit(){fakeWindow.__testImportSigner=async()=>{
            throw new Error('Signing service temporarily unavailable');
        };},
        get releasePrompt(){return releasePrompt;}};
}
test('ordinary session restore never collects a recovery phrase',async()=>{
    const h=harness(false);
    h.bridge.start('restore','101','https://snapchain.example.test','');
    await tick();
    assert.deepEqual(h.calls,['restore:101']);
});
test('first-run setup reuses a confirmed signer without prompting',async()=>{
    const h=harness(true);
    h.bridge.start('restore-for-setup','101','https://snapchain.example.test','');
    await tick();
    assert.deepEqual(h.calls,['restore:101']);
    assert.equal(h.events.some(e=>e.phase==='ready'),true);
});
test('first-run setup prompts once only after missing network signer',async()=>{
    const h=harness(false);
    h.bridge.start('restore-for-setup','101','https://snapchain.example.test','');
    await tick();
    assert.deepEqual(h.calls,['restore:101','phrase-dialog','authorize:101:test-vector-no-secrets']);
});
test('sign-out during recovery closes prompt and prevents stale authorization',async()=>{
    const h=harness(false);
    h.setDelayPrompt();
    h.bridge.start('restore-for-setup','101','https://snapchain.example.test','');
    await tick();
    assert.equal(h.calls.includes('phrase-dialog'),true);
    h.bridge.start('signed-out','','https://snapchain.example.test','');
    await tick();
    assert.equal(h.calls.some(c=>c.startsWith('authorize:')),false);
});
test('new account cancels earlier recovery and never authorizes old fid',async()=>{
    const h=harness(false);
    h.setDelayPrompt();
    h.bridge.start('restore-for-setup','101','https://snapchain.example.test','');
    await tick();
    h.bridge.start('restore','202','https://snapchain.example.test','');
    await tick();
    assert.deepEqual(h.calls.filter(c=>c.startsWith('restore:')),['restore:101','restore:202']);
    assert.equal(h.calls.some(c=>c.startsWith('authorize:')),false);
});


test('direct Setup recovery returns only a verified FID and the one-shot request ID',async()=>{
    const h=harness(false);
    const requestId='12345678-1234-4234-8234-123456789abc';
    h.bridge.start('recover-account','','https://snapchain.example.test',
                   JSON.stringify({requestId}));
    await tick();
    assert.ok(h.calls.includes('phrase-dialog'), JSON.stringify({calls:h.calls,events:h.events}));
    assert.ok(h.calls.includes('recover-account:test-vector-no-secrets'));
    const event=h.events.find(e=>e.kind==='setup-account');
    assert.equal(event.phase,'ready');
    assert.equal(event.fid,'101');
    assert.equal(event.requestId,requestId);
    assert.ok(!JSON.stringify(h.events).includes('test-vector-no-secrets'));
});

test('cancelling direct Setup recovery cannot emit an authenticated identity',async()=>{
    const h=harness(false);
    h.setDelayPrompt();
    h.bridge.start('recover-account','','https://snapchain.example.test',
                   JSON.stringify({requestId:'aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee'}));
    await tick();
    h.bridge.start('signed-out','','https://snapchain.example.test');
    await tick();
    assert.equal(h.events.some(e=>e.kind==='setup-account'&&e.phase==='ready'),false);
    assert.equal(h.calls.some(c=>c.startsWith('recover-account:')),false);
});

test('verified sign-in is remembered locally and refresh restores device proof',async()=>{
    const h=harness(false);
    h.bridge.start('restore','101','https://snapchain.example.test',
        JSON.stringify({rememberLocal:true}));
    await tick();
    h.bridge.start('signed-out','','https://snapchain.example.test');
    await tick();
    const requestId='eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee';
    h.bridge.start('resume-account','','https://snapchain.example.test',
        JSON.stringify({requestId}));
    await tick();
    const restored=h.events.find(event=>event.kind==='setup-account'&&event.requestId===requestId);
    assert.equal(restored?.phase,'ready');
    assert.equal(restored?.fid,'101');
    assert.equal(restored?.localDevice,true);
    assert.ok(!JSON.stringify(h.events).includes('privateKey'));
});

test('read-only HyperSnap recovery reopens after a reload without a registered signer',async()=>{
    const base='https://snapchain.example.test';
    const protocol='hypersnap-v2';
    const first=harness();
    first.bridge.start('recover-hypersnap','',base,JSON.stringify({
        requestId:'aaaaaaaa-bbbb-4ccc-8ddd-000000000003',protocol,
    }));
    await tick();
    assert.equal(first.events.find(event=>event.kind==='setup-account')?.phase,'ready');
    assert.deepEqual(first.remembered,{fid:nativeFid,protocol});

    const reloaded=harness(false,first.remembered);
    const requestId='aaaaaaaa-bbbb-4ccc-8ddd-000000000004';
    reloaded.bridge.start('resume-account','',base,JSON.stringify({requestId}));
    await tick();
    const restored=reloaded.events.find(event=>event.kind==='setup-account');
    assert.equal(restored?.phase,'ready');
    assert.equal(restored?.fid,nativeFid);
    assert.equal(restored?.protocol,protocol);
    assert.equal(restored?.localDevice,true);
    assert.equal(restored?.requestId,requestId);
    assert.deepEqual(reloaded.calls,[]);

    // The native bridge asks about publishing only after the viewer opens.
    reloaded.bridge.start('restore',nativeFid,base,JSON.stringify({protocol}));
    await tick();
    assert.deepEqual(reloaded.calls,['restore:'+nativeFid]);
    const signer=reloaded.events.find(event=>event.kind==='signer');
    assert.equal(signer?.phase,'missing');
    assert.equal(signer?.ready,false);
});

test('explicit logout disables automatic sign-in and erases remembered device',async()=>{
    const h=harness(false);
    h.bridge.start('restore','101','https://snapchain.example.test',
        JSON.stringify({rememberLocal:true}));
    await tick();
    h.bridge.start('forget-account','','https://snapchain.example.test');
    await tick();
    assert.equal(h.storage.get('genycaster.device-session.signed-out'),'1');
    h.bridge.start('resume-account','','https://snapchain.example.test',
        JSON.stringify({requestId:'ffffffff-ffff-4fff-8fff-ffffffffffff'}));
    await tick();
    assert.equal(h.events.some(event=>event.kind==='setup-account'&&event.localDevice&&event.phase==='ready'),false);
    assert.ok(h.events.some(event=>event.kind==='setup-account'&&event.phase==='cancelled'),
        'explicit sign-out must reject remembered-device restoration');
});

test('mobile recovery opens local numbered-words dialog before signer initialization',async()=>{
    const h=harness(false);
    h.setDelayPrompt();
    h.failSignerInit();
    h.bridge.start('recover-account','','https://snapchain.example.test',
        JSON.stringify({requestId:'aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeef'}));
    await tick();
    assert.ok(h.calls.includes('phrase-dialog'));
    assert.equal(h.events.some(e=>e.kind==='setup-account'&&e.phase==='failed'),false,
        'signer transport is not contacted before phrase authorization');
    h.bridge.start('signed-out','','https://snapchain.example.test');
    await tick();
});

test('a repeated Setup recovery click produces a visible one-shot failure instead of a lost write event',async()=>{
    const h=harness(false);
    h.setDelayPrompt();
    const first='aaaaaaaa-bbbb-4ccc-8ddd-000000000001';
    const second='aaaaaaaa-bbbb-4ccc-8ddd-000000000002';
    h.bridge.start('recover-account','','https://snapchain.example.test',
        JSON.stringify({requestId:first}));
    await tick();
    assert.equal(h.calls.filter(c=>c==='phrase-dialog').length,1);
    h.bridge.start('recover-account','','https://snapchain.example.test',
        JSON.stringify({requestId:second}));
    await tick();
    const failed=h.events.find(e=>e.kind==='setup-account'&&e.requestId===second);
    assert.equal(failed?.phase,'failed');
    assert.match(failed?.error||'',/in progress/);
    assert.equal(h.events.some(e=>e.kind==='write'&&e.error),false);
    h.bridge.start('signed-out','','https://snapchain.example.test','');
    await tick();
});

test('native HyperSnap authorization requests the browser-only custody recovery flow and enables signer',async()=>{
    const h=harness(false);
    h.bridge.start('provision',nativeFid,'https://snapchain.example.test',
        JSON.stringify({protocol:'hypersnap-v2'}));
    await tick();
    assert.deepEqual(h.calls,[
        'restore:'+nativeFid,
        'phrase-dialog',
        'authorize-native:'+nativeFid,
        'lookup-native-registry',
        'custody-disposed',
    ]);
    const ready=h.events.find(e=>e.kind==='signer'&&e.phase==='ready');
    assert.equal(ready?.protocol,'hypersnap-v2');
    assert.equal(ready?.fid,nativeFid);
    assert.equal(ready?.ready,true);
    assert.ok(!JSON.stringify(h.events).includes('test-vector-no-secrets'));
});

test('a native HyperSnap browser signer already authorized in the vault restores without a new recovery prompt',async()=>{
    const h=harness(true);
    h.bridge.start('restore',nativeFid,'https://snapchain.example.test',
        JSON.stringify({protocol:'hypersnap-v2'}));
    await tick();
    assert.deepEqual(h.calls,['restore:'+nativeFid]);
    assert.ok(h.events.some(e=>e.kind==='signer'&&e.protocol==='hypersnap-v2'
                              &&e.ready===true));
});

test('repeated HyperSnap Enable interactions recovers previously registered signer without asking for words',async()=>{
    const h=harness(true);
    h.bridge.start('provision',nativeFid,'https://snapchain.example.test',
        JSON.stringify({protocol:'hypersnap-v2'}));
    await tick();
    assert.deepEqual(h.calls,['restore:'+nativeFid]);
    assert.equal(h.events.some(e=>e.kind==='signer' &&e.protocol==='hypersnap-v2'
                          &&e.phase==='ready'&&e.ready===true),true);
});
