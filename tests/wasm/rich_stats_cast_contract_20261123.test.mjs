import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('Rich Cast v1 uses one CPU-raster PNG as both preview and published image', () => {
    const panel=read('ui/components/home/StatsPanel.qml');
    const card=read('ui/components/home/StatsShareCard.qml');
    const renderer=read('src/presentation/qt/stats_share_card_renderer.cpp');
    const controller=read('src/presentation/qt/media_upload_controller.cpp');
    const header=read('src/presentation/qt/media_upload_controller.hpp');
    const cmake=read('src/presentation/CMakeLists.txt');
    const bridge=read('ui/app/runtime/bindings.cpp');
    assert.match(panel,/root\.shareSnapshot = Object\.assign\(\{\}, root\.metrics\)/);
    assert.match(panel,/root\.shareCapturedAt = Date\.now\(\)/);
    assert.match(panel,/root\.sharePeriodDays = root\.periodDays/);
    assert.match(panel,/mediaUpload\.renderStatsCard\(/);
    assert.match(panel,/root\.shareGeneratedCardUrl = next/);
    assert.match(panel,/sourceUrl: root\.shareGeneratedCardUrl/);
    assert.match(panel,/root\.shareCardHandedOff = true/);
    assert.match(panel,/mediaUpload\.discardStatsCardImage\(unusedImage\)/);
    assert.match(panel,/if \(!root\.shareGeneratedCardUrl\.length \|\| !shareCardPreview\.ready\)/);
    assert.doesNotMatch(panel,/\.grabToImage\s*\(|result\.saveToFile\s*\(|StandardPaths\.TempLocation/);
    assert.match(card,/objectName: "genycasterRichStatsCard"/);
    assert.match(card,/width: 960\s*height: 960/);
    assert.match(card,/source: root\.sourceUrl/);
    assert.match(card,/cache: false/);
    assert.match(card,/readonly property bool ready: previewImage\.status === Image\.Ready/);
    assert.match(renderer,/LayoutWidth = 960/);
    assert.match(renderer,/LayoutHeight = 960/);
    assert.match(renderer,/RasterScale = 2/);
    assert.match(renderer,/painter.scale\(RasterScale, RasterScale\)/);
    assert.match(card,/sourceSize: Qt.size\(1920, 1920\)/);
    assert.match(renderer,/QImage::Format_ARGB32_Premultiplied/);
    assert.match(renderer,/QPainter painter\{&output\}/);
    assert.match(renderer,/QTemporaryFile temp/);
    assert.match(renderer,/output\.save\(&temp, "PNG"\)/);
    assert.match(renderer,/avatar\(painter, portrait, displayName/);
    assert.doesNotMatch(renderer,/brandMark\(/);
    assert.match(renderer,/statistic\(painter, palette/);
    assert.match(renderer,/for \(int i = 0; i < 3; \+\+i\)/);
    assert.match(renderer,/for \(int i = 0; i < 2; \+\+i\)/);
    assert.match(panel,/Math\.min\(460, parent\.width/);
    assert.match(panel,/Layout\.preferredHeight: width \/\/ 1:1 square/);
    assert.match(panel,/shareAccountAvatarUrl/);
    assert.match(panel,/mediaUpload\.prepareStatsAvatar\(root\.shareAccountAvatarUrl\)/);
    assert.match(panel,/function onStatsAvatarChanged\(\)/);
    assert.match(controller,/m_statsAvatarUrl/);
    assert.match(controller,/QNetworkReply::finished/);
    for(const metric of ['engagement','casts','replies','likes','recasts','receivedReplies'])
        assert.ok(renderer.includes('formattedCount(stats, "' + metric + '")'));
    assert.match(controller,/QVariantMap MediaUploadController::renderStatsCard/);
    assert.match(header,/Q_INVOKABLE QVariantMap renderStatsCard/);
    assert.match(header,/Q_INVOKABLE void prepareStatsAvatar/);
    assert.match(header,/void statsAvatarChanged\(\)/);
    assert.match(header,/Q_PROPERTY\(bool statsAvatarLoading/);
    assert.match(header,/Q_PROPERTY\(bool statsAvatarReady/);
    assert.match(panel,/statsAvatarReady/);
    assert.match(panel,/function shareImageExternally\(\)/);
    assert.match(panel,/onClicked: root\.shareImageExternally\(\)/);
    assert.doesNotMatch(panel,/twitter\.com\/intent\/tweet|t\.me\/share\/url/);
    assert.match(panel,/mediaUpload\.shareStatsImage\(/);
    assert.match(bridge,/sharePreparedStatsImage/);
    // C++ unit targets only link QtCore/Qml/Network. Never force QtGui's
    // QImage header into this public presentation adapter's include graph.
    assert.doesNotMatch(header,/#include\s*[<"]QImage[>"]/);
    assert.match(header,/class QImage;/);
    assert.match(header,/std::unique_ptr<QImage> m_statsAvatar/);
    assert.match(controller,/std::make_unique<QImage>/);
    assert.match(controller,/m_statsAvatarImageProvider->requestImageResponse/);
    assert.match(header,/setStatsAvatarImageProvider/);
    assert.match(read('ui/app/main.cpp'),/mediaUploadController\.setStatsAvatarImageProvider\(remoteAvatarImageProvider\)/);
    assert.doesNotMatch(panel,/\&\& \(!root\.shareAccountAvatarUrl\.length \|\| mediaUpload\.statsAvatarReady\)/);
    assert.match(panel,/!mediaUpload\.statsAvatarLoading/);
    assert.match(cmake,/qt\/stats_share_card_renderer\.cpp/);
});

test('Stats Cast is ordinary text and a single signed media embed, not a nonstandard protocol', () => {
    const home=read('ui/components/home/HomeDashboard.qml');
    const panel=read('ui/components/home/StatsPanel.qml');
    const shell=read('ui/components/shell/AppShell.qml');
    const composer=read('ui/components/composer/ComposerDrawer.qml');
    assert.match(panel,/signal shareRequested\(var payload\)/);
    assert.match(panel,/schema: "genycaster\.stats\/v1"/);
    assert.match(panel,/Genycaster Stats · /);
    assert.match(panel,/lines\.push\("", "Powered by Genycaster"\)/);
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
    assert.match(composer,/root\.statsImageUploadAttempted = true/);
    assert.match(composer,/mediaUpload\.discardStatsCardImage\(temporaryImage\)/);
    assert.match(composer,/qsTrId\("genycaster\.media\.retry"\)/);
    assert.match(composer,/richComposer\.setBlockText\(0, String\(root\.suggestedText \|\| ""\)\)/);
    assert.match(composer,/root\.publicationEmbeds\(\)/);
    assert.doesNotMatch(panel,/publishCast|submitCast|sendCast/);
});

test('WebAssembly generated PNG upload reuses the signer proof, bounded media and cleanup', () => {
    const bridge=read('ui/app/runtime/bindings.cpp');
    const browser=read('ui/wasm/protocol-signer-pre.js');
    const sender=read('ui/wasm/browser_media_upload.mjs');
    assert.match(bridge,/MediaUploadController::uploadRequested/);
    assert.match(bridge,/const bytes=FS\.readFile\(path\)/);
    assert.match(bridge,/__genycasterBrowserMedia\.uploadGenerated\(fid,\$4,bytes,\$5\)/);
    assert.match(browser,/function uploadGeneratedMedia\(fid,afterIndex,bytes,maxImageBytes/);
    assert.match(browser,/maxImageBytes===5\*1024\*1024\?5\*1024\*1024:2\*1024\*1024/);
    assert.match(browser,/new File\(\[bytes\],'genycaster-stats-card\.png'/);
    assert.match(browser,/uploadBrowserImage\(\{/);
    assert.match(browser,/kind:'completed'/);
    assert.match(sender,/vault\.signMediaChallenge\(/);
    assert.match(sender,/\/v1\/uploads\/complete/);
    assert.match(browser,/remove:removeManagedMedia/);
});

test('Native uploads distinguish local session rejection from remote gateway unavailability', () => {
    const bridge = read('ui/app/runtime/bindings.cpp');
    const upload = read('src/application/media_upload.cpp');
    const gateway = read('src/transport/qt_network/media_upload.cpp');
    assert.match(bridge,/QString mediaUploadFailureText\(const foundation::error::ErrorCode code\)/);
    assert.match(bridge,/case ErrorCode::Unauthorized:\s*return qtTrId\("genycaster\.media\.upload_authorization_failed"\)/);
    assert.match(bridge,/case ErrorCode::TransportFailure:/);
    assert.match(bridge,/mediaUploadFailureText\(started\.error\(\)\.code\)/);
    assert.match(bridge,/mediaUploadFailureText\(state\.error->code\)/);
    assert.doesNotMatch(bridge,/controllerGuard->applyFailed\(qtTrId\("genycaster\.media\.upload_network_failed"\)\)/);
    assert.match(upload,/if \(!session \|\| session->fid\.value == 0U \|\| domain::session::isExpired/);
    assert.match(gateway,/authorizationHeaders\(/);
    assert.match(gateway,/performRequest\(/);
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
