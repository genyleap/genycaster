// Build the immutable web host bundle from the pinned official SDK.
// The output is served from the GenyCaster origin, not a remote CDN.
import {build} from 'esbuild';
import {resolve} from 'node:path';

const [input, output] = process.argv.slice(2);
if (!input || !output) throw new Error('Usage: build.mjs <staged-entry> <output>');
await build({
  entryPoints:[resolve(input)],
  outfile:resolve(output),
  bundle:true, platform:'browser', format:'esm', target:'es2022',
  minify:true, legalComments:'inline', sourcemap:false
});
console.log('Built origin-scoped Farcaster Mini App host SDK asset');
