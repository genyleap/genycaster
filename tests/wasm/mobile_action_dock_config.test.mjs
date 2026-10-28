import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=relative=>readFileSync(relative,'utf8');
const dock=read('ui/components/BottomNavigation.qml');
const theme=read('ui/theme/Theme.qml');
const settings=read('ui/components/LocaleSettingsPopup.qml');
const shell=read('ui/components/AppShell.qml');

test('five destinations stay in a capsule and the sixth control is independent',()=>{
  const capsule=dock.slice(dock.indexOf('objectName: "mobileNavigationCapsule"'));
  const action=capsule.slice(capsule.indexOf('objectName: "mobilePrimaryActionButton"'));
  const destinations=['mobileDockHome','mobileDockExplore','mobileDockApps',
    'mobileBottomBookmarksButton','mobileDockProfile'];
  for (const name of destinations)
    assert.match(capsule,new RegExp('objectName: "'+name+'"'));
  assert.equal((capsule.match(/DockDestination \{/g)||[]).length,5);
  assert.ok(capsule.indexOf('objectName: "mobilePrimaryActionButton"')
    >capsule.indexOf('objectName: "mobileDockProfile"'));
  assert.match(dock,/showSelectedLabel: width >= 375/);
  assert.match(dock,/Layout\.minimumWidth: 39/);
  assert.match(dock,/Layout\.preferredWidth: 56/);
  assert.match(dock,/layoutDirection: Theme\.uiDirection/);
  assert.match(action,/Accessible\.name: text/);
  assert.match(action,/Theme\.contrastInk\(Theme\.mobileActionColor\)/);
  assert.match(action,/enabled: !Theme\.reduceMotion/);
});

test('dock dispatches only implemented actions through AppShell',()=>{
  assert.match(dock,/case "photo": root\.photoRequested\(\); break/);
  assert.match(dock,/case "search": root\.searchRequested\(\); break/);
  assert.match(dock,/case "settings": root\.settingsRequested\(\); break/);
  assert.match(dock,/default: root\.createRequested\(\); break/);
  assert.match(shell,/onCreateRequested: root\.openComposer\(null\)/);
  assert.match(shell,/onPhotoRequested: root\.openComposer\(null, "media"\)/);
  assert.match(shell,/onSearchRequested: searchPalette\.open\(\)/);
  assert.match(shell,/onSettingsRequested: root\.openLocaleSettings\(\)/);
  assert.match(shell,/onProfileRequested: \{[\s\S]*?presentationBridge\.isAuthenticated/);
});

test('persisted action choice is allowlisted and settings can be changed from Appearance',()=>{
  assert.match(theme,/property Settings mobileActionPreferences: Settings \{/);
  assert.match(theme,/category: "MobileActionButton"/);
  assert.match(theme,/property string action: "compose"/);
  assert.match(theme,/property string colorPreset: "blue"/);
  for(const value of ['compose','photo','search','settings'])
    assert.match(theme,new RegExp('"'+value+'"'));
  for(const value of ['blue','coral','theme'])
    assert.match(theme,new RegExp('"'+value+'"'));
  assert.match(theme,/function setMobileAction\(action\) \{/);
  assert.match(theme,/function setMobileActionColor\(preset\) \{/);
  assert.match(settings,/objectName: "mobileActionButtonPreferences"/);
  assert.match(settings,/Theme\.setMobileAction\(modelData\.id\)/);
  assert.match(settings,/Theme\.setMobileActionColor\(modelData\.id\)/);
  assert.match(settings,/Theme\.mobileAction === modelData\.id/);
  assert.match(settings,/Theme\.mobileActionColorPreset === modelData\.id/);
});

test('settings and accessibility labels are localized in every catalog',()=>{
  const labels=['mobile_action_title','mobile_action_description',
    'mobile_action_function','mobile_action_search','mobile_action_color',
    'mobile_action_blue','mobile_action_coral','mobile_action_theme'];
  const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN',
    'ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const locale of locales){
    const catalog=read('ui/i18n/qml_'+locale+'.ts');
    for(const label of labels) {
      const id='genycaster.settings.'+label;
      assert.equal(catalog.split('id="'+id+'"').length-1,1,locale+': '+id);
    }
  }
  const fa=read('ui/i18n/qml_fa_IR.ts');
  assert.match(fa,/<translation>دکمهٔ اکشن موبایل<\/translation>/);
  assert.match(fa,/<translation>قرمز<\/translation>/);
});
