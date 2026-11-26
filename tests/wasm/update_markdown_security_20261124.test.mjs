import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('static WASM Lottie registers both runtime-generated QML dependencies',()=>{
    const cmake=read('ui/CMakeLists.txt');
    assert.match(cmake,/Qt6::qtquicktimelineplugin/);
    assert.match(cmake,/Qt6::LottieVectorImageHelpersplugin/);
    assert.match(cmake,/find_package\(Qt6 REQUIRED COMPONENTS QuickTimeline LottieVectorImageHelpers\)/);
});

test('WASM browser update revalidates security-critical JS instead of replaying stale signer',()=>{
    const worker=read('ui/wasm/genycaster-sw.js');
    const vault=read('ui/wasm/secure_signer_vault.mjs');
    assert.match(worker,/const CACHE="genycaster-static-v17"/);
    assert.match(worker,/fetch\(request,\{cache:"no-cache"\}\)/);
    assert.match(vault,/ArrayBuffer\.isView\(challenge\)/);
    assert.match(vault,/size > 4096/);
    assert.match(vault,/media challenge signer key mismatch/);
});

test('native update availability is a verified install action, never a GitHub CTA',()=>{
    const controller=read('ui/app/update/controller.cpp');
    const notice=read('ui/components/settings/UpdateNoticeDrawer.qml');
    const settings=read('ui/components/settings/SettingsPopup.qml');
    assert.match(controller,/!safeDigest\(digest\)/);
    assert.match(controller,/macos-/);
    assert.match(controller,/adhoc-unnotarized\.dmg/);
    assert.match(controller,/matchingAsset\(name\)/);
    assert.match(controller,/downloadUpdate\(\)/);
    assert.match(notice,/const actionable = Qt\.platform\.os === "wasm"/);
    assert.match(notice,/updateController\.canDownload/);
    assert.match(notice,/updateController\.installDownloadedUpdate\(\)/);
    assert.match(notice,/updateController\.downloadUpdate\(\)/);
    assert.doesNotMatch(notice,/updateController\.openReleasePage\(\)/);
    assert.doesNotMatch(settings,/updateController\.openReleasePage\(\)/);
    assert.match(settings,/GenyProgressBar \{/);
});

test('shared official reading surfaces render markdown with secure HTTPS links and spacing',()=>{
    const doc=read('ui/components/content/ReadableMarkdown.qml');
    const notes=read('ui/components/settings/UpdateNoticeDrawer.qml');
    const settings=read('ui/components/settings/SettingsPopup.qml');
    const setup=read('ui/components/setup/InitialSetup.qml');
    assert.match(doc,/textFormat: Text\.MarkdownText/);
    assert.match(doc,/onLinkActivated: function\(link\)/);
    assert.match(doc,/^.*https:.*$/m);
    assert.match(doc,/if \(\/\^#\{1,6\}/);
    assert.match(doc,/lineHeight: 1\.48/);
    assert.match(setup,/id: legalBody[\s\S]*?markdownSource:/);
    assert.match(notes,/id: releaseNotesContent[\s\S]*?ReadableMarkdown \{/);
    assert.match(settings,/markdownSource: LocaleFormat\.localizeDigits\(/);
    assert.doesNotMatch(notes,/releaseNotesText.*Text\.PlainText/);
});

test('HTML fallback update dialog uses semantic list elements and safe text-only rendering',()=>{
    const html=read('ui/wasm/genycaster-shell.html');
    const loader=read('ui/wasm/genycaster-loader.js');
    assert.match(html,/<div id="genycaster-update-details"><p>/);
    assert.match(html,/\.update-details ul\{[^}]*display:grid;gap:11px/);
    assert.match(html,/\.update-details li\{[^}]*line-height:1\.6/);
    assert.match(loader,/document\.createElement\("ul"\)/);
    assert.match(loader,/document\.createElement\("li"\)/);
    assert.match(loader,/entry\.textContent=localizedUpdateDigits/);
    assert.match(loader,/details\.replaceChildren\(\)/);
    assert.doesNotMatch(loader,/details\.innerHTML\s*=/);
    assert.match(loader,/updateBanner\.dir=.*"rtl"/);
});
