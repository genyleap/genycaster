import test from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {spawnSync} from 'node:child_process';

const binary = resolve('build/local-pro-runtime/bin/Genycaster');
const available = existsSync(binary)
  && existsSync('/usr/bin/xvfb-run') && existsSync('/usr/bin/identify');

for (const configuration of [
  {name: 'Qt scene-graph software', env: {QT_QUICK_BACKEND: 'software'}},
  {name: 'Mesa OpenGL (CPU llvmpipe)', env: {
    QSG_RHI_BACKEND: 'opengl', LIBGL_ALWAYS_SOFTWARE: '1',
  }},
]) {
  test('the actual Linux GenyCaster window renders nonblank content: ' + configuration.name,
    {skip: !available, timeout: 30000}, () => {
      const home = mkdtempSync(join(tmpdir(), 'genycaster-visual-shell-'));
      const diagnostics = resolve('build/local-pro-runtime/diagnostics');
      mkdirSync(diagnostics, {recursive: true});
      const image = join(diagnostics, configuration.name.startsWith('Qt')
        ? 'home-software.png' : 'home-mesa.png');
      try {
        const result = spawnSync('/usr/bin/xvfb-run', [
          '-a', binary, '--visual-test', '--snapshot', image,
          '--snapshot-delay-ms=2800', '--width=1180', '--height=820',
          '--settings-path=' + join(home, 'settings.ini'),
        ], {encoding: 'utf8', timeout: 20000, env: {
          ...process.env, HOME: home, QT_QPA_PLATFORM: 'xcb',
          QML_DISABLE_DISK_CACHE: '1', ...configuration.env,
          LD_LIBRARY_PATH: [
            resolve('build/local-pro-runtime/bin'),
            '/opt/genycaster/Qt/6.12.0/gcc_64/lib',
            '/opt/gcc-16.2.0/lib64', process.env.LD_LIBRARY_PATH || '',
          ].filter(Boolean).join(':'),
        }});
        const output = (result.stderr || '') + '\n' + (result.stdout || '');
        assert.equal(result.status, 0, 'native visual startup: ' + output.slice(-5000));
        assert.ok(existsSync(image), 'window screenshot must exist');
        const count = spawnSync('/usr/bin/identify', ['-format', '%w %h %k', image],
          {encoding: 'utf8', timeout: 5000});
        assert.equal(count.status, 0, count.stderr);
        const [width, height, colors] = count.stdout.trim().split(/\s+/).map(Number);
        assert.ok(width >= 320 && height >= 240, 'unexpected viewport size');
        assert.ok(colors > 12, 'actual Qt Quick home was blank (' + colors + ' unique colors)');
      } finally {rmSync(home, {recursive: true, force: true});}
    });
}
