import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');
const drawer = read('ui/components/profile/GenyProDetailsDrawer.qml');
const card = read('ui/components/profile/ProfileTrustCard.qml');

test('unknown Pro price remains nonpayable and can be refreshed', () => {
    assert.match(drawer, /readonly property bool priceKnown: monthlyUsdCents > 0/);
    assert.match(drawer, /readonly property bool priceUnavailable: !priceKnown && !priceLoading/);
    assert.match(drawer, /readonly property bool paymentOptionsReady: priceKnown[\s\S]*?plan\.payment_available === true && cloudReady && membershipVerified/);
    assert.match(drawer, /enabled: root\.paymentOptionsReady/);
    assert.match(drawer, /visible: root\.paymentOptionsReady/);
    assert.match(drawer, /if \(monthlyUsdCents <= 0\) return qsTrId\("genycaster\.pro\.refresh_price"\)/);
    assert.match(drawer, /openProofConnections\.fetchProPlan\(true\)/);
    assert.match(drawer, /if \(!root\.canRequestQuote \|\| root\.busy[\s\S]*?walletAvailable && \(!root\.walletConnected \|\| !root\.walletVerified\)/);
    assert.match(drawer, /root\.selectedAsset = modelData/);
    assert.doesNotMatch(drawer, /pay\(\)\s*;\s*\/\/.*price_unavailable/);
});

test('Pro pricing stays readable in a narrow drawer with localized text', () => {
    const i = drawer.indexOf('objectName: "genyProCompactPriceHeadline"');
    assert.ok(i >= 0);
    const title = drawer.slice(i, i+1550);
    assert.match(title, /Layout\.fillWidth: true/);
    assert.match(title, /Layout\.minimumWidth: 0/);
    assert.match(title, /font\.pixelSize: root\.priceKnown && !root\.checkoutActive/);
    assert.match(title, /wrapMode: Text\.WordWrap/);
    assert.match(title, /maximumLineCount: 2/);
    assert.match(title, /elide: Text\.ElideRight/);
    assert.match(drawer, /objectName: "genyProPriceUnavailableHint"/);
    assert.match(drawer, /visible: !root\.eligible && root\.priceUnavailable/);
    assert.match(drawer, /qsTrId\("genycaster\.pro\.price_retry_hint"\)/);
    assert.doesNotMatch(drawer, /Theme\.typeTitle1Size/);
});

test('Streak metric has mirrored inset without modifying statistics or number', () => {
    const i = card.indexOf('objectName: "profileStreakValue"');
    assert.ok(i >= 0);
    const metric = card.slice(i, i+750);
    assert.match(metric, /Layout\.rightMargin: Theme\.rtl \? 0 : Theme\.space3/);
    assert.match(metric, /Layout\.leftMargin: Theme\.rtl \? Theme\.space3 : 0/);
    assert.match(metric, /LocaleFormat\.formatInteger\(root\.streakCurrent\)/);
});

test('Pro unavailable text is concise, translated, and placeholder-free', () => {
    const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    for (const locale of locales) {
        const ts = read('ui/i18n/qml_'+locale+'.ts');
        for (const key of ['price_unavailable','price_retry_hint']) {
            const needle = 'id="genycaster.pro.'+key+'"';
            assert.equal(ts.split(needle).length-1, 1, locale+' duplicate/missing '+key);
            const m = ts.match(new RegExp('<message id="genycaster\\.pro\\.'+key+'">\\s*<source>([^<]+)<\\/source>\\s*<translation>([^<]+)<\\/translation>'));
            assert.ok(m, locale+' missing translation '+key);
            assert.ok(m[2].trim(), locale+' empty translation '+key);
            assert.doesNotMatch(m[1]+m[2], /%[1-9][0-9]*/);
        }
        assert.match(ts, /<source>Pro price unavailable<\/source>/);
    }
});
