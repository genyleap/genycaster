import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const project=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const dir=path.join(project,'prototypes/setup-brand-3d');
const read=p=>fs.readFileSync(path.join(dir,p),'utf8');
const file=p=>fs.readFileSync(path.join(dir,p));

function glb(filename){
    const bytes=file('assets/'+filename);
    assert.equal(bytes.toString('ascii',0,4),'glTF');
    assert.equal(bytes.readUInt32LE(4),2);
    assert.equal(bytes.readUInt32LE(8),bytes.length);
    assert.equal(bytes.toString('ascii',16,20),'JSON');
    const size=bytes.readUInt32LE(12);
    const manifest=JSON.parse(bytes.subarray(20,20+size).toString('utf8'));
    assert.equal(manifest.asset.version,'2.0');
    const binOffset=20+size;
    assert.equal(bytes.toString('ascii',binOffset+4,binOffset+8),'BIN\0');
    assert.equal(binOffset+8+bytes.readUInt32LE(binOffset),bytes.length);
    return {manifest,bytes,bin:binOffset+8};
}

test('both themes provide the same 11 real 3D meshes with distinct PBR material sets',()=>{
    const dark=glb('brand-dark.glb').manifest;
    const light=glb('brand-light.glb').manifest;
    assert.equal(dark.meshes.length,11);
    assert.deepEqual(dark.meshes.map(x=>x.name),light.meshes.map(x=>x.name));
    for(const name of ['GlassPanel_Front','GlassPanel_Middle','GlassPanel_Back',
                        'SculpturalRibbon','FloatingGlassOrb','ReflectiveEdge_0']){
        assert.ok(dark.meshes.some(x=>x.name===name),name);
    }
    assert.ok(dark.materials.some(x=>x.alphaMode==='BLEND'));
    assert.notDeepEqual(dark.materials[0].pbrMetallicRoughness.baseColorFactor,
                        light.materials[0].pbrMetallicRoughness.baseColorFactor);
    for(const theme of ['dark','light'])assert.ok(file('assets/brand-'+theme+'.glb').length<1_500_000);
});

test('glass panel triangle indices are non-degenerate (prevent checkerboard holes)',()=>{
    for(const theme of ['dark','light']){
        const {manifest,bytes,bin}=glb('brand-'+theme+'.glb');
        const p=manifest.meshes[0].primitives[0];
        const pa=manifest.accessors[p.attributes.POSITION];
        const ia=manifest.accessors[p.indices];
        const posBase=bin+manifest.bufferViews[pa.bufferView].byteOffset;
        const idxBase=bin+manifest.bufferViews[ia.bufferView].byteOffset;
        assert.equal(ia.count%3,0);
        const xy=index=>{
            const off=posBase+index*12;
            return [bytes.readFloatLE(off),bytes.readFloatLE(off+4)];
        };
        for(let i=0;i<Math.min(ia.count,1200);i+=3){
            const tri=[0,1,2].map(j=>bytes.readUInt32LE(idxBase+(i+j)*4));
            assert.equal(new Set(tri).size,3,'duplicate vertex in triangle '+i/3);
            const [a,b,c]=tri.map(xy);
            const area=Math.abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2;
            assert.ok(area>0.01,'degenerate triangle '+i/3+' area '+area);
        }
    }
});

test('standalone Qt Quick 3D keeps copyright in accessible 2D layout',()=>{
    const qml=read('Prototype.qml');
    assert.match(qml,/import QtQuick3D\b/);
    assert.match(qml,/import QtQuick3D\.AssetUtils/);
    assert.match(qml,/View3D\s*\{/);
    assert.match(qml,/RuntimeLoader\s*\{/);
    assert.match(qml,/brand-dark\.glb/);
    assert.match(qml,/brand-light\.glb/);
    assert.match(qml,/studio-dark\.png/);
    assert.match(qml,/studio-light\.png/);
    assert.match(qml,/property bool reducedMotion/);
    assert.match(qml,/onPositionChanged: function\(mouse\)/);
    assert.match(qml,/© 2026 Genyleap/);
    assert.match(qml,/Beta 1\.0/);
    assert.doesNotMatch(qml,/Animation\.Infinite/);
});

test('browser rendering is fully local, GPU-based, interactive and theme-aware',()=>{
    const html=read('index.html');
    const js=read('scene.js');
    assert.match(html,/prefers-reduced-motion:reduce/);
    assert.match(js,/prefers-reduced-motion: reduce/);
    assert.match(js,/getContext\("webgl2"\)/);
    assert.match(js,/THREE\.MeshPhysicalMaterial/);
    assert.match(js,/THREE\.ShaderMaterial/);
    assert.match(js,/THREE\.PMREMGenerator/);
    assert.match(js,/studio-dark\.png/);
    assert.match(js,/studio-light\.png/);
    assert.match(html,/type="module" src="\.\/scene\.js"/);
    assert.match(js,/from "\.\/three\.module\.js"/);
    assert.match(html,/id="dark"/);
    assert.match(html,/id="light"/);
    assert.match(html,/id="motion"/);
    assert.match(html,/© 2026 Genyleap/);
    assert.match(html,/https:\/\/genyleap\.com\//);
    assert.match(html,/noindex,nofollow/);
    assert.doesNotMatch(js,/https?:\/\//);
    assert.match(read('THREE_LICENSE.txt'),/MIT License/);
    assert.match(read('three.module.js').slice(0,200),/Copyright 2010-2024 Three.js/);
    for(const t of ['dark','light']) {
        const png=file('assets/studio-'+t+'.png');
        assert.equal(png.toString('hex',0,8),'89504e470d0a1a0a');
    }
    const setup=fs.readFileSync(path.join(project,'ui/components/InitialSetup.qml'),'utf8');
    assert.doesNotMatch(setup,/SetupBrand3D|brand-dark\.glb|Brand3DPrototype/);
});
