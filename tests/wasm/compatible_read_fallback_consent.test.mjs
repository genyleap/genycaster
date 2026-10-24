import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('selected read provider remains strictly pinned without user consent', () => {
    const main = read('ui/mobile/main.cpp');
    const composition = read('ui/mobile/runtime_composition.cpp');
    const bindings = read('ui/mobile/runtime_ui_bindings.cpp');
    const controller = read('ui/mobile/provider_profiles_controller.cpp');
    assert.match(main, /network\/allowCompatibleReadFallback/);
    assert.match(main, /GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK/);
    assert.match(main, /--allow-compatible-read-fallback/);
    assert.match(controller, /network\/allowCompatibleReadFallback/);
    assert.match(controller, /setCompatibleReadFallback\(const bool enabled\)/);
    assert.match(composition, /\.require = !allowCompatibleReadFallback/);
    assert.match(bindings, /\.require = !runtime->allowCompatibleReadFallback/);
    assert.match(composition, /\.require = true/);
    assert.doesNotMatch(controller, /protocol\.message\.submit|snapchainAuthorizationValue/);
});

test('read fallback consent is an explicit localized control; Write stays pinned', () => {
    const panel = read('ui/components/ProviderProfilesPanel.qml');
    const runtime = read('ui/mobile/runtime_composition.cpp');
    const core = read('tests/unit/core_tests.cpp');
    assert.match(panel, /providerProfiles\.setCompatibleReadFallback\(checked\)/);
    assert.match(panel, /genycaster\.providers\.compatible_read_fallback_description/);
    assert.match(runtime, /RoutingMode::StrictSelfHosted/);
    assert.match(runtime, /allowManagedProviders = !strict/);
    assert.match(core, /assert\(!router\.resolve\(indexRequest\)\)/);
    assert.match(core, /assert\(router\.resolve\(indexRequest\)->provider\.id\.value == "remote-beta"\)/);
    assert.match(core, /assert\(!strict\.resolve\(indexRequest\)\)/);

    for (const lang of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                         'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const ts = read('ui/i18n/qml_' + lang + '.ts');
        for (const id of ['compatible_read_fallback','compatible_read_fallback_description']) {
            assert.match(ts, new RegExp('<message id="genycaster\\.providers\\.'
                + id + '">[\\s\\S]*?<translation>[^<]+<\\/translation>'), lang + ': ' + id);
        }
    }
});
