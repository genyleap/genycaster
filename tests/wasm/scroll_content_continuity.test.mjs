import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';
const read = name => readFileSync(new URL('../../ui/components/' + componentPath(name) + '.qml', import.meta.url), 'utf8');
function method(source, name, context) {
    const match = source.match(new RegExp('(^[ ]*)function ' + name + '\\(', 'm'));
    assert.ok(match);
    const end = source.indexOf('\n' + match[1] + '}', match.index) + match[1].length + 2;
    return runInNewContext('(' + source.slice(match.index, end).trim() + ')', context);
}
const imageSource = read('SuperellipseImage');
function imageFixture(defer = false) {
    const root = {
        componentReady: true, scrollMoving: true, deferNewLoadsWhileScrolling: defer,
        effectiveSource: 'image://remote/new', activeSource: '',
        decodedReady: true, presentationReady: false, requiresFramePacedPresentation: true,
        presentationAdmissionPending: false, presentationGeneration: 0,
    };
    const sourceStartTimer = { restarts: 0, stops: 0,
        restart() { ++this.restarts; }, stop() { ++this.stops; } };
    const renderingController = { requests: [], requestMediaPresentation(owner, generation) {
        this.requests.push({owner, generation});
    } };
    const context = {root, sourceStartTimer, renderingController};
    root.synchronizeActiveSource = method(imageSource, 'synchronizeActiveSource', context);
    root.synchronizePresentation = method(imageSource, 'synchronizePresentation', context);
    const start = imageSource.indexOf('    onScrollMovingChanged: {');
    const end = imageSource.indexOf('    onDeferNewLoadsWhileScrollingChanged:', start);
    const onMotion = runInNewContext('(' + imageSource.slice(start, end)
        .replace('onScrollMovingChanged:', 'function onMotion()') + ')', context);
    return {...context, onMotion};
}
test('new media starts and requests frame-paced presentation during continuous scrolling', () => {
    const h = imageFixture();
    h.root.synchronizeActiveSource();
    assert.equal(h.root.activeSource, h.root.effectiveSource);
    h.root.synchronizePresentation();
    h.root.synchronizePresentation();
    assert.equal(h.renderingController.requests.length, 1, 'one request per presentation generation');
});
test('wheel bursts never detach an in-flight image, including explicit deferred surfaces', () => {
    for (const defer of [false, true]) {
        const h = imageFixture(defer);
        h.root.activeSource = h.root.effectiveSource;
        for (let i = 0; i < 10; ++i) {
            h.root.scrollMoving = !h.root.scrollMoving;
            h.onMotion();
            assert.equal(h.root.activeSource, h.root.effectiveSource);
        }
    }
});
test('reusing an image clears the previous identity before starting its replacement', () => {
    const h = imageFixture();
    h.root.activeSource = 'image://remote/old';
    h.root.synchronizeActiveSource();
    assert.equal(h.root.activeSource, '');
    assert.equal(h.sourceStartTimer.restarts, 1);
    h.root.synchronizeActiveSource();
    assert.equal(h.root.activeSource, 'image://remote/new');
});
test('optional request deferral still waits for rest without dropping decoded data', () => {
    const h = imageFixture(true);
    h.root.synchronizeActiveSource();
    assert.equal(h.root.activeSource, '');
    h.onMotion();
    assert.equal(h.sourceStartTimer.stops, 1);
    h.root.scrollMoving = false;
    h.onMotion();
    h.root.synchronizeActiveSource();
    assert.equal(h.root.activeSource, h.root.effectiveSource);
});
test('gallery and supporting tiles schedule once even while the viewport keeps moving', () => {
    for (const [file, tileName, timerName] of [
        ['GalleryLayout', 'tileSlot', 'tileAdmission'],
        ['CastEmbedsPanel', 'tile', 'previewAdmission'],
    ]) {
        const root = {scrollMoving: true, scrollHostResolved: true};
        const tile = {admitted: false};
        const timer = {running: false, starts: 0, restart() {this.running = true; ++this.starts;}};
        const schedule = method(read(file), 'scheduleAdmission', {root, [tileName]: tile, [timerName]: timer});
        schedule(); schedule();
        assert.equal(timer.starts, 1, file + ' does not starve or continually restart admission');
    }
});
test('payload refresh preserves admitted supporting subtree and new links wait until rest', () => {
    const cast = read('CastItem');
    assert.match(cast, /property bool supportingAdmitted: false/);
    assert.match(cast, /property string admittedSupportingIdentity: ""/);
    assert.match(cast, /onSupportingEmbedsChanged:/);
    assert.match(cast, /if \(root\.admittedSupportingIdentity !== identity\)/);
    assert.match(cast, /if \(!root\.supportingAdmitted && !root\.scrollMoving && root\.visible\)/);
    assert.match(cast, /active: !root\.mediaFirst && root\.supportingEmbeds\.length > 0/);
    const root = {
        supportingEmbeds: [{url: 'https://example.org/article'}],
        mediaFirst: true,
        supportingAdmitted: true,
        scrollMoving: true,
        visible: true,
    };
    const supportingAdmissionTimer = {
        starts: 0, stops: 0,
        start() { ++this.starts; }, stop() { ++this.stops; },
    };
    root.scheduleSupportingAdmission = method(cast, 'scheduleSupportingAdmission',
        {root, supportingAdmissionTimer});
    root.scheduleSupportingAdmission();
    assert.equal(root.supportingAdmitted, true);
    assert.equal(supportingAdmissionTimer.starts, 0,
        'a previously admitted preview must not be re-incubated during a flick');
    root.scrollMoving = false;
    root.scheduleSupportingAdmission();
    assert.equal(supportingAdmissionTimer.starts, 0,
        'counter refresh after a flick must retain the existing subtree');
    root.supportingAdmitted = false;
    root.scheduleSupportingAdmission();
    assert.equal(supportingAdmissionTimer.starts, 1,
        'only new supporting identities are scheduled when scrolling stops');
});
test('wallet picker applies mirroring to its content Item, never the Popup object', () => {
    const picker = read('WalletBrowserPicker');
    const popup = picker.slice(picker.indexOf('    GenyModal {'), picker.indexOf('        contentItem:'));
    assert.doesNotMatch(popup, /LayoutMirroring\./);
    assert.match(picker, /contentItem: ColumnLayout \{[\s\S]*?LayoutMirroring.enabled: false/);
    assert.match(picker, /layoutDirection: Theme.uiDirection/);
});
