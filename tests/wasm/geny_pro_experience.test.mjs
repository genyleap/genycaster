import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=relative=>readFileSync(new URL('../../'+relative,import.meta.url),'utf8');
const qml=name=>read('ui/components/' + componentPath(name));
const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
const ids=['short','title','verified_wallet','unlock_hint',
    'verification_unavailable','not_now','check_status',
    'bookmark_collections','connections','requires_pro',
    'connections_hint','advanced_editor','market_pulse_hint','market_pulse'];


test('Profile Pro badge opens the same responsive drawer on touch and desktop',()=>{
 const badge=qml('ProfileProBadge.qml');
 const profile=qml('ProfileSurface.qml');
 const shell=qml('AppShell.qml');
 const drawer=qml('GenyProDetailsDrawer.qml');
 assert.match(badge,/signal detailsRequested\(\)/);
 assert.match(badge,/onClicked: \{[\s\S]*?root\.detailsRequested\(\)/);
 assert.match(profile,/onDetailsRequested: root\.proRequested\(\)/);
 assert.match(shell,/onProRequested: genyProDetailsDrawer\.open\(\)/);
 assert.match(drawer,/bottomSheet: root\.compact/);
 assert.match(drawer,/objectName: "genyProDrawerScrollableDetails"/);
 assert.match(drawer,/property int expandedFeature: -1/);
 assert.match(drawer,/onClicked: root\.expandedFeature = expanded \? -1 : index/);
});

test('desktop bookmarks use measured compact columns and mobile stays single-column',()=>{
 const bookmarks=qml('BookmarksSurface.qml');
 const shell=qml('AppShell.qml');
 assert.match(bookmarks,/objectName: "desktopBookmarkGrid"/);
 assert.match(bookmarks,/readonly property int columnCount: root\.compact \|\| gridAvailableWidth < 660/);
 assert.match(bookmarks,/gridAvailableWidth < 1680 \? 2 : 3/);
 assert.match(bookmarks,/Layout\.alignment: Qt\.AlignTop \| Qt\.AlignLeft/);
 assert.match(bookmarks,/Layout\.preferredWidth: savedCastsGrid\.cardWidth/);
 assert.match(bookmarks,/card\.mapToItem\(root\.contentItem/);
 assert.match(shell,/Layout\.maximumWidth: root\.compact[\s\S]*?root\.adaptiveDashboardWidth/);
});

test('cast replies clear the dock and channel toolbar separates narrow layouts',()=>{
 const detail=qml('DetailSurface.qml');
 const shell=qml('AppShell.qml');
 assert.match(detail,/bottomMargin: root\.compact \? root\.bottomOverlayInset/);
 assert.match(detail,/footer: Item \{/);
 assert.match(detail,/visible: root\.compact && root\.activeView === "cast"[\s\S]*?sourceUrl: root\.resolvedChannelImage/);
 assert.match(shell,/DetailSurface \{[\s\S]*?bottomOverlayInset: root\.readingOverlayInset\(castSurfaceLoader\)/);
 assert.match(shell,/function readingOverlayInset\(surface\)/);
});

test('explorer and profile filter chips share the normal hairline stroke',()=>{
 const filters=qml('ContentFilterSheet.qml');
 assert.match(filters,/borderWidth: Theme\.strokeHairline/);
 assert.doesNotMatch(filters,/borderWidth: chip\.chosen \? 2/);
});
test('Pro badge shares the streak row, with themed popup and accessible inputs',()=>{
    const profile=qml('ProfileSurface.qml');
    const badge=qml('ProfileProBadge.qml');
    const top=profile.slice(profile.indexOf('objectName: "profileIdentityRow"'),
        profile.indexOf('objectName: "profileLocationRow"'));
    const bottom=profile.slice(profile.indexOf('id: profileBadgesRow'),
        profile.indexOf('objectName: "profileOwnerActions"'));
    assert.doesNotMatch(top,/genyProBadge|ProfileProBadge|verifiedGenyProBadge/);
    assert.match(bottom,/ProfileProBadge\s*\{[\s\S]*?id: profileProBadge[\s\S]*?id: profileStreakBadge/);
    assert.match(bottom,/profileStreakBadge[\s\S]*?profileUnverifiedStreak/);
    assert.match(profile,/id: profileBadgesRow[\s\S]*?anchors\.right: parent\.right/);
    assert.match(badge,/entitlement.status === "verified"[\s\S]*?entitlement.eligible === true/);
    assert.match(badge,/onHoveredChanged:/);
    assert.match(badge,/onClicked:/);
    assert.match(badge,/Keys.onEscapePressed:/);
    assert.match(badge,/Accessible.description:/);
    assert.match(badge,/objectName: "genyProThemedTooltip"/);
    assert.match(badge,/background: PopupSurface/);
    assert.doesNotMatch(badge,/ToolTip\.visible/);
    for(const token of ['proAccent','proSurface','proSurfaceHover','proBorder'])
        assert.ok(read('ui/theme/Theme.qml').includes('readonly property color '+token));
});

test('Pro-only actions explain why they are locked, but users may unlink and unfile',()=>{
    const connections=qml('AccountConnectionsPopup.qml');
    const chooser=qml('BookmarkCollectionChooser.qml');
    const notice=qml('ProUpgradeNotice.qml');
    const formatting=qml('FormattingToolbar.qml');
    const composer=qml('ComposerDrawer.qml');
    assert.match(connections,/objectName: "connectionsProGatingBanner"/);
    assert.match(connections,/connectionsProNotice\.open\(\)/);
    assert.match(connections,/picker\.disconnectRequested\(providerRow\.modelData/);
    assert.match(connections,/picker\.authorized && providerRow\.redirectCapable/);
    assert.match(chooser,/text: qsTrId\("genycaster.bookmarks.uncategorized"\)\s+enabled: !root.busy/);
    assert.match(chooser,/root\.unfiledRequested\(\)/);
    assert.match(chooser,/collectionsProNotice\.open\(\)/);
    assert.match(chooser,/enabled: root.proEligible/);
    assert.match(formatting,/signal proFeatureRequested\(\)/);
    assert.match(formatting,/root\.proFeatureRequested\(\)/);
    assert.match(composer,/onProFeatureRequested: composerProNotice\.open\(\)/);
    assert.match(notice,/openProofConnections\.retryProEntitlements\(\)/);
    assert.match(notice,/background: PopupSurface/);
    assert.match(notice,/Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.doesNotMatch(notice,/eligible\s*=\s*true/);
});

test('Market Pulse basic cap offers Pro information, without bypassing native cap',()=>{
    const settings=qml('LocaleSettingsPopup.qml');
    const controller=read('ui/app/data/insights.cpp');
    const wiring=read('ui/app/main.cpp');
    assert.match(settings,/readonly property int marketAssetLimit: marketProEligible \? 16 : 4/);
    assert.match(settings,/objectName: "genyProMarketQuota"/);
    assert.match(settings,/marketProNotice\.open\(\)/);
    assert.match(settings,/openProofConnections\.fetchProEntitlements\(true\)/);
    assert.match(controller,/std::clamp\(limit, 4, 16\)/);
    assert.match(controller,/current >= m_impl->marketAssetLimit/);
    assert.match(wiring,/proEntitlementsChanged/);
});

test('New Pro explanations have a translation and valid placeholders in every locale',()=>{
    for(const lang of locales){
        const xml=read('ui/i18n/qml_'+lang+'.ts');
        for(const id of ids) {
            const pattern=new RegExp('<message id="genycaster\\.pro\\.'+id+'">\\s*'
                +'<source>([\\s\\S]*?)<\\/source>\\s*'
                +'<translation>([\\s\\S]*?)<\\/translation>');
            const match=xml.match(pattern);
            assert.ok(match,lang+' missing '+id);
            assert.ok(match[2].trim(),lang+' empty '+id);
            assert.deepEqual([...match[1].matchAll(/%\\d+/g)].map(x=>x[0]),
                [...match[2].matchAll(/%\\d+/g)].map(x=>x[0]),lang+' placeholders for '+id);
        }
    }
});
