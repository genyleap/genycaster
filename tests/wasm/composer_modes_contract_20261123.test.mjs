import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');

test('Markdown source is text-only and preserves typed upload and embed blocks', () => {
    const header = read('src/presentation/qt/rich_composer_controller.hpp');
    const controller = read('src/presentation/qt/rich_composer_controller.cpp');
    const composer = read('ui/components/composer/ComposerDrawer.qml');
    assert.match(header, /Q_PROPERTY\(QString markdownBodySource READ markdownBodySource WRITE setMarkdownBodySource/);
    assert.match(controller, /QString RichComposerController::markdownBodySource\(\) const/);
    assert.match(controller, /void RichComposerController::setMarkdownBodySource\(const QString& source\)/);
    assert.match(controller, /block\.kind == BlockKind::Media/);
    assert.match(controller, /block\.kind == BlockKind::Embed/);
    assert.match(controller, /block\.kind == BlockKind::CastReference/);
    assert.match(controller, /parsed->blocks\.insert\(/);
    assert.match(controller, /m_impl->syncMarkdownFromDocument\(\);/);
    assert.match(composer, /text: richComposer\.markdownBodySource/);
    assert.match(composer, /richComposer\.markdownBodySource = markdownEditor\.text/);
    assert.doesNotMatch(composer, /markdownEditor\.text !== richComposer\.markdownSource/);
    assert.match(composer, /richComposer\.removeBlock\(/);
});

test('preview scales actual image without cropping or nested fixed-height clipping', () => {
    const preview = read('ui/components/media/FarcasterPreview.qml');
    const media = read('ui/components/composer/MediaUploadPanel.qml');
    assert.match(preview, /fitImage: true/);
    assert.match(preview, /implicitHeight: Math\.max\(root\.compact \? 190 : 245,/);
    assert.match(preview, /previewColumn\.implicitHeight \+ Theme\.space4/);
    assert.doesNotMatch(preview, /ScrollView \{/);
    assert.match(preview, /id: previewViewport/);
    assert.match(preview, /mediaTile\.imageAspectRatio/);
    assert.match(preview, /const presentationSource = localPreview\.length > 0 \? localPreview : url/);
    assert.match(read('ui/components/media/MediaPreview.qml'), /imageAspectRatio:/);
    assert.match(media, /objectName: "composerOriginalImageStrip"/);
    assert.match(media, /Layout\.preferredHeight: root\.width < 620 \? 122 : 140/);
    assert.match(media, /onClicked: richComposer\.removeBlock\(Number\(modelData\.blockIndex\)\)/);
    assert.match(media, /width: visible \? Theme\.compactControlHeight \+ Theme\.space2 : 0/);
});

test('Neon and Minimal use different raster layouts and are not palette variants', () => {
    const renderer = read('src/presentation/qt/stats_share_card_renderer.cpp');
    assert.match(renderer, /void paintMinimalCard\(/);
    assert.match(renderer, /if \(minimal\) \{\s*paintMinimalCard\(/);
    assert.match(renderer, /QStringLiteral\("AUDIENCE RESPONSE"\)/);
    assert.match(renderer, /QStringLiteral\("PUBLISHING"\)/);
    assert.match(renderer, /QStringLiteral\("Interactions received"\)/);
    assert.match(renderer, /CanvasWidth = LayoutWidth \* RasterScale/);
});

test('avatar failure retries are bounded and origin-safe', () => {
    const controller = read('src/presentation/qt/media_upload_controller.cpp');
    const gateway = read('services/media-gateway/browser_image_proxy.mjs');
    assert.match(controller, /statsAvatarSource\(avatarUrl\)/);
    assert.match(controller, /statsAvatarSource\(m_statsAvatarUrl\)/);
    assert.match(controller, /QTimer::singleShot\(10000, this,/);
    assert.match(controller, /startStatsAvatarFallback\(generation, true\)/);
    assert.match(controller, /QByteArray::AbortOnBase64DecodingErrors/);
    assert.match(gateway, /\? 'image\/jpeg,image\/png;q=0\.95'/);
    assert.doesNotMatch(gateway, /image\/webp;q=0\.85/);
});
