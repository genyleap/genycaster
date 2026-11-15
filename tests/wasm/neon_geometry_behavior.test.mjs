import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Execute the original Kit's self-contained geometry library, without its
// QML-only .pragma directive, to regression-test arc traversal and projection.
const code = readFileSync('ui/components/visual/NeonGeometry.js', 'utf8')
    .replace(/^\.pragma library\s*/m, '');
const geometry = vm.createContext({});
vm.runInContext(code, geometry, { filename: 'NeonGeometry.js' });

test('perimeter parametrization uses real rounded-square arc length', () => {
    const w = 214, h = 78, r = 14, n = 4;
    const perimeter = geometry.borderLength(w, h, r, n);
    assert.ok(Number.isFinite(perimeter) && perimeter > 2*(w-2*r)+2*(h-2*r));
    for (const [fraction, target] of [
        [0, { x:w/2, y:0 }], [0.25, { x:w, y:h/2 }],
        [0.5, { x:w/2, y:h }], [0.75, { x:0, y:h/2 }],
        [1, { x:w/2, y:0 }],
    ]) {
        const p = geometry.borderPoint(fraction, w, h, r, n);
        assert.ok(Math.abs(p.x-target.x) < 0.15, 'x at '+fraction);
        assert.ok(Math.abs(p.y-target.y) < 0.15, 'y at '+fraction);
    }
});

test('pointer projection is continuous near straight-to-curved boundaries', () => {
    const w = 240, h = 110, r = 22, n = 4;
    for (let x = -12; x <= w+12; x+=2) {
        const point = geometry.nearestBorderPoint(x, 3, w, h, r, n);
        assert.ok(Number.isFinite(point.x) && Number.isFinite(point.y));
        assert.ok(Number.isFinite(point.distance) && point.distance >= 0);
        assert.ok(point.x >= 0 && point.x <= w);
        assert.ok(point.y >= 0 && point.y <= h);
        const exact = Math.hypot(x-point.x, 3-point.y);
        assert.ok(Math.abs(point.distance-exact) < 1e-7);
    }
});

test('activity orbit has no teleport/jitter across 512 samples', () => {
    for(const [w,h,r] of [[214,78,14],[46,26,13],[420,380,28],[82,44,22]]) {
        const length = geometry.borderLength(w,h,r,4);
        let last = geometry.borderPoint(0,w,h,r,4);
        for(let i=1;i<=512;i++) {
            const next = geometry.borderPoint(i/512,w,h,r,4);
            assert.ok(next.x >= -1e-6 && next.x <= w+1e-6);
            assert.ok(next.y >= -1e-6 && next.y <= h+1e-6);
            assert.ok(Math.hypot(next.x-last.x,next.y-last.y) < length/512*2.2);
            last=next;
        }
    }
});
