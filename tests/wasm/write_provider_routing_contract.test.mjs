import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('separate validator URL is persisted, validated, and passed to the Qt composition', () => {
    const main = read('ui/app/main.cpp');
    const composition = read('ui/app/runtime_composition.cpp');
    const controller = read('src/providers/hypersnap/node_selection_controller.cpp');
    const settings = read('ui/components/LocaleSettingsPopup.qml');
    const nodes = read('ui/components/NodeManagementPanel.qml');

    assert.match(main, /--snapchain-write-base-url=/);
    assert.match(main, /GENYCASTER_SNAPCHAIN_WRITE_BASE_URL/);
    assert.match(main, /network\/snapchainWriteBaseUrl/);
    assert.match(main, /isValidHttpServiceUrl\(snapchainWriteBaseUrlText\)/);
    assert.match(main, /snapchainBaseUrl, snapchainWriteBaseUrl,/);
    assert.match(composition, /signerRegistryConfig\.submitBaseUrl = \*snapchainWriteBaseUrl/);
    assert.match(composition, /writeSubmitterConfig\.baseUrl = \*snapchainWriteBaseUrl/);
    assert.match(controller, /bool NodeSelectionController::setSnapchainWriteEndpoint/);
    assert.match(controller, /m_settings\.sync\(\)/);
    assert.match(settings, /NodeManagementPanel \{/);
    assert.match(nodes, /providerProfiles\.addWriteProfile\(id, writeName\.text\.trim\(\),/);
    assert.match(nodes, /"snapchain-v1", writeEndpoint\.text\.trim\(\)/);
    assert.match(nodes, /providerProfiles\.writeRestartRequired/);
});

test('no read-node API credentials are forwarded to another write endpoint', () => {
    const composition = read('ui/app/runtime_composition.cpp');
    const signer = read('src/providers/snapchain/signer.cpp');
    assert.match(composition, /snapchainWriteBaseUrl == snapchainBaseUrl/);
    assert.match(signer, /trimTrailingSlashes\(config\.submitBaseUrl\) == trimTrailingSlashes\(config\.baseUrl\)/);
    assert.match(signer, /config\.submitAuthorizationHeaderName/);
    assert.match(signer, /config\.submitAuthorizationHeaderValue/);
    assert.match(signer, /config\.submitBaseUrl\.empty\(\) \? config\.baseUrl : config\.submitBaseUrl/);
});

test('all bundled languages contain the validator write settings copy', () => {
    const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    const ids = [
        'genycaster.settings.validator_write_endpoint',
        'genycaster.settings.validator_write_description',
        'genycaster.settings.validator_write_invalid',
    ];
    for (const locale of locales) {
        const catalog = read('ui/i18n/qml_' + locale + '.ts');
        for (const id of ids) {
            assert.match(catalog, new RegExp('<message id="' + id.replace(/\./g,'\\.') + '">'));
        }
    }
});

test('Qt HTTP transport does not follow credentialed or write redirects', () => {
    const sync = read('src/transport/qt_network/http.cpp');
    const asyncTransport = read('src/transport/qt_network/async_http.cpp');
    assert.match(sync, /requestBody != nullptr \|\| carriesCustomHeaders/);
    assert.match(sync, /QNetworkRequest::ManualRedirectPolicy/);
    assert.match(asyncTransport, /credentialedApiRequest \? QNetworkRequest::ManualRedirectPolicy/);
    assert.match(asyncTransport, /name != "accept" && name != "content-type"/);
});
