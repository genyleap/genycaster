import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Qt Quick WASM is pinned to OpenGL RHI and browser WebGL instead of Qt raster backend',()=>{
  const main=read('ui/app/main.cpp');
  const loader=read('ui/wasm/genycaster-loader.js');
  assert.match(main,/#if defined\(Q_OS_WASM\)\s*\/\/ Qt Quick's browser scene graph[\s\S]*?QQuickWindow::setGraphicsApi\(QSGRendererInterface::OpenGLRhi\)/);
  assert.match(main,/Genycaster WebAssembly Qt Quick renderer/);
  assert.match(loader,/function browserGraphicsStatus\(\)/);
  assert.match(loader,/getContext\('webgl2'\)\|\|canvas.getContext\('webgl'\)/);
  assert.match(loader,/WEBGL_debug_renderer_info/);
  assert.match(loader,/swiftshader\|llvmpipe\|softpipe/);
  assert.match(loader,/console\.info\("\[Genycaster\] Graphics"/);
  assert.doesNotMatch(main,/#if defined\(Q_OS_WASM\)[\s\S]{0,250}qputenv\("QT_QUICK_BACKEND", "software"\)/);
});

test('paid Farcaster reserve is never probed at ordinary app startup or login',()=>{
  const c=read('ui/app/identity/neynar_registration_controller.cpp');
  const setup=read('ui/components/setup/InitialSetup.qml');
  const initial=c.slice(c.indexOf('NeynarRegistrationController::NeynarRegistrationController'),
    c.indexOf('void NeynarRegistrationController::refreshAvailability'));
  assert.doesNotMatch(initial,/QTimer::singleShot\([^;]*refreshAvailability/);
  const ready=c.slice(c.indexOf('void NeynarRegistrationController::setIdentityRuntimeReady'),
    c.indexOf('bool NeynarRegistrationController::walletMatches'));
  assert.doesNotMatch(ready,/refreshAvailability\(\)/);
  assert.match(setup,/objectName: "setupRegisterNeynar"[\s\S]*?neynarRegistration\.refreshAvailability\(\)/);
  assert.match(setup,/onRuntimeReadyChanged:[\s\S]*?registrationProviderId === "farcaster-snapchain"[\s\S]*?neynarRegistration\.refreshAvailability\(\)/);
});
