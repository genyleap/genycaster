import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('Rich Cast v1 snapshots real stats and renders one 960 x 600 branded PNG', () => {
    const panel=read('ui/components/home/StatsPanel.qml');
    const card=read('ui/components/home/StatsShareCard.qml');
    assert.match(panel,/root\.shareSnapshot = Object\.assign\(\{\}, root\.metrics\)/);
    assert.match(panel,/root\.shareCapturedAt = Date\.now\(\)/);
    assert.match(panel,/root\.sharePeriodDays = root\.periodDays/);
    assert.match(panel,/shareCardPreview\.grabToImage\(/);
    assert.match(panel,/Qt\.size\(960, 600\)/);
    assert.match(panel,/result\.saveToFile\(localPath\)/);
    assert.match(panel,/StandardPaths\.TempLocation/);
    assert.match(panel,/statsShareModal\.opened/);
    assert.match(card,/objectName: "genycasterRichStatsCard"/);
    assert.match(card,/width: 960\s*height: 600/);
    assert.match(card,/snapshot\.hasEngagement === true/);
    assert.match(card,/engagementAvailable \? fmt\(value\) : "—"/);
    for(const metric of ['engagement','casts','replies','likes','recasts','receivedReplies'])
        assert.match(card,new RegExp('root\\.snapshot\\.'+metric));
    assert.match(card,/width: \(parent\.width - 13\) \/ 2/);
    assert.match(card,/width: \(parent\.width - 26\) \/ 3/);
    assert.match(card,/modelData\.value\.length > 6 \? 23/);
    assert.match(card,/cardStyle: "neon"/);
    assert.match(card,/lightStyle: cardStyle === "minimal"/);
});

test('Stats Cast is ordinary text and a single signed media embed, not a nonstandard protocol', () => {
    const home=read('ui/components/home/HomeDashboard.qml');
    const panel=read('ui/components/home/StatsPanel.qml');
    const shell=read('ui/components/shell/AppShell.qml');
    const composer=read('ui/components/composer/ComposerDrawer.qml');
    assert.match(panel,/signal shareRequested\(var payload\)/);
    assert.match(panel,/schema: "genycaster\.stats\/v1"/);
    assert.match(panel,/Genycaster Stats · /);
    assert.match(panel,/Powered by Genycaster/);
    assert.match(panel,/imageFileUrl: fileUrl/);
    assert.match(panel,/imageFileUrl: ""/); // explicit text-only fallback
    assert.match(home,/signal statsReportRequested\(var payload\)/);
    assert.equal((home.match(/onShareRequested: payload => root\.statsReportRequested\(payload\)/g)||[]).length,2);
    assert.match(shell,/composerDrawer\.statsImageFileUrl = String\(statsImageFileUrl \|\| ""\)/);
    assert.match(shell,/composerDrawer\.transientDraft = Boolean\(structuredReport\)/);
    assert.match(shell,/"statsImageFileUrl": String\(statsImageFileUrl \|\| ""\)/);
    assert.match(composer,/mediaUpload\.beginUpload\(root\.statsImageFileUrl, -1\)/);
    assert.match(composer,/richComposer\.insertManagedMedia\(url, mediaType, managementToken/);
    assert.match(composer,/mediaUpload\.busy/);
    assert.match(composer,/root\.statsImageFileUrl\.length > 0/);
    assert.match(composer,/root\.statsImageFileUrl\.length === 0/);
    assert.match(composer,/objectName: "statsCardUploadFallback"/);
    assert.match(composer,/root\.statsImageFileUrl = ""/);
    assert.match(composer,/root\.publicationEmbeds\(\)/);
    assert.doesNotMatch(panel,/publishCast|submitCast|sendCast/);
});

test('WebAssembly generated PNG upload reuses the signer proof, bounded media and cleanup', () => {
    const bridge=read('ui/app/runtime/bindings.cpp');
    const browser=read('ui/wasm/protocol-signer-pre.js');
    const sender=read('ui/wasm/browser_media_upload.mjs');
    assert.match(bridge,/MediaUploadController::uploadRequested/);
    assert.match(bridge,/Module\.FS\.readFile\(path\)/);
    assert.match(bridge,/__genycasterBrowserMedia\.uploadGenerated\(fid,\$4,bytes\)/);
    assert.match(browser,/function uploadGeneratedMedia\(fid,afterIndex,bytes\)/);
    assert.match(browser,/8\*1024\*1024/);
    assert.match(browser,/new File\(\[bytes\],'genycaster-stats-card\.png'/);
    assert.match(browser,/uploadBrowserImage\(\{/);
    assert.match(browser,/kind:'completed'/);
    assert.match(sender,/vault\.signMediaChallenge\(/);
    assert.match(sender,/\/v1\/uploads\/complete/);
    assert.match(browser,/remove:removeManagedMedia/);
});

test('Native feed identifies recap as presentation only, never verification', () => {
    const feed=read('ui/components/feed/CastItem.qml');
    assert.match(feed,/readonly property bool genycasterStatsCast/);
    assert.match(feed,/root\.visualEmbeds\.length === 1/);
    assert.match(feed,/genycasterStatsCastIdentity/);
    assert.match(feed,/NOT verification of author or metric values/);
    assert.match(feed,/id: castTextBody/);
    assert.match(feed,/objectName: "castVisualMedia"/);
});
