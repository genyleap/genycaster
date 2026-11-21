import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const source=readFileSync(new URL('../../ui/wasm/browser_account_create_dialog.mjs',import.meta.url),'utf8');
const start=source.indexOf('const translations=')+'const translations='.length;
const end=source.indexOf('\n};\nconst titleFor=',start);
if(start<18||end<0)throw Error('wallet dialog translations object missing');
const literals=source.slice(start,end+2);
const translations=runInNewContext('('+literals+')');

test('browser custody wallet has complete labels for every supported language',()=>{
  const locales=['en','fa','ar','de','es','fr','hi','ja','ko','pt','ru','zh'];
  assert.deepEqual(Object.keys(translations).sort(),locales.sort());
  const keys=Object.keys(translations.en).sort();
  assert.equal(keys.length,36);
  for(const locale of locales){
    const localized=translations[locale];
    assert.deepEqual(Object.keys(localized).sort(),keys,locale+' missing fields');
    for(const key of keys){
      assert.equal(typeof localized[key],'string',locale+': '+key);
      assert.ok(localized[key].trim(),locale+': '+key);
      // Some genuine translations are identical cognates (e.g. French Confirmation).
      if(locale!=='en' && key!=='blockchainCaption'
         && !(locale==='fr' && key==='stepThree'))
        assert.notEqual(localized[key],translations.en[key],locale+' untranslated '+key);
    }
  }
});

test('wallet dialog uses the selected language and preserves right-to-left layout',()=>{
  assert.match(source,/browser.__genycasterPreferredLanguage/);
  assert.match(source,/translations\[language\]\?language:'en'/);
  assert.match(source,/\['fa','ar'\].includes\(lang\)/);
});
