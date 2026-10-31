import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
const src=fs.readFileSync(path.resolve(path.dirname(fileURLToPath(import.meta.url)),"../../ui/shaders/setup_glass_g.frag"),"utf8");

test("G and forward arrow share one extrusion and continuous bevel",()=>{
    assert.match(src,/const float halfDepth=\.118;/);
    assert.match(src,/const float bevel=\.019;/);
    assert.match(src,/vec2 g=vec2\(ring,abs\(p\.z\)-halfDepth\)/);
    assert.match(src,/vec2 a=vec2\(direction,abs\(p\.z\)-halfDepth\)/);
    assert.doesNotMatch(src,/p\.z-\.14/);
});
test("SDF uses an actual filleted union rather than touching separate components",()=>{
    assert.match(src,/const float fillet=\.072;/);
    assert.match(src,/float h=clamp\(\.5\+\.5\*\(dg-da\)\/fillet,0\.0,1\.0\)/);
    assert.match(src,/float merged=mix\(dg,da,h\)-fillet\*h\*\(1\.0-h\)/);
    assert.match(src,/float arrowTint=smoothstep\(\.06,\.94,h\)/);
    assert.match(src,/return vec2\(merged,arrowTint\)/);
    assert.doesNotMatch(src,/vec2\(min\(dg,da\), da<dg/);
});
test("pointer rotation and dark/light presentation remain GPU-driven",()=>{
    assert.match(src,/float orbitYaw/);
    assert.match(src,/float dragYaw/);
    assert.match(src,/float dragPitch/);
    assert.match(src,/surfaceNormal\(/);
    assert.match(src,/darkMode/);
    assert.match(src,/qt_Opacity/);
});
