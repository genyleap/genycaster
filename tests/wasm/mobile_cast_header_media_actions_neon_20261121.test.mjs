import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('cast header caps long names and aligns compact overflow to author line', () => {
    const cast=read('ui/components/feed/CastItem.qml');
    const name=cast.slice(cast.indexOf('id: castAuthorName'),cast.indexOf('id: castInlineChannel'));
    const menu=cast.slice(cast.indexOf('id: moreButton'),cast.indexOf('visible: root.hasWriteState'));
    assert.match(name,/readonly property real nameWidthCap:/);
    assert.match(name,/root\.compact[\s\S]*?root\.width \* \(root\.castChannel\.length > 0 \? 0\.34 : 0\.44\)/);
    assert.match(name,/Layout\.minimumWidth: 0/);
    assert.match(name,/Layout\.maximumWidth: nameWidthCap/);
    assert.match(name,/wrapMode: Text\.NoWrap/);
    assert.match(name,/elide: Text\.ElideRight/);
    assert.match(cast,/objectName: "compactCastAuthorMetadata"[\s\S]*?Layout\.alignment: Qt\.AlignVCenter/);
    assert.match(cast,/id: castTrailingMetadata[\s\S]*?objectName: "compactCastAuthorMetadata"/);
    assert.match(cast,/id: castTrailingMetadata[\s\S]*?objectName: "magazineCastTime"[\s\S]*?id: moreButton/);
    assert.match(menu,/Layout\.alignment: Qt\.AlignVCenter/);
    assert.doesNotMatch(menu,/Layout\.topMargin:/);
    assert.match(cast,/objectName: "magazineCastTime"/);
});

test('compact reaction bar is one swipeable baseline, without reducing hit targets', () => {
    const bar=read('ui/components/feed/EngagementBar.qml');
    const button=read('ui/components/feed/EngagementButton.qml');
    assert.match(bar,/readonly property bool stackedActions: width < reactionRowWidth[\s\S]*?&& !root\.compact/);
    assert.match(bar,/id: actionViewport[\s\S]*?flickableDirection: Flickable\.HorizontalFlick/);
    assert.match(bar,/contentWidth: root\.compact[\s\S]*?root\.reactionRowWidth \+ utilityActions\.implicitWidth/);
    assert.match(bar,/clip: root\.compact/);
    assert.match(bar,/id: actionViewport[\s\S]*?id: reactionActions/);
    assert.match(bar,/columns: root\.compact \? 4/);
    assert.match(bar,/id: utilityActions[\s\S]*?y: root\.stackedActions \?/);
    assert.match(bar,/root\.compact \? actionViewport\.contentWidth - width/);
    assert.match(bar,/x: root\.layoutDirection === Qt\.RightToLeft \? 0/);
    assert.equal((read('ui/components/feed/CastItem.qml').match(/showViews: false/g) || []).length, 2);
    assert.match(read('ui/components/feed/DetailSurface.qml'),/showViews: true/);
    assert.match(bar,/root\.layoutDirection === Qt\.RightToLeft[\s\S]*?contentWidth - width/);
    assert.match(button,/implicitWidth: Math\.max\(Theme\.touchTarget/);
});

test('quote media stays within bounded card while names, handles and age elide', () => {
    const card=read('ui/components/media/EmbedCard.qml');
    const quote=card.slice(card.indexOf('visible: root.isCastReference'),
                           card.indexOf('visible: root.isExternal && (root.isImage || root.isVideo)'));
    assert.match(card,/Layout\.minimumWidth: 0/);
    assert.match(quote,/Layout\.minimumWidth: 0/g);
    assert.match(quote,/Layout\.maximumWidth: Math\.min\(100, root\.width \* 0\.24\)/);
    assert.match(quote,/Layout\.maximumWidth: Math\.min\(84, root\.width \* 0\.21\)/);
    assert.match(quote,/readonly property real boundedWidth: Math\.min\(92,/);
    assert.match(quote,/Layout\.maximumWidth: visible \? boundedWidth : 0/);
    assert.match(quote,/clip: true/);
    assert.match(quote,/fillMode: Image\.PreserveAspectCrop/);
});

test('header keeps diffuse NEON without clipped chromatic separator or duplicate safe-area', () => {
    const header=read('ui/components/shell/AppHeader.qml');
    const shell=read('ui/components/shell/AppShell.qml');
    assert.match(header,/implicitHeight: root\.compact \? Theme\.scaleUi\(54\)/);
    assert.match(header,/clip: false/);
    assert.match(header,/edgeStrength: 0\.68/);
    assert.match(header,/edgeWidth: 4/);
    assert.match(header,/strength: Theme\.reflectionStrength \* \(root\.compact \? 2\.30 : 2\.10\)/);
    const dock=read('ui/components/navigation/BottomNavigation.qml');
    assert.match(dock,/objectName: "mobileNavigationChromaticReflection"/);
    assert.match(dock,/reflectionSource: root\.reflectionSource/);
    assert.match(dock,/cornerRadius: navigationCapsule\.height \/ 2/);
    assert.match(shell,/reflectionSource: workspaceReflection/);
    assert.match(header,/bottomEdgeOnly: true/);
    assert.match(dock,/objectName: "mobileNavigationTopEdgeReflection"/);
    assert.match(dock,/edgeStrength: 0\.72/);
    assert.doesNotMatch(header,/height: 1\s+color: Theme\.borderMuted/);
    assert.match(header,/y: root\.compact \? \(parent\.height - height\) \/ 2 : 0/);
    assert.match(shell,/Layout\.topMargin: 0\s+Layout\.bottomMargin: 0/);
});

test('selected bottom navigation highlight is circular despite elastic slot width', () => {
    const nav=read('ui/components/navigation/BottomNavigation.qml');
    const destination=nav.slice(nav.indexOf('component DockDestination: Button'),
                                nav.indexOf('id: dockRow'));
    assert.match(destination,/background: Item \{/);
    assert.match(destination,/width: destination\.labelVisible \? parent\.width\s+: Math\.min\(parent\.width, parent\.height\)/);
    assert.match(destination,/height: parent\.height/);
    assert.match(destination,/anchors\.centerIn: parent/);
    assert.match(destination,/exponent: 2\.0/);
    assert.match(destination,/highQualityEdges: true/);
});
