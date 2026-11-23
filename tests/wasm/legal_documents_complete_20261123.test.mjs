import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = file => readFileSync(file,'utf8');
const setup = source('ui/components/setup/InitialSetup.qml');
const catalog = source('ui/i18n/catalog.cpp');
const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
               'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
const keys=['terms_title','terms_body','privacy_body','terms_updated',
  'privacy_updated','design_terms_title','design_terms_description',
  'legal_intro','single_legal_consent'];

test('Genycaster legal reader is entirely in-app with real Markdown and no external links',()=>{
  const legal=setup.slice(setup.indexOf('id: legalReader'),setup.indexOf('id: setupCanvas'));
  assert.match(legal,/id: legalBodyScroll/);
  assert.match(legal,/contentHeight: legalDocumentContent\.implicitHeight/);
  assert.match(legal,/ReadableMarkdown \{/);
  const documentRenderer=source('ui/components/content/ReadableMarkdown.qml');
  assert.match(documentRenderer,/textFormat: Text\.MarkdownText/);
  assert.match(documentRenderer,/onLinkActivated:/);
  assert.match(documentRenderer,/https:/);
  assert.match(legal,/genycaster\.setup\.terms_body/);
  assert.match(legal,/genycaster\.setup\.privacy_body/);
  assert.match(legal,/genycaster\.setup\.legal_intro/);
  assert.doesNotMatch(legal,/Qt\.openUrlExternally|setupOpenPublishedLegalDocument|https?:\/\//);
  const cards=setup.slice(setup.indexOf('id: termsStage'),setup.indexOf('id: accountStage'));
  assert.match(cards,/root\.openLegalDocument\("terms"\)/);
  assert.match(cards,/root\.openLegalDocument\("privacy"\)/);
  assert.match(cards,/onClicked: root\.consentChecked = !root\.consentChecked/);
});

test('all twelve locales have translated, non-placeholder policies and consent',()=>{
  for(const locale of locales){
    const ts=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of keys){
      const match=ts.match(new RegExp('<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>'));
      assert.ok(match,locale+' missing '+key);
      const value=match[1].match(/<translation>([\s\S]*?)<\/translation>/)?.[1]||'';
      assert.ok(value.trim().length>0,locale+' has an empty translation: '+key);
      assert.ok(value !== 'genycaster.setup.'+key,locale+' prints translation ID: '+key);
      assert.match(catalog,new RegExp('qtTrId\\("genycaster\\.setup\\.'+key+'"\\)'));
    }
    for(const [key,sections] of [['terms_body',7],['privacy_body',9]]){
      const part=ts.match(new RegExp('<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>'))[1];
      const value=part.match(/<translation>([\s\S]*?)<\/translation>/)[1];
      assert.ok(value.length>750,locale+' '+key+' too short');
      assert.equal((value.match(/^### /gm)||[]).length,sections,locale+' '+key+' lacks sections');
      assert.match(value,/\*\*[\s\S]+?\*\*/);
      assert.doesNotMatch(value,/https?:\/\/|read below|website preview/i);
    }
  }
});

test('policy contents explain actual app custody, QStorage and public-data boundaries',()=>{
  const en=source('ui/i18n/qml_en_US.ts');
  assert.match(en,/Signing in is not permission to publish/);
  assert.match(en,/Deleting a post in Genycaster cannot guarantee deletion/);
  assert.match(en,/QStorage/);
  assert.match(en,/phrase itself is not uploaded/);
  assert.match(en,/independent services/);
  assert.match(en,/Your choices and limits/);
});
