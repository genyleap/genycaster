import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../../ui/components/visual/SuperellipseImage.qml',import.meta.url),'utf8');

test('AnimatedImage uses validated first-party proxy for CDN media1 GIFs across platforms',()=>{
 assert.match(source,/Qt\.platform\.os === 'wasm'[\s\S]*?media1\\\.tenor/);
 assert.match(source,/deploymentEndpoints\.browserGateway \+ "\/v1\/browser\/images\?url="/);
 assert.match(source,/encodeURIComponent\(mediaUrl\)/);
});

test('GIF requests respect policy and use still-image fallback without implicit HEAD',()=>{
 assert.match(source,/source: root\.componentReady && root\.hasRenderableGeometry/);
 assert.match(source,/root\.automaticDownloadAllowed/);
 assert.match(source,/root\.activeSource\.toString\(\)\.length > 0/);
 assert.match(source,/visible: root\.animatedGifUrl\.length > 0 && status === AnimatedImage\.Ready/);
 assert.match(source,/Image \{\s+id: imageSource/);
});
