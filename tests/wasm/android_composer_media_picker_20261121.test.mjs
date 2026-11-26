import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Android composer photo and GIF actions use the scoped system document picker', () => {
  const qml = source('ui/components/composer/ComposerDrawer.qml');
  const picker = qml.slice(qml.indexOf('function openMediaPicker()'),
    qml.indexOf('function clearPendingMediaQueue()'));
  assert.ok(picker.includes('Qt.platform.os === "android"'), 'Android must have a native picker path');
  assert.ok(picker.includes('root.insertKind = "media"'), 'Import errors must have a visible panel');
  assert.ok(picker.includes('mediaUpload.browseBrowserImage(root.pendingMediaBlockIndex)'));
  assert.ok(picker.includes('mediaFileDialog.open()'), 'Desktop picker must remain supported');
  const mobileBar = qml.slice(qml.indexOf('objectName: "mobileComposerMediaBar"'));
  for (const marker of ['Theme.iconImage', 'text: "GIF"'])
    assert.ok(mobileBar.includes(marker), marker);
  assert.ok(mobileBar.match(/root\.openMediaPicker\(\)/g)?.length >= 2,
    'Both Android mobile toolbar actions must invoke the same picker');
});

test('Android native SAF bridge imports content URI safely before normal upload', () => {
  const picker = source('ui/app/media/android_document_picker.hpp');
  const main = source('ui/app/main.cpp');
  const controller = source('src/presentation/qt/media_upload_controller.cpp');
  for (const token of [
    'android.intent.action.OPEN_DOCUMENT', 'android.intent.category.OPENABLE',
    'android.intent.extra.MIME_TYPES', 'QtAndroidPrivate::startActivity',
    'handleActivityResult', 'content://', 'getContentResolver',
    'QStandardPaths::CacheLocation', 'QTemporaryDir', 'QSaveFile',
    'QThread::create', 'QIODevice::ReadOnly', 'QUrl::fromLocalFile(output)',
    'm_media->beginUpload', '25LL * 1024 * 1024', '512LL * 1024 * 1024'
  ]) assert.ok(picker.includes(token), token);
  assert.ok(!picker.includes('jni->DeleteLocalRef(stringClass)'),
    'Qt findClass returns a borrowed global reference, never a local reference');
  assert.ok(picker.includes('const bool allocationFailed = jni.checkAndClearExceptions()'));
  assert.ok(main.includes('androidMediaPickerOwner = std::make_unique<AndroidComposerMediaPicker>'));
  assert.ok(main.includes('browserImagePickRequested'));
  assert.ok(controller.includes('!localFile.isLocalFile()'),
    'Transport must continue rejecting unnormalized content:// URLs');
});

test('Android scoped chooser requests no broad storage permissions and is not linked on desktop/WASM', () => {
  const cmake = source('ui/CMakeLists.txt');
  const manifest = source('properties/Android/AndroidManifest.xml');
  assert.ok(cmake.includes('if(ANDROID)\n    # QAndroidActivityResultReceiver'));
  assert.ok(cmake.includes('Svg CorePrivate'));
  assert.ok(cmake.includes('app/media/android_document_picker.hpp'));
  assert.ok(!manifest.includes('android.permission.READ_MEDIA_IMAGES'));
  assert.ok(!manifest.includes('android.permission.READ_EXTERNAL_STORAGE'));
  assert.ok(!manifest.includes('android.permission.MANAGE_EXTERNAL_STORAGE'));
});
