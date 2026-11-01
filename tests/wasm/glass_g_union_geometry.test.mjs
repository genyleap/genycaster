import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync,statSync} from "node:fs";
import {fileURLToPath} from "node:url";
const path=new URL("../../ui/",import.meta.url);
const src=readFileSync(new URL("shaders/setup_glass_g.frag",path),"utf8");
const svg=readFileSync(new URL("wasm/genycaster-icon.svg",path),"utf8");
const generator=readFileSync(new URL("shaders/generate_logo_sdf.mjs",path),"utf8");

test("the exact master SVG G and directional mark supply GPU geometry",()=>{
 assert.match(svg,/<path fill="#FFFFFF" d="M 366 204/);
 assert.match(svg,/<path fill="#BEBEBE" d="M 254 231/);
 assert.match(generator,/const g=paths.find\(x=>x\[1\]==="#FFFFFF"\)/);
 assert.match(generator,/const arrow=paths.find\(x=>x\[1\]==="#BEBEBE"\)/);
 assert.match(generator,/new Path2D\(path\)/);
 assert.ok(statSync(new URL("shaders/genycaster_logo_sdf.png",path)).size>10000);
 assert.match(src,/uniform sampler2D logoDistanceMap/);
 assert.match(src,/vec3 canonicalDistance\(/);
 assert.match(src,/float merged=min\(dg,da\)/);
 assert.doesNotMatch(src,/sdTriangle|abs\(length\(p\.xy|fillet=\.072/);
});

test("extrusion bevel is inset rather than growing the official glyph silhouette",()=>{
 assert.match(src,/const float halfDepth=\.112/);
 assert.match(src,/const float bevel=\.007/);
 assert.match(src,/contour\.x\+bevel/);
 assert.match(src,/contour\.y\+bevel/);
 assert.match(src,/float dg=length\(max\(a,0\.0\)\)/);
 assert.match(src,/float da=length\(max\(b,0\.0\)\)/);
});
test("rotation, presentation and GPU texture all remain native Qt driven",()=>{
 const qml=readFileSync(new URL("components/InitialSetup.qml",path),"utf8");
 const cmake=readFileSync(new URL("CMakeLists.txt",path),"utf8");
 assert.match(qml,/property var logoDistanceMap: Image/);
 assert.match(qml,/shaders\/genycaster_logo_sdf\.png/);
 assert.match(cmake,/shaders\/genycaster_logo_sdf\.png/);
 assert.match(src,/float orbitYaw/);
 assert.match(src,/float dragYaw/);
 assert.match(src,/float dragPitch/);
 assert.match(qml,/id: automaticRotation/);
 assert.match(qml,/!Theme\.reduceMotion/);
});
test("studio lighting and fine normal field use canonical SVG geometry",()=>{
 assert.match(generator,/const fineDistance=6/);
 assert.match(generator,/Math\.min\(dsG,dsA\)/);
 assert.match(src,/vec3 canonicalDistance\(/);
 assert.match(src,/float nearSurface=1\.0-smoothstep/);
 assert.match(src,/uniform sampler2D logoNormalMap/);
 assert.match(src,/vec3 normalAt\(/);
 assert.match(src,/smoothstep\(\.101,\.109/);
 assert.ok(statSync(new URL("shaders/genycaster_logo_normals.png",path)).size>10000);
 assert.match(src,/vec3 studioEnv\(/);
 assert.match(src,/float roughness=mix/);
 assert.match(src,/float viewZoom/);
});
test("studio viewport supports reset, scroll zoom, auto-orbit and accessible controls",()=>{
 const qml=readFileSync(new URL("components/InitialSetup.qml",path),"utf8");
 assert.match(qml,/objectName: "gpuResetView"/);
 assert.match(qml,/objectName: "gpuAutoOrbit"/);
 assert.match(qml,/objectName: "gpuZoomOut"/);
 assert.match(qml,/objectName: "gpuZoomIn"/);
 assert.match(qml,/function resetView\(\)/);
 assert.match(qml,/function changeZoom\(steps\)/);
 assert.match(qml,/onWheel: function\(wheel\)/);
 assert.match(qml,/autoOrbitEnabled/);
 assert.match(qml,/viewZoom: desktopBrandPane\.viewZoom/);
 assert.match(qml,/Keys\.onPressed: function\(event\)/);
});
