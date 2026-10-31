import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const project=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const preview=path.join(project,'prototypes/setup-reference-page');
const source=name=>fs.readFileSync(path.join(preview,name),'utf8');

test('dual-theme reference is a genuine page with semantic controls, not a screenshot',()=>{
  const page=source('index.html');
  assert.match(page,/<canvas id="scene"/);
  assert.match(page,/<section class="setup-panel"/);
  assert.match(page,/<button class="continue-button"/);
  assert.match(page,/<button class="back-button"/);
  assert.match(page,/<button type="button" id="darkTheme"/);
  assert.match(page,/<button type="button" id="lightTheme"/);
  assert.match(page,/<a href="https:\/\/genyleap\.com\/"/);
  assert.match(page,/Beta 1\.0 · v1\.0\.0/);
  assert.match(page,/src="\.\/assets\/genycaster-icon\.svg"/);
  assert.match(page,/type="module" src="\.\/page\.js"/);
  assert.match(page,/noindex,nofollow/);
  assert.doesNotMatch(page,/\.png"[^>]*class="setup-panel"/);
});
test('the four-step example is actually interactive, and explicitly not authentication',()=>{
  const js=source('page.js');
  assert.match(js,/const steps=\[/);
  assert.match(js,/step=\(step\+1\)%steps\.length/);
  assert.match(js,/step=Math\.max\(0,step-1\)/);
  assert.match(js,/dark\.addEventListener\("click"/);
  assert.match(js,/light\.addEventListener\("click"/);
  assert.match(js,/This is an interactive design preview/);
  assert.doesNotMatch(js,/fetch\(|localStorage|recoveryPhrase|password|signer\.start/);
});
test('artwork is a real GPU WebGL renderer that respects reduced-motion',()=>{
  const js=source('art.js');
  assert.match(js,/THREE\.WebGLRenderer/);
  assert.match(js,/canvas\.addEventListener\("pointermove"/);
  assert.match(js,/prefers-reduced-motion: reduce/);
  assert.match(js,/THREE\.SphereGeometry/);
  assert.match(js,/THREE\.BufferGeometry/);
  assert.match(js,/THREE\.ShaderMaterial/);
  assert.match(js,/requestAnimationFrame/);
  assert.match(js,/canvas\.style\.display="none"/);
  assert.doesNotMatch(js,/fetch\(|WebSocket|https?:\/\//);
});
test('comparison uses both live preview pages, and repository Setup remains untouched',()=>{
  const compare=source('compare.html');
  assert.match(compare,/iframe src="\.\/index\.html\?theme=dark&compare=1"/);
  assert.match(compare,/iframe src="\.\/index\.html\?theme=light&compare=1"/);
  const prod=fs.readFileSync(path.join(project,'ui/components/InitialSetup.qml'),'utf8');
  assert.doesNotMatch(prod,/setup-reference-page|preview\/setup-concept/);
  assert.equal(fs.statSync(path.join(preview,'assets/three.module.js')).size>1000000,true);
  assert.equal(fs.statSync(path.join(preview,'assets/genycaster-icon.svg')).size>100,true);
});
