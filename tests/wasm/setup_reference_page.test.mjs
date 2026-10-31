import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const dir=path.join(root,'prototypes/setup-reference-page');
const src=name=>fs.readFileSync(path.join(dir,name),'utf8');
test('preview is standalone and uses real semantic page controls',()=>{
 const html=src('index.html');
 assert.match(html,/<main class="app dark" id="app"/);
 assert.match(html,/id="scene"/);
 assert.match(html,/id="continueButton"/);
 assert.match(html,/id="backButton"/);
 assert.match(html,/id="darkTheme"/);
 assert.match(html,/id="lightTheme"/);
 assert.match(html,/© 2026/);
 assert.match(html,/https:\/\/genyleap\.com\//);
 assert.match(html,/type="module" src="\.\/page\.js"/);
 assert.match(html,/noindex,nofollow/);
 assert.match(html,/real Setup remains unchanged|existing GenyCaster Setup and account data are unchanged/i);
});
test('the reference is a GPU-driven theme-aware page with functional navigation',()=>{
 const art=src('art.js'),script=src('page.js');
 assert.match(art,/THREE\.WebGLRenderer/);
 assert.match(art,/CatmullRomCurve3/);
 assert.match(art,/THREE\.MeshPhysicalMaterial/);
 assert.match(art,/pointermove/);
 assert.match(art,/prefers-reduced-motion/);
 assert.match(script,/const steps=\[/);
 assert.match(script,/next\.addEventListener\("click"/);
 assert.match(script,/back\.addEventListener\("click"/);
 assert.match(script,/setArtTheme/);
 assert.match(script,/aria-pressed/);
 assert.match(src('compare.html'),/iframe/);
 assert.match(src('compare.html'),/theme=dark/);
 assert.match(src('compare.html'),/theme=light/);
});
test('no external dependencies or production Setup modifications',()=>{
 for(const file of ['index.html','page.js','art.js','styles.css','compare.html']){
   assert.doesNotMatch(src(file),/src="https?:\/\//);
 }
 assert.ok(fs.statSync(path.join(dir,'assets/three.module.js')).size>500000);
 assert.ok(fs.statSync(path.join(dir,'assets/genycaster-icon.svg')).size>500);
 const setup=fs.readFileSync(path.join(root,'ui/components/InitialSetup.qml'),'utf8');
 assert.doesNotMatch(setup,/setup-reference-page|SetupConceptThree/);
});
