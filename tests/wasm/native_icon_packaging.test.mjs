import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const read=p=>readFileSync(resolve(root,p));
const text=p=>read(p).toString('utf8');
const pngHeader=Buffer.from([137,80,78,71,13,10,26,10]);
function pngSize(blob) {
  assert.ok(blob.subarray(0,8).equals(pngHeader),'valid PNG signature');
  assert.equal(blob.subarray(12,16).toString(),'IHDR');
  return {width:blob.readUInt32BE(16),height:blob.readUInt32BE(20),
    depth:blob[24],colorType:blob[25]};
}

test('native icons share one canonical master and are explicitly packaged',()=>{
  const cmake=text('ui/CMakeLists.txt')+text('properties/ConfigureTarget.cmake')+
    ['Android','iOS','macOS','Windows','Linux'].map(platform=>
      text('properties/'+platform+'/Configure.cmake')).join('\n');
  const main=text('ui/app/main.cpp');
  assert.match(cmake,/set\(_gcIcon .*wasm\/genycaster-512\.png/);
  assert.match(cmake,/qt_add_resources\(genycaster genycaster_app_icon/);
  assert.match(cmake,/MACOSX_BUNDLE_ICON_FILE "genycaster\.icns"/);
  assert.match(cmake,/genycaster-app\.rc/);
  assert.match(cmake,/QT_ANDROID_PACKAGE_SOURCE_DIR/);
  assert.match(cmake,/Assets\.xcassets/);
  assert.match(cmake,/genycaster\.desktop/);
  assert.match(main,/QGuiApplication::setWindowIcon\(QIcon/);
  assert.equal(pngSize(read('ui/wasm/genycaster-512.png')).width,512);
  assert.match(text('cmake/generate_app_icons.py'),/requires only Python/i);
});
test('Windows PE icon has all normal shell sizes including 256px',()=>{
  const ico=read('properties/Windows/genycaster.ico');
  assert.equal(ico.readUInt16LE(0),0);
  assert.equal(ico.readUInt16LE(2),1);
  const count=ico.readUInt16LE(4);
  assert.ok(count>=6);
  const sizes=[];
  for(let i=0;i<count;i++){
    const off=6+i*16;
    const size=ico[off]||256;
    const len=ico.readUInt32LE(off+8);
    const dataOff=ico.readUInt32LE(off+12);
    const png=ico.subarray(dataOff,dataOff+len);
    assert.deepEqual(pngSize(png),{width:size,height:size,depth:8,colorType:6});
    sizes.push(size);
  }
  for(const size of [16,32,48,128,256]) assert.ok(sizes.includes(size));
});
test('macOS ICNS has valid chunk boundaries and 512/1024 high resolution icons',()=>{
  const icns=read('properties/macOS/genycaster.icns');
  assert.equal(icns.subarray(0,4).toString(),'icns');
  assert.equal(icns.readUInt32BE(4),icns.length);
  const chunks={};
  let offset=8;
  while(offset<icns.length){
    const kind=icns.subarray(offset,offset+4).toString();
    const len=icns.readUInt32BE(offset+4);
    assert.ok(len>8&&offset+len<=icns.length);
    chunks[kind]=pngSize(icns.subarray(offset+8,offset+len));
    offset+=len;
  }
  assert.equal(offset,icns.length);
  assert.equal(chunks.ic09.width,512);
  assert.equal(chunks.ic10.width,1024);
});
test('iOS App Store source is opaque 1024px asset-catalog icon',()=>{
  const icon=pngSize(read('properties/iOS/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png'));
  assert.deepEqual(icon,{width:1024,height:1024,depth:8,colorType:2});
  const contents=JSON.parse(text('properties/iOS/Assets.xcassets/AppIcon.appiconset/Contents.json'));
  assert.equal(contents.images[0].filename,'AppIcon-1024.png');
  assert.equal(contents.images[0].idiom,'universal');
});
test('Android supplies actual legacy icons, adaptive foreground and monochrome',()=>{
  const base='properties/Android/';
  const manifest=text(base+'AndroidManifest.xml');
  assert.match(manifest,/-- %%INSERT_APP_LIB_NAME%% --/);
  assert.match(manifest,/android:icon="@mipmap\/ic_launcher"/);
  assert.match(manifest,/android:roundIcon="@mipmap\/ic_launcher_round"/);
  assert.match(manifest,/org\.qtproject\.qt\.android\.bindings\.QtActivity/);
  for(const [folder,size] of Object.entries({mdpi:48,hdpi:72,xhdpi:96,xxhdpi:144,xxxhdpi:192})){
    for(const name of ['ic_launcher','ic_launcher_round']){
      const icon=pngSize(read(base+'res/mipmap-'+folder+'/'+name+'.png'));
      assert.equal(icon.width,size);
      assert.equal(icon.height,size);
    }
  }
  for(const name of ['ic_launcher','ic_launcher_round']){
    const adaptive=text(base+'res/mipmap-anydpi-v26/'+name+'.xml');
    assert.match(adaptive,/<adaptive-icon/);
    assert.match(adaptive,/@drawable\/ic_launcher_foreground/);
    assert.match(adaptive,/@drawable\/ic_launcher_monochrome/);
  }
  assert.match(text(base+'res/drawable/ic_launcher_foreground.xml'),/android:pathData=/);
});
test('Linux exposes correct freedesktop launcher and hicolor icon',()=>{
  const png=pngSize(read('properties/Linux/genycaster.png'));
  assert.equal(png.width,512);
  const desktop=text('properties/Linux/genycaster.desktop');
  assert.match(desktop,/^Exec=Genycaster$/m);
  assert.match(desktop,/^Icon=genycaster$/m);
  assert.match(desktop,/^Terminal=false$/m);
});

test('platform properties own all package artifacts without forking the shared Qt UI',()=>{
  const cmake=text('ui/CMakeLists.txt')+text('properties/ConfigureTarget.cmake')+
    ['Android','iOS','macOS','Windows','Linux'].map(platform=>
      text('properties/'+platform+'/Configure.cmake')).join('\n');
  const build=text('cmake/generate_app_icons.py');
  const manifest=JSON.parse(text('properties/WebAssembly/genycaster.webmanifest'));
  const readme=text('properties/README.md');
  for(const dir of ['Android','iOS','macOS','Windows','Linux','WebAssembly']){
    assert.ok(existsSync(resolve(root,'properties',dir)), 'properties/'+dir);
    assert.match(readme,new RegExp('\\['+dir+'\\]'));
    assert.ok(cmake.includes('properties/'+dir),dir+' configured from root');
  }
  assert.match(cmake,/configure_file\("\$\{CMAKE_SOURCE_DIR\}\/properties\/Windows\/Genycaster\.rc\.in"/);
  assert.match(cmake,/QT_ANDROID_PACKAGE_SOURCE_DIR "\$\{CMAKE_SOURCE_DIR\}\/properties\/Android"/);
  assert.match(cmake,/properties\/WebAssembly\/genycaster\.webmanifest/);
  assert.match(build,/DEST = ROOT \/ "properties"/);
  assert.match(build,/"Android\/res\//);
  assert.equal(manifest.start_url,'./genycaster.html');
  assert.equal(manifest.display,'standalone');
  const resource=text('properties/Windows/Genycaster.rc.in');
  assert.match(resource,/OriginalFilename", "Genycaster\.exe/);
  assert.match(resource,/GENYCASTER_WINDOWS_ICON_PATH/);
  assert.ok(!existsSync(resolve(root,'ui/platform')),'removed old platform property tree');
});
