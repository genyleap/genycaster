import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,mkdtempSync,writeFileSync,rmSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import {join} from 'node:path';

const root=new URL('../../',import.meta.url);
const read=file=>readFileSync(new URL(file,root),'utf8');

test('supplementary totals require consent and cannot replace source identity',()=>{
  const gateway=read('src/routing/read_async_gateway.cpp');
  const composition=read('ui/mobile/runtime_composition.cpp');
  assert.match(gateway,/m_allowCompatibleProfileEnrichment && !\(\*binding\)->providesProfileCounts/);
  assert.match(gateway,/candidate\.providerId\.value != route->provider\.id\.value/);
  assert.match(gateway,/indexed->fid\.value == fid\.value/);
  assert.match(gateway,/base->followerCount = indexed->followerCount/);
  assert.match(gateway,/base->followingCount = indexed->followingCount/);
  assert.match(gateway,/state->cancelled\.load/);
  assert.match(composition,/allowCompatibleReadFallback\);/);
  assert.match(read('src/application/read_async_service.cpp'),/!fresh->socialCountsKnown/);
});

test('wasm manifest carries curated real update type and bounded changes',()=>{
  const dir=mkdtempSync(join(tmpdir(),'genycaster-release-notes-'));
  try{
    writeFileSync(join(dir,'farcaster_client.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
    for(const name of ['farcaster_client.html','qtloader.js','farcaster_client.js','genycaster-loader.js','genycaster-sw.js'])
      writeFileSync(join(dir,name),'test-'+name);
    const args=[new URL('../../tools/package_wasm_assets.py',import.meta.url).pathname,
      '--dir',dir,'--app-version','1.0.0','--release-label','Beta 1.0',
      '--release-notes',new URL('../../ui/wasm/release-notes.json',import.meta.url).pathname];
    const p=spawnSync('python3',args,{encoding:'utf8'});
    assert.equal(p.status,0,p.stderr);
    const metadata=JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    assert.equal(metadata.releaseNotes.type,'improvement');
    assert.ok(metadata.releaseNotes.changes.length>=2);
    assert.equal(metadata.releaseNotes.title,'Clearer Settings and node connection checks');
    assert.match(metadata.version,/^[a-f0-9]{20}$/);
  }finally{rmSync(dir,{recursive:true,force:true});}
});

test('wasm update UI uses same-origin manifest and never substitutes GitHub installer notes',()=>{
  const controller=read('ui/mobile/update_controller.cpp');
  const drawer=read('ui/components/UpdateNoticeDrawer.qml');
  const settings=read('ui/components/LocaleSettingsPopup.qml');
  assert.match(controller,/void UpdateController::checkWebManifest/);
  assert.match(controller,/genycaster-assets\.json/);
  assert.match(controller,/m_webCurrentBuild/);
  assert.match(controller,/m_releaseType = notes\.value/);
  assert.match(controller,/m_releaseNotes = lines\.join/);
  assert.match(controller,/QNetworkRequest::ManualRedirectPolicy/);
  assert.match(drawer,/genycaster\.update\.type_/);
  assert.match(settings,/updateController\.releaseNotes/);
  const pulse=read('ui/components/FeedPulseCard.qml');
  assert.match(pulse,/activeReadContract !== "snapchain-v1"/);
  assert.match(pulse,/!root\.engagementAvailable/);
  const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const locale of locales){
    const cat=read('ui/i18n/qml_'+locale+'.ts');
    for(const type of ['fix','improvement','feature','security','maintenance'])
      assert.ok(cat.includes('genycaster.update.type_'+type),locale+' '+type);
  }
});
