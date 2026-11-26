import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
test('Android selects FFmpeg explicitly before media construction and requires deployed plugin',()=>{
 const main=read('ui/app/main.cpp'),cmake=read('ui/CMakeLists.txt'),packageCheck=read('ci/verify_platform_package.py');
 assert.match(main,/QPlatformMediaIntegration::setBackend\(QStringLiteral\("ffmpeg"\)\)/);
 assert.ok(main.indexOf('QPlatformMediaIntegration::setBackend') < main.indexOf('QQmlApplicationEngine engine'));
 assert.match(main,/ro\.boot\.qemu", emulatorMedia[\s\S]*?QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ","/);
 assert.match(main,/QT_DISABLE_HW_TEXTURES_CONVERSION", "1"/);
 assert.match(main,/QQuickWindow::setGraphicsApi\(QSGRendererInterface::Vulkan\)/);
 assert.match(cmake,/MultimediaPrivate/);
 assert.match(cmake,/INCLUDE_BY_TYPE multimedia Qt6::QFFmpegMediaPlugin/);
 assert.match(packageCheck,/libplugins_multimedia_ffmpegmediaplugin_/);
});
