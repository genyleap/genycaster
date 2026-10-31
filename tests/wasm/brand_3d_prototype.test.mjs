import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const project=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const dir=path.join(project,'prototypes/setup-brand-3d');
const read=p=>fs.readFileSync(path.join(dir,p),'utf8');

function glb(filename){
    const bytes=fs.readFileSync(path.join(dir,'assets',filename));
    assert.equal(bytes.toString('ascii',0,4),'glTF');
    assert.equal(bytes.readUInt32LE(4),2);
    assert.equal(bytes.readUInt32LE(8),bytes.length);
    assert.equal(bytes.toString('ascii',16,20),'JSON');
    const size=bytes.readUInt32LE(12);
    const json=JSON.parse(bytes.subarray(20,20+size).toString('utf8'));
    assert.equal(json.asset.version,'2.0');
    const binOffset=20+size;
    assert.equal(bytes.toString('ascii',binOffset+4,binOffset+8),'BIN\0');
    assert.equal(binOffset+8+bytes.readUInt32LE(binOffset),bytes.length);
    return json;
}

test('both local-first GPU scenes use identical compact procedural geometry',()=>{
    const dark=glb('brand-dark.glb');
    const light=glb('brand-light.glb');
    assert.equal(dark.meshes.length,9);
    assert.deepEqual(dark.meshes.map(m=>m.name),light.meshes.map(m=>m.name));
    assert.ok(dark.meshes.some(m=>m.name==='GlassHelixPrimary'));
    assert.ok(dark.meshes.some(m=>m.name==='GlassHelixSecondary'));
    assert.ok(dark.meshes.some(m=>m.name==='FloatingPearl'));
    assert.ok(dark.materials.some(m=>m.alphaMode==='BLEND'));
    assert.notDeepEqual(dark.materials[0].pbrMetallicRoughness.baseColorFactor,
                        light.materials[0].pbrMetallicRoughness.baseColorFactor);
    assert.ok(fs.statSync(path.join(dir,'assets/brand-dark.glb')).size<600_000);
});

test('Qt Quick 3D prototype keeps render, text overlay, interactions and accessibility separate',()=>{
    const qml=read('Prototype.qml');
    assert.match(qml,/import QtQuick3D\b/);
    assert.match(qml,/import QtQuick3D\.AssetUtils/);
    assert.match(qml,/View3D\s*\{/);
    assert.match(qml,/RuntimeLoader\s*\{/);
    assert.match(qml,/brand-dark\.glb/);
    assert.match(qml,/brand-light\.glb/);
    assert.match(qml,/property bool reducedMotion/);
    assert.match(qml,/onPositionChanged: function\(mouse\)/);
    assert.match(qml,/color: root\.accent/);
    assert.match(qml,/© 2026 Genyleap/);
    assert.match(qml,/Beta 1\.0/);
    assert.doesNotMatch(qml,/ShaderEffect\s*\{|Animation\.Infinite/);
});

test('browser GPU prototype is self-contained, respects preferences, and leaves real Setup untouched',()=>{
    const html=read('index.html');
    assert.match(html,/getContext\('webgl2'/);
    assert.match(html,/prefers-reduced-motion: reduce/);
    assert.match(html,/matchMedia\('\(prefers-reduced-motion: reduce\)'\)/);
    assert.match(html,/brand-dark\.glb/);
    assert.match(html,/brand-light\.glb/);
    assert.match(html,/id="dark"/);
    assert.match(html,/id="light"/);
    assert.match(html,/id="motion"/);
    assert.match(html,/© 2026 Genyleap/);
    assert.match(html,/https:\/\/genyleap\.com\//);
    assert.match(html,/noindex,nofollow/);
    assert.doesNotMatch(html,/https?:\/\/(?:unpkg|cdn|jsdelivr)/);
    assert.ok(fs.statSync(path.join(dir,'assets/genycaster-icon.svg')).size>300);
    const setup=fs.readFileSync(path.join(project,'ui/components/InitialSetup.qml'),'utf8');
    assert.doesNotMatch(setup,/SetupBrand3D|brand-dark\.glb|Brand3DPrototype/);
});
