import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Replace only the two module-loading expressions with controllable fakes;
// execute the real browser pre-js state machine without a live wallet or keys.
const original = readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js', import.meta.url), 'utf8');
const source = original
  .replace("await import('./browser_protocol_signer.mjs')", "await window.__testImportSigner()")
  .replaceAll("await import('./recovery-phrase-dialog.mjs')", "await window.__testImportPhrase()");
assert.notEqual(source, original);
assert.ok(source.includes('window.__testImportPhrase()'));

const tick = () => new Promise(resolve => setTimeout(resolve, 20));
function harness(registered = false) {
    const events = [];
    const calls = [];
    let releasePrompt;
    let delayPrompt = false;
    const fakeWindow = {
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
        __testImportPhrase: async () => ({
            promptForFarcasterRecovery: async (browser,signal) => {
                calls.push('phrase-dialog');
                if (delayPrompt) return new Promise((resolve,reject) => {
                    releasePrompt = {resolve,reject};
                    signal.addEventListener('abort',()=>reject(new Error('Recovery import cancelled')), {once:true});
                });
                return 'test-vector-no-secrets';
            }
        }),
    };
    const context = vm.createContext({window:fakeWindow,console,URL,AbortController});
    vm.runInContext(source, context, {filename:'protocol-signer-pre.js'});
    const bridge=fakeWindow.__genycasterBrowserSigning;
    return {bridge,calls,events:bridge.events,
        setDelayPrompt(){delayPrompt=true;},
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
