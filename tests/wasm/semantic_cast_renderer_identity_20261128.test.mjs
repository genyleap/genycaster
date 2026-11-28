import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const source=read('ui/components/feed/SemanticFeedItem.qml');

test('classification changes retain a single cast renderer',()=>{
    assert.equal((source.match(/\bCastItem\s*\{/g)||[]).length,1);
    assert.doesNotMatch(source,/sourceComponent: root\.magazineCard|\b(?:ConversationRow|VisualPost|DeveloperPost)\s*\{/);
    assert.match(source,/active: root\.developerChrome/);
    assert.match(source,/modelData: root\.modelData/);
    assert.match(source,/onReactionRequested: \(kind, data, active\) => root\.reactionRequested/);
});

test('semantic chrome preserves the former geometry and compact policy',()=>{
    const expression = name => {
        const match=source.match(new RegExp('readonly property (?:real|bool|string) '+name+': ([\\s\\S]*?)(?=\\n    (?:readonly property|//|implicitHeight:))'));
        assert.ok(match,name);
        return new Function('root','Theme','builderHeader','return ('+match[1].trim()+')');
    };
    const Theme={space1:4,space2:8,space3:12,space5:20};
    const header={height:18};
    for(const magazineCard of [false,true]) for(const compact of [false,true])
        for(const presentationKind of ['conversation','visual','developer','unknown']) {
            const root={magazineCard,compact,presentationKind};
            for(const name of ['resolvedKind','visualChrome','developerChrome','topInset','totalInset'])
                root[name]=expression(name)(root,Theme,header);
            const expectedTop=magazineCard?0:presentationKind==='developer'?22:presentationKind==='visual'?8:0;
            const expectedTotal=magazineCard?0:presentationKind==='developer'?34:presentationKind==='visual'?(compact?8:20):0;
            assert.equal(root.topInset,expectedTop);
            assert.equal(root.totalInset,expectedTotal);
        }
    assert.match(source,/compact: root\.magazineCard \|\| root\.resolvedKind === "conversation" \|\| root\.compact/);
    assert.match(source,/height: root\.magazineCard \? Math\.max\(implicitHeight, magazinePaintHeight\) : implicitHeight/);
});
