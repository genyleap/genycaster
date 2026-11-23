import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Clear content queues only unpublished managed uploads before resetting the draft', () => {
    const qml = read('ui/components/composer/ComposerDrawer.qml');
    const cpp = read('src/presentation/qt/rich_composer_controller.cpp');
    const header = read('src/presentation/qt/rich_composer_controller.hpp');
    const view = qml.slice(qml.indexOf('function clearAllComposerContent()'),
                           qml.indexOf('function startNextMediaUpload()'));
    assert.match(view,/mediaUpload\.cancel\(\)/);
    assert.match(view,/richComposer\.clearCurrentContent\(/);
    assert.match(view,/root\.replyMode \|\| root\.quoteMode \|\| root\.transientDraft/);
    assert.match(header,/Q_INVOKABLE void clearCurrentContent\(bool preserveContext\)/);
    const cleanup=cpp.slice(cpp.indexOf('void RichComposerController::clearCurrentContent('),
                            cpp.indexOf('void RichComposerController::undo()'));
    assert.match(cleanup,/m_impl->contextDocument/);
    assert.match(cleanup,/tokenReferencedByPublishedCast/);
    assert.match(cleanup,/queueCleanupToken/);
    assert.match(cleanup,/m_impl->settings\.sync\(\)/);
    assert.match(cleanup,/emit managedAssetCleanupRequested\(\)/);
    assert.match(cpp,/void RichComposerController::clearDraft\(\)[\s\S]*?if \(cleanupQueued\)\s+emit managedAssetCleanupRequested\(\)/);
    assert.match(cpp,/void RichComposerController::beginContextDocument\(\)[\s\S]*?clearCurrentContent\(true\)/);
});

test('qStorage deletion requires a signed owner-scoped management token', () => {
    const browser = read('ui/wasm/browser_media_upload.mjs');
    const runtime = read('ui/app/runtime/bindings.cpp');
    const native = read('src/presentation/qt/rich_composer_controller.cpp');
    assert.match(browser,/export async function deleteBrowserAsset/);
    assert.match(browser,/const method='DELETE',path='\/v1\/assets'/);
    assert.match(browser,/vault\.signMediaChallenge\(fid,challenge\)/);
    assert.match(browser,/if\(response\.status===204\)/);
    assert.match(native,/tokenReferencedByPublishedCast/);
    assert.match(runtime,/scheduleManagedMediaCleanup\(richComposer, runtime\)/);
    assert.match(runtime,/richComposer\.pendingManagedAssetCleanup\(\)/);
    assert.match(runtime,/richComposer\.markManagedAssetCleanupCompleted\(token\)/);
});

test('compact attachment row keeps file details and actions beside the actual thumbnail', () => {
    const panel=read('ui/components/composer/MediaUploadPanel.qml');
    assert.match(panel,/objectName: "composerOriginalImageStrip"/);
    assert.match(panel,/Layout\.preferredHeight: root\.width < 620 \? 122 : 140/);
    assert.match(panel,/width: Math\.max\(180, Math\.min\(root\.width/);
    assert.match(panel,/anchors\.left: attachedPreview\.right/);
    assert.match(panel,/text: String\(modelData\.fileName \|\| ""\)/);
    assert.match(panel,/onClicked: richComposer\.removeBlock\(Number\(modelData\.blockIndex\)\)/);
    assert.match(panel,/onClicked: root\.browseRequested\(root\.blockIndex\)/);
});

test('Farcaster Preview has one scroll owner and sizes image tiles by intrinsic aspect ratio', () => {
    const preview=read('ui/components/media/FarcasterPreview.qml');
    const shared=read('ui/components/media/MediaPreview.qml');
    const composer=read('ui/components/composer/ComposerDrawer.qml');
    assert.doesNotMatch(preview,/ScrollView \{/);
    assert.match(preview,/previewColumn\.implicitHeight \+ Theme\.space4/);
    assert.match(preview,/mediaTile\.imageAspectRatio/);
    assert.match(preview,/const presentationSource = localPreview\.length > 0 \? localPreview : url/);
    assert.match(shared,/readonly property real imageAspectRatio:/);
    assert.match(composer,/id: composerScroll/);
    assert.match(composer,/contentHeight: composerContent\.implicitHeight/);
});
