import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path, import.meta.url),'utf8');

test('plain cast tokenizer preserves handles and distinct #/$ styles without false price tokens',()=>{
    const source=read('ui/components/RichCastRenderer.qml');
    const body=source.match(/function inlineSpansFor\(value\) \{([\s\S]*?)\n    \}\n\n    \/\/ Protocol Markdown/);
    assert.ok(body);
    const tokenize=new Function('Theme','value',body[1]);
    const spans=tokenize({accent:'#9a98e3'},'Hi @compez.eth $GENY #Farcaster 0.01 USD and http://example.com/a');
    assert.ok(spans.some(v=>v.kind==='mention' && v.username==='compez.eth'));
    assert.equal(tokenize({accent:'#9a98e3'},'hi @fid:12345')[1].username,'fid:12345');
    assert.ok(spans.some(v=>v.text==='$GENY' && v.color==='#9a98e3'));
    assert.ok(spans.some(v=>v.text==='#Farcaster' && v.color==='#9a98e3'));
    assert.ok(spans.some(v=>v.kind==='link' && v.target==='http://example.com/a'));
    const email=tokenize({accent:'#aaa'},'support@site.com $5 #42 bar$NO');
    assert.equal(email.filter(v=>v.kind==='mention').length,0);
    assert.equal(email.filter(v=>v.color).length,0);
});

test('mentions are true internal profile links in both Qt text rendering modes and every cast route',()=>{
    const cpp=read('src/presentation/qt/rich_inline_text_item.cpp');
    const inline=read('ui/components/RichInlineText.qml');
    const shell=read('ui/components/AppShell.qml');
    assert.match(cpp,/base.setAnchor\(true\)/);
    assert.match(cpp,/base.setAnchorHref\(QStringLiteral\("@"\) \+ username\)/);
    assert.match(inline,/const handle = "@" \+ root.escapedHtml\(span.username\)/);
    assert.match(inline,/value = "<a href=/);
    assert.match(shell,/function openCastTextLink\(url\)/);
    assert.match(shell,/presentationBridge.openProfileUsername\(target.substring\(1\)\)/);
    assert.match(shell,/presentationBridge.openProfileFid\(Number\(target.substring\(5\)\)\)/);
    assert.equal((shell.match(/onExternalRequested: url => root.openCastTextLink\(url\)/g)||[]).length,5);
    assert.match(read('ui/components/RichText.qml'),/token.startsWith\("@"\)/);
});

test('miniapp loading tiles do not pretend a successful image failed while request is pending',()=>{
    const placeholder=read('ui/components/MiniAppArtPlaceholder.qml');
    const apps=read('ui/components/AppsSurface.qml');
    const library=read('ui/components/MiniAppLibraryShelves.qml');
    assert.match(placeholder,/opacity: root.imageReady \? 0 : 1/);
    assert.match(placeholder,/visible: !root.hasSource \|\| root.loadFailed/);
    assert.match(placeholder,/Theme.reduceMotion/);
    assert.equal((library.match(/MiniAppArtPlaceholder \{/g)||[]).length,2);
    assert.match(apps,/id: storeIcon[\s\S]*?MiniAppArtPlaceholder/);
    assert.match(apps,/model: root.compact \? 6 : parent.columns/);
    assert.match(apps,/Layout.preferredHeight: root.compact \? 116 : 190/);
});

test('shared nonblocking refresh indicator is consistent across reading and discovery surfaces',()=>{
    const indicator=read('ui/components/ContentRefreshIndicator.qml');
    assert.match(indicator,/visible: active/);
    assert.match(indicator,/Theme.reduceMotion/);
    for(const view of ['FeedSurface','ProfileSurface','DetailSurface','AppsSurface'])
        assert.match(read('ui/components/'+view+'.qml'),/ContentRefreshIndicator \{/);
    const cmake=read('ui/CMakeLists.txt');
    assert.match(cmake,/components\/ContentRefreshIndicator.qml/);
    assert.match(cmake,/components\/MiniAppArtPlaceholder.qml/);
});

test('unhydrated canonical Farcaster mentions remain visible without a provider-specific username',()=>{
    const codec=read('src/providers/hypersnap/codec.cpp');
    const contract=read('tests/providers/hypersnap/contract_tests.cpp');
    assert.match(codec,/findValue\(castObject, "mentions"\)/);
    assert.match(codec,/original\[pos\]/);
    assert.match(contract,/@fid:12345/);
});
