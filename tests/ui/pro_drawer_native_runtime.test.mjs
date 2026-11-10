import test from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {spawnSync} from 'node:child_process';

// Runs the production Qt/QML application, not a hand-built Drawer mock.
// Uses an isolated visual fixture: no real accounts, payments or signer writes.
const binary = process.env.GENYCASTER_LOCAL_BINARY
  || resolve('build/local-pro-runtime/bin/Genycaster');

test('native Qt Pro drawer opens and closes six times without freezing the GUI loop',
  {timeout: 35000, skip: !existsSync(binary) || !existsSync('/usr/bin/xvfb-run')},
  () => {
    const home = mkdtempSync(join(tmpdir(), 'genycaster-pro-gui-smoke-'));
    try {
      const result = spawnSync('/usr/bin/xvfb-run', [
        '-a', binary, '--visual-test', '--exercise-pro-drawer',
        '--settings-path=' + join(home, 'prefs.ini'),
      ], {
        timeout: 25000, encoding: 'utf8',
        env: {
          ...process.env,
          HOME: home,
          QT_QPA_PLATFORM: 'xcb',
          QT_QUICK_BACKEND: 'software',
          QML_DISABLE_DISK_CACHE: '1',
          LD_LIBRARY_PATH: [
            resolve('build/local-pro-runtime/bin'),
            '/opt/genycaster/Qt/6.12.0/gcc_64/lib',
            '/opt/gcc-16.2.0/lib64',
            process.env.LD_LIBRARY_PATH || '',
          ].filter(Boolean).join(':'),
        },
      });
      const output = (result.stderr || '') + '\n' + (result.stdout || '');
      assert.equal(result.status, 0,
        'actual Pro GUI runtime did not complete (signal ' + result.signal + '):\n' + output.slice(-5000));
      assert.match(output, /Pro drawer: six open\/close cycles passed/);
      assert.doesNotMatch(output, /Binding loop detected|recursive rearrange/i);
    } finally {
      rmSync(home, {recursive: true, force: true});
    }
  });
