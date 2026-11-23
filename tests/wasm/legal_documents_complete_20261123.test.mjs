import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml=readFileSync('ui/components/setup/InitialSetup.qml','utf8');
const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
               'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];

test('Setup renders full official legal text inside the app and offers real published links',()=>{
    const legal=qml.slice(qml.indexOf('id: legalReader'),qml.indexOf('id: setupCanvas'));
    assert.match(legal,/id: legalBodyScroll/);
    assert.match(legal,/contentHeight: legalDocumentContent\.implicitHeight/);
    assert.match(legal,/id: legalBody/);
    assert.match(legal,/qsTrId\("genycaster\.setup\.terms_body"\)/);
    assert.match(legal,/qsTrId\("genycaster\.setup\.privacy_body"\)/);
    assert.match(legal,/objectName: "setupOpenPublishedLegalDocument"/);
    assert.match(legal,/https:\/\/genyleap\.com\/terms-of-use/);
    assert.match(legal,/https:\/\/genyleap\.com\/privacy/);
    const cards=qml.slice(qml.indexOf('id: termsStage'),qml.indexOf('id: accountStage'));
    assert.match(cards,/root\.openLegalDocument\("terms"\)/);
    assert.match(cards,/root\.openLegalDocument\("privacy"\)/);
});

test('Terms and Privacy translations contain real policy sections across 12 locales',()=>{
    for(const locale of locales){
        const ts=readFileSync('ui/i18n/qml_'+locale+'.ts','utf8');
        for(const key of ['terms_body','privacy_body']){
            const match=ts.match(new RegExp('<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>'));
            assert.ok(match,locale+' missing '+key);
            const text=match[1].match(/<translation>([\s\S]*?)<\/translation>/)?.[1]||'';
            assert.ok(text.length>180,locale+' policy body is still a placeholder: '+key);
            assert.ok(text.includes('\n\n'),locale+' policy needs readable paragraphs: '+key);
            assert.doesNotMatch(text,/\bread below\b/i,locale+' has a missing-content placeholder');
        }
    }
    const en=readFileSync('ui/i18n/qml_en_US.ts','utf8');
    assert.match(en,/Website content[\s\S]*Downloads and repositories[\s\S]*Project enquiries/);
    assert.match(en,/What we collect[\s\S]*Why we use it[\s\S]*Genycaster-specific notice/);
});
