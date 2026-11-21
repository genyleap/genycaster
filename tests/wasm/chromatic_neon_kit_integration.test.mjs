import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,statSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Qt 6.12 portable chromatic shaders and source material are registered together',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const paths=JSON.parse(read('ui/components/source_paths.json'));
  for(const part of ['NeonReflection','NeonReflectionSource']){
    assert.equal(paths[part+'.qml'],'visual/'+part+'.qml');
    assert.match(cmake,new RegExp('components/visual/'+part+'\\.qml'));
  }
  for(const shader of ['gaussian','neon','reflection','reflection-gaussian']){
    assert.match(cmake,new RegExp('shaders/'+shader+'\\.frag\\.qsb'));
    assert.ok(statSync(new URL('../../ui/shaders/'+shader+'.frag.qsb',import.meta.url)).size>2000);
  }
  assert.match(cmake,/shaders\/transparent\.png/);
  assert.ok(statSync(new URL('../../ui/shaders/transparent.png',import.meta.url)).size>=60);
});
test('low-resolution scene captures are shared, bounded, nonrecursive and outside the reading subtree',()=>{
  const source=read('ui/components/visual/NeonReflectionSource.qml');
  const shell=read('ui/components/shell/AppShell.qml');
  assert.match(source,/sourceIsSafe:[\s\S]*ancestor === sourceItem/);
  assert.match(source,/ShaderEffectSource\.RGBA16F/g);
  assert.match(source,/root\.browserRenderer[\s\S]*?Math\.min\(0\.0625, 256 \//);
  assert.match(source,/Math\.min\(0\.125, 512 \//);
  assert.match(source,/recursive: false/);
  assert.match(source,/hideSource: false/);
  assert.match(source,/live: root\.capturing/);
  assert.match(source,/root\.effectiveBlur \//);
  assert.match(shell,/id: workspaceReflection[\s\S]*sourceItem: readingBackdrop/);
  assert.ok(shell.indexOf('id: workspaceReflection')<shell.indexOf('id: readingBackdrop'));
  assert.doesNotMatch(shell,/sourceItem: shellContent/);
  assert.equal((shell.match(/NeonReflectionSource\s*\{/g)||[]).length,1,'one shared capture in shell');
  for(const component of ['SideNavigation','CompactNavigationRail','AppHeader','ContextRail','SocialSidebar'])
    assert.match(shell,new RegExp(component+' \\{[\\s\\S]{0,110}reflectionSource: workspaceReflection'));
});
test('edge shaders keep Lame corner geometry and sample adjacent color, not a static accent',()=>{
  const halo=read('ui/components/visual/NeonHalo.qml');
  const material=read('ui/components/visual/NeonSurface.qml');
  const reflecting=read('ui/components/visual/NeonReflection.qml');
  const chroma=read('ui/shaders/reflection.frag');
  const rim=read('ui/shaders/neon.frag');
  assert.match(rim,/vec4 cornerFlags;/);
  assert.match(rim,/surfaceParameters\.z,[\s\S]*surfaceParameters\.w, cornerFlags/);
  assert.match(rim,/sampler2D environmentSource/);
  assert.match(rim,/sourceFacingWeight/);
  assert.match(rim,/reflectedEdgeMaterial/);
  assert.match(chroma,/sampler2D source;/);
  assert.match(chroma,/sourceFacingWeight/);
  assert.match(chroma,/distanceFade/);
  assert.match(chroma,/edgeMode/);
  assert.match(halo,/property variant environmentSource:/);
  assert.match(halo,/property vector4d environmentParameters:/);
  assert.match(material,/sourceComponent: NeonReflection \{/);
  assert.match(material,/sourceComponent: NeonHalo \{/);
  assert.match(reflecting,/property variant source: sourceIsSafe \?/);
  assert.match(reflecting,/property bool bottomEdgeOnly: false/);
});
test('headers, nav rows and inspector surfaces consume shared material without their own captures',()=>{
  const header=read('ui/components/shell/AppHeader.qml');
  const sidebar=read('ui/components/navigation/SideNavigation.qml');
  const compact=read('ui/components/navigation/CompactNavigationRail.qml');
  const nav=read('ui/components/navigation/NavItem.qml');
  const rail=read('ui/components/navigation/ContextRail.qml');
  const social=read('ui/components/navigation/SocialSidebar.qml');
  const buttons=read('ui/components/controls/ActionButton.qml');
  for(const c of [header,sidebar,compact,nav,rail,social,buttons])
    assert.match(c,/property NeonReflectionSource reflectionSource: null/);
  for(const c of [header,sidebar,compact,nav,rail,social,buttons])
    assert.doesNotMatch(c,/NeonReflectionSource\s*\{/);
  assert.match(header,/bottomEdgeOnly: false/);
  assert.match(header,/edgeStrength: 0/);
  assert.match(read('ui/components/navigation/BottomNavigation.qml'),/objectName: "mobileNavigationChromaticReflection"/);
  assert.match(read('ui/components/navigation/BottomNavigation.qml'),/reflectionSource: root\.reflectionSource/);
  assert.match(sidebar,/reflectionSource: root\.reflectionSource/);
  assert.match(nav,/reflectionSource: control\.reflectionSource/);
  assert.match(rail,/component WidgetCard: NeonCard[\s\S]*reflectionSource: root\.reflectionSource/);
  assert.match(social,/NeonReflection \{/);
  assert.match(buttons,/background: NeonSurface \{[\s\S]*reflectionSource: control\.reflectionSource/);
});
test('reflection controls are persisted, bounded, localized and respect battery/reduced-transparency preferences',()=>{
  const settings=read('ui/components/settings/SettingsPopup.qml');
  const theme=read('ui/theme/Theme.qml');
  const keys=['chromatic_reflections','chromatic_reflections_description','reflection_intensity','reduce_reflection_transparency'];
  assert.match(theme,/category: "ChromaticReflections"/);
  assert.match(theme,/readonly property real reflectionGain:[\s\S]*!powerSave/);
  assert.match(theme,/function setReflectionIntensity\(intensity\)/);
  assert.match(settings,/objectName: "settingsChromaticReflections"/);
  assert.match(settings,/objectName: "chromaticReflectionIntensity"/);
  assert.match(settings,/onMoved: Theme\.setReflectionIntensity\(value \/ 100\)/);
  assert.match(settings,/Theme\.setReduceTransparency\(checked\)/);
  for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
    const xml=read('ui/i18n/qml_'+locale+'.ts');
    for(const key of keys){
      const match=xml.match(new RegExp('<message id="genycaster\\.settings\\.'+key+'">([\\s\\S]*?)</message>'));
      assert.ok(match,locale+':'+key);
      assert.match(match[1],/<translation>[^<]+<\/translation>/);
    }
  }
});
