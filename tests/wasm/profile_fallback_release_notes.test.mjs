import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,mkdtempSync,writeFileSync,rmSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import {join} from 'node:path';

const root=new URL('../../',import.meta.url);
const read=file=>readFileSync(new URL(file,root),'utf8');
const rootCmake=read('CMakeLists.txt');
const numericVersion=rootCmake.match(/project\(\s*Genycaster\s+VERSION\s+(\d+\.\d+\.\d+)/)?.[1];
const candidate=rootCmake.match(/set\(GENYCASTER_RELEASE_CANDIDATE\s+"(rc[1-9]\d*)"/)?.[1];
assert.ok(numericVersion&&candidate,'CMake must declare product and candidate versions');
const releaseVersion=numericVersion+'-'+candidate;
const releaseLabel='Genycaster v'+releaseVersion;

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
      writeFileSync(join(dir,name),name==='farcaster_client.html' ? '<title>@@GENYCASTER_RELEASE_LABEL@@</title><span>@@GENYCASTER_RELEASE_LABEL@@</span>' : 'test-'+name);
    const args=[new URL('../../ci/package_wasm_assets.py',import.meta.url).pathname,
      '--dir',dir,'--app-version',releaseVersion,'--release-label',releaseLabel,
      '--release-notes',new URL('../../release-notes/v'+releaseVersion+'.md',import.meta.url).pathname];
    const p=spawnSync('python3',args,{encoding:'utf8'});
    assert.equal(p.status,0,p.stderr);
    const metadata=JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    assert.equal(metadata.releaseNotes.type,'feature');
    assert.ok(metadata.releaseNotes.changes.length>=2);
    assert.equal(metadata.releaseNotes.title,releaseLabel);
    assert.equal(metadata.appVersion,releaseVersion);
    const rendered=readFileSync(join(dir,'farcaster_client.html'),'utf8');
    assert.equal(rendered.split(releaseLabel).length-1,2);
    assert.doesNotMatch(rendered,/@@GENYCASTER_RELEASE_LABEL@@/);
    assert.match(metadata.version,/^[a-f0-9]{20}$/);
  }finally{rmSync(dir,{recursive:true,force:true});}
});

test('native, WASM and tagged GitHub Releases share one root Markdown file',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const rootCmake=read('CMakeLists.txt');
  const native=read('ui/mobile/update_controller.cpp');
  const workflow=read('.github/workflows/ci.yml');
  assert.match(rootCmake,/release-notes\/\$\{GENYCASTER_RELEASE_TAG\}\.md/);
  assert.match(cmake,/qt_add_resources\(farcaster_client genycaster_release_notes/);
  assert.match(cmake,/--release-notes "\$\{GENYCASTER_RELEASE_NOTES_FILE\}"/);
  assert.match(native,/genycaster-release-notes\/v%1\.md/);
  assert.match(workflow,/body_path: release-notes\/\$\{\{ github\.ref_name \}\}\.md/);
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
  assert.match(controller,/releases\?per_page=20/);
  assert.match(controller,/compareReleaseVersions\(parsed, current\)/);
  assert.match(controller,/loadBundledReleaseNotes\(m_appVersion/);
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
