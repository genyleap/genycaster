import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,readFileSync,writeFileSync,mkdirSync,rmSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,dirname} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const modules=[
 'browser_setup_wallet.mjs','hypersnap_native_account.mjs','hypersnap_native_pow_worker.mjs',
 'farcaster_new_account.mjs','farcaster_recovery.mjs','farcaster_custody_recovery.mjs',
 'openproof_recovery.mjs','browser_protocol_signer.mjs','browser_device_session.mjs',
 'browser_account_session.mjs','browser_media_upload.mjs','client_image_optimizer.mjs',
 'deployment_config.mjs','farcaster_protocol.mjs','secure_signer_vault.mjs',
 'recovery-phrase-dialog.mjs','vendor/noble-secp256k1.mjs','vendor/bip39-english.mjs'
];
test('account module graph is immutable, complete, versioned and idempotent',()=>{
 const dir=mkdtempSync(join(tmpdir(),'genycaster-auth-graph-'));
 try{
  writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
  writeFileSync(join(dir,'genycaster.html'),'<script src="genycaster.js"></script>');
  writeFileSync(join(dir,'genycaster.js'),modules.map(m=>'import("./'+m+'")').join(';'));
  for(const m of modules){mkdirSync(dirname(join(dir,m)),{recursive:true});writeFileSync(join(dir,m),'// '+m);}
  const pack=()=>{
   const result=spawnSync('python3',['ci/package_wasm_assets.py','--dir',dir],{encoding:'utf8'});
   assert.equal(result.status,0,result.stderr);
   return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
  };
  const first=pack(),root=first.files.find(f=>f.endsWith('/browser_setup_wallet.mjs')).split('/').slice(0,2).join('/');
  assert.match(root,/^auth-modules\/[0-9a-f]{20}$/);
  const boot=readFileSync(join(dir,'genycaster.js'),'utf8');
  for(const name of modules){
   const path=root+'/'+name;assert.ok(first.files.includes(path));assert.ok(boot.includes('./'+path));
   assert.equal(first.integrity[path],createHash('sha256').update(readFileSync(join(dir,path))).digest('hex'));
  }
  assert.deepEqual(pack(),first);
  writeFileSync(join(dir,'browser_setup_wallet.mjs'),'// changed');
  const next=pack(),newRoot=next.files.find(f=>f.endsWith('/browser_setup_wallet.mjs')).split('/').slice(0,2).join('/');
  assert.notEqual(newRoot,root);assert.ok(existsSync(join(dir,root,'browser_setup_wallet.mjs')));
  assert.equal(readFileSync(join(dir,root,'browser_setup_wallet.mjs'),'utf8'),'// browser_setup_wallet.mjs');
  assert.ok(!readFileSync(join(dir,'genycaster.js'),'utf8').includes('./'+root+'/'));
  assert.deepEqual(pack(),next);
  rmSync(join(dir,'secure_signer_vault.mjs'));
  const bad=spawnSync('python3',['ci/package_wasm_assets.py','--dir',dir],{encoding:'utf8'});
  assert.notEqual(bad.status,0);assert.match(bad.stderr,/Incomplete browser account module graph/);
 }finally{rmSync(dir,{recursive:true,force:true});}
});
