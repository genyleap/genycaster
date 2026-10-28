import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=path=>readFileSync(path,'utf8');
const sidebar=read('ui/components/SideNavigation.qml');
const pulse=read('ui/components/GenyPulsePanel.qml');
const bookmarks=read('ui/components/BookmarksSurface.qml');
const chooser=read('ui/components/BookmarkCollectionChooser.qml');
const format=read('ui/components/LocaleFormat.qml');

test('sidebar brand stays visually unified without inherited RTL mirror',()=>{
  const brand=sidebar.slice(sidebar.indexOf('id: navigationBrand'),sidebar.indexOf('SystemStatusCard {'));
  assert.match(brand,/objectName: "sidebarBrandCluster"/);
  assert.match(brand,/LayoutMirroring.enabled: false/);
  assert.match(brand,/LayoutMirroring.childrenInherit: false/);
  assert.match(brand,/navigationBrandSymbol/);
  assert.match(brand,/navigationBrandText/);
  assert.match(brand,/x: Theme.rtl \? navigationBrand.width - width : 0/);
  assert.match(brand,/x: Theme.rtl \? 0 : navigationBrandSymbol.width \+ navigationBrand.brandGap/);
  assert.match(brand,/horizontalAlignment: Theme.rtl \? Image.AlignRight : Image.AlignLeft/);
  assert.match(brand,/font.family: Theme.fontFamilyForText\(text\)/);
  assert.doesNotMatch(brand,/layoutDirection: Theme.uiDirection/);
});

test('all new bookmark UI labels come from translated catalog IDs',()=>{
  const ids=[
    'new_collection','refresh_collections','rename','delete','delete_collection',
    'collection_name','delete_confirmation','confirm_delete','save','cancel',
    'add_to_collection','remove_from_collection','chooser_title','chooser_description',
    'uncategorized','create'
  ];
  for(const key of ids) {
    const id='genycaster.bookmarks.'+key;
    assert.ok((bookmarks+chooser).includes('qsTrId("'+id+'")'),id);
    for(const lang of ['fa_IR','en_US','ar_SA']) {
      const catalog=read('ui/i18n/qml_'+lang+'.ts');
      assert.ok(catalog.includes('id="'+id+'"'),lang+':'+id);
    }
  }
  assert.doesNotMatch(bookmarks,/text: "(?:Delete|Rename|New collection|Cancel|Save)"/);
  assert.doesNotMatch(chooser,/text: "(?:Save to collection|Create|Uncategorized)"/);
  const persian=read('ui/i18n/qml_fa_IR.ts');
  assert.match(persian,/<translation>حذف از مجموعه<\/translation>/);
  assert.match(persian,/<translation>مجموعهٔ جدید<\/translation>/);
});

test('localized pulse labels and all numeric statistics react to language changes',()=>{
  for(const id of [
    'genycaster.activity.casts','genycaster.activity.onchain',
    'genycaster.activity.range_year','genycaster.activity.range_month',
    'genycaster.activity.range_three_months'
  ]) {
    assert.ok(pulse.includes('qsTrId("'+id+'")'),id);
    for (const lang of ['fa_IR','en_US'])
      assert.ok(read('ui/i18n/qml_'+lang+'.ts').includes('id="'+id+'"'));
  }
  assert.match(pulse,/LocaleFormat.formatInteger\(root.githubYear\)/);
  assert.match(pulse,/LocaleFormat.formatInteger\(root.githubVisibleTotal\)/);
  assert.match(pulse,/LocaleFormat.formatInteger\(Number\(root.farcasterData.count/);
  assert.match(pulse,/LocaleFormat.localizeDigits\(root.selectedDate\)/);
  assert.match(pulse,/LocaleFormat.localizeDigits\(root.selectedGithubDate\)/);
  assert.match(bookmarks,/LocaleFormat.formatInteger\(root.pendingCount\)/);
});

test('presentation-only digit translation keeps canonical IDs unchanged',()=>{
  const match=format.match(/function localizeDigits\(value\) \{([\s\S]*?)\n    \}/);
  assert.ok(match);
  const code='(function(value){'+match[1]+'\n})';
  const context={root:{observeRevision(){}},
    localizationController:{effectiveLanguageTag:'fa'}};
  const fn=vm.runInNewContext(code,context);
  assert.equal(fn('2026-10-28 / 1625'),'۲۰۲۶-۱۰-۲۸ / ۱۶۲۵');
  context.localizationController.effectiveLanguageTag='en';
  assert.equal(fn('2026-10-28'),'2026-10-28');
  context.localizationController.effectiveLanguageTag='ar';
  assert.equal(fn('2026-10-28'),'٢٠٢٦-١٠-٢٨');
  assert.match(format,/localizationController.formatInteger\(value\)/);
});
