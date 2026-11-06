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
const channel=rootCmake.match(/set\(GENYCASTER_RELEASE_CHANNEL\s+"(stable|alpha|beta|rc)"/)?.[1];
const channelNumber=rootCmake.match(/set\(GENYCASTER_RELEASE_CHANNEL_NUMBER\s+"(\d+)"/)?.[1];
assert.ok(numericVersion&&channel&&channelNumber,'CMake must declare independent release fields');
const releaseVersion=numericVersion+(channel==='stable'?'':'-'+channel+channelNumber);
const sourceNotes=JSON.parse(read('release-notes/v'+releaseVersion+'.json'));

test('supplementary totals require consent and cannot replace source identity',()=>{
  const gateway=read('src/routing/read_async_gateway.cpp');
  const composition=read('ui/app/runtime_composition.cpp');
  assert.match(gateway,/m_allowCompatibleProfileEnrichment && !\(\*binding\)->providesProfileCounts/);
  assert.match(gateway,/candidate\.providerId\.value != route->provider\.id\.value/);
  assert.match(gateway,/indexed->fid\.value == fid\.value/);
  assert.match(gateway,/base->followerCount = indexed->followerCount/);
  assert.match(gateway,/base->followingCount = indexed->followingCount/);
  assert.match(gateway,/state->cancelled\.load/);
  assert.match(composition,/allowCompatibleReadFallback\);/);
  assert.match(read('src/application/read_async_service.cpp'),/!fresh->socialCountsKnown/);
});

test('WASM metadata carries independently versioned, fully localized release information',()=>{
  const dir=mkdtempSync(join(tmpdir(),'genycaster-release-notes-'));
  try{
    writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
    for(const name of ['genycaster.html','qtloader.js','genycaster.js','genycaster-loader.js','genycaster-sw.js'])
      writeFileSync(join(dir,name),name==='genycaster.html'
        ? '<title>@@GENYCASTER_PRODUCT_NAME@@</title><span>@@GENYCASTER_RELEASE_VERSION@@</span>'
        : 'test-'+name);
    const args=[new URL('../../ci/package_wasm_assets.py',import.meta.url).pathname,
      '--dir',dir,'--app-version',releaseVersion,'--product-name','Genycaster',
      '--version-number',numericVersion,'--release-channel',channel,
      '--release-channel-number',channelNumber,
      '--release-notes',new URL('../../release-notes/v'+releaseVersion+'.json',import.meta.url).pathname];
    const result=spawnSync('python3',args,{encoding:'utf8'});
    assert.equal(result.status,0,result.stderr);
    const metadata=JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    assert.equal(metadata.releaseNotes.type,'security');
    assert.equal(metadata.releaseNotes.changes.length,6);
    assert.deepEqual(metadata.releaseNotes,sourceNotes);
    assert.equal(metadata.productName,'Genycaster');
    assert.equal(metadata.versionNumber,numericVersion);
    assert.equal(metadata.releaseChannel,channel);
    assert.equal(metadata.releaseChannelNumber,Number(channelNumber));
    assert.equal(metadata.appVersion,releaseVersion);
    assert.equal(metadata.releaseNotes.translations['fa-IR'].title,'مجوزدهی ایمن‌تر به دستگاه');
    assert.notEqual(metadata.releaseNotes.translations['fa-IR'].title,'Genycaster v'+releaseVersion);
    const rendered=readFileSync(join(dir,'genycaster.html'),'utf8');
    assert.match(rendered,/<title>Genycaster<\/title>/);
    assert.match(rendered,new RegExp('<span>v'+releaseVersion+'<\/span>'));
    assert.doesNotMatch(rendered,/@\@GENYCASTER_/);
    assert.match(metadata.version,/^[a-f0-9]{20}$/);
  }finally{rmSync(dir,{recursive:true,force:true});}
});

test('no UI hardcodes numeric versions or concatenates product and version',()=>{
  for(const path of [
    'ui/components/SideNavigation.qml',
    'ui/components/AppHeader.qml',
    'ui/components/InitialSetup.qml',
    'ui/components/LocaleSettingsPopup.qml'
  ]){
    const qml=read(path);
    assert.doesNotMatch(qml,/\b\d+\.\d+\.\d+(?:-rc\d+)?\b/,path);
    assert.doesNotMatch(qml,/"Genycaster v"\s*\+/,path);
  }
  assert.match(read('ui/components/AppHeader.qml'),/visible: !root\.compact/);
  const html=read('ui/wasm/genycaster-shell.html');
  assert.match(html,/<title>@@GENYCASTER_PRODUCT_NAME@@<\/title>/);
  assert.match(html, /@@GENYCASTER_RELEASE_VERSION@@/);
  assert.doesNotMatch(html,/\b\d+\.\d+\.\d+(?:-rc\d+)?\b/);
  assert.match(read('ci/package_wasm_assets.py'),/html\.replace\(release_placeholder, "v" \+ args\.app_version\)/);
  assert.match(read('ui/CMakeLists.txt'),/GENYCASTER_APP_VERSION="/);
  assert.match(read('ui/CMakeLists.txt'),/--release-channel/);
});

test('all platform notes use canonical JSON and generated GitHub Markdown',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const rootCmake=read('CMakeLists.txt');
  const native=read('ui/app/update_controller.cpp');
  const workflow=read('.github/workflows/ci.yml');
  assert.match(rootCmake,/release-notes\/\$\{GENYCASTER_RELEASE_TAG\}\.json/);
  assert.match(rootCmake,/release-notes\/\$\{GENYCASTER_RELEASE_TAG\}\.md/);
  assert.match(cmake,/qt_add_resources\(genycaster genycaster_release_notes/);
  assert.match(cmake,/--release-notes "\$\{GENYCASTER_RELEASE_NOTES_FILE\}"/);
  assert.match(native,/genycaster-release-notes\/v%1\.json/);
  assert.match(native,/genycaster-locale:/);
  assert.match(workflow,/body_path: release-notes\/\$\{\{ github\.ref_name \}\}\.md/);
  for(const tag of ['en-US','fa-IR','ar-SA','de-DE','es-ES','fr-FR',
                    'hi-IN','ja-JP','ko-KR','pt-BR','ru-RU','zh-CN']){
    const local=sourceNotes.translations[tag];
    assert.ok(local.title&&local.summary,tag);
    assert.equal(Object.keys(local.items).length,sourceNotes.changes.length,tag);
    assert.ok(read('release-notes/v'+releaseVersion+'.md').includes('<!-- genycaster-locale:'+tag+' -->'));
  }
});

test('wasm update UI uses same-origin manifest and never substitutes GitHub installer notes',()=>{
  const controller=read('ui/app/update_controller.cpp');
  const drawer=read('ui/components/UpdateNoticeDrawer.qml');
  const settings=read('ui/components/LocaleSettingsPopup.qml');
  assert.match(controller,/void UpdateController::checkWebManifest/);
  assert.match(controller,/genycaster-assets\.json/);
  assert.match(controller,/m_webCurrentBuild/);
  assert.match(controller,/m_releaseData = notes/);
  assert.match(controller,/m_releaseNotes = bullets\.join/);
  assert.match(controller,/QNetworkRequest::ManualRedirectPolicy/);
  assert.match(controller,/releases\?per_page=20/);
  assert.match(controller,/compareReleaseVersions\(parsed, current\)/);
  assert.match(controller,/loadBundledReleaseNotes\(m_appVersion/);
  assert.match(controller,/setLanguageTag\(QString tag\)/);
  assert.match(read('ui/app/main.cpp'),/updateController\.setLanguageTag/);
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
    for(const id of ['version_number','channel_alpha','channel_beta','channel_rc',
                      'channel_stable','prerelease_info','up_to_date','build_label'])
      assert.ok(cat.includes('genycaster.update.'+id),locale+' '+id);
  }
});
