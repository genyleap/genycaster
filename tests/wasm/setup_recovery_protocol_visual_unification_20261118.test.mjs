import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = p => readFileSync(new URL('../../' + p, import.meta.url), 'utf8');
const extract = (source, marker, next) => {
  const from = source.indexOf(marker);
  assert.ok(from !== -1, marker);
  const to = next ? source.indexOf(next, from + marker.length) : -1;
  return source.slice(from, to < 0 ? undefined : to);
};

test('Recovery follows one cross-platform Existing protocol picker', () => {
  const setup = read('ui/components/setup/InitialSetup.qml');
  const recover = extract(setup, 'id: signInStage', 'id: verifyingStage');
  const choice=extract(recover,'objectName: "setupExistingProtocolChoice"', 'objectName: "setupExistingSelectedProtocol"');
  for (const [name, value] of [
    ['setupExistingHyperSnap', 'hypersnap-v2'],
    ['setupExistingFarcaster', 'farcaster-snapchain'],
  ]) {
    const card=extract(choice,'objectName: "'+name+'"','root.existingProtocolSelected = true');
    assert.match(card,/onboardingStyle: true/);
    assert.match(card,/protocolChoice: true/);
    assert.ok(card.includes('root.recoveryProtocol = "' + value + '"'));
  }
  assert.match(recover,/setupEmphasis: true/);
  assert.match(recover,/root.existingProtocolSelected && !root.authenticated/);
  assert.match(recover,/root.recoverAccount\(root.recoveryProtocol\)/);
  assert.doesNotMatch(recover,/objectName: "setupRecover(?:HyperSnap|Farcaster)"/);
  assert.doesNotMatch(recover,/objectName: "setupBrowserRecover(?:HyperSnap|Farcaster)"/);
});

test('Provider registration cards share the same protocol selection material', () => {
  const setup = read('ui/components/setup/InitialSetup.qml');
  for(const name of ['setupRegisterHyperSnap', 'setupRegisterNeynar']) {
    const card = extract(setup, 'objectName: "' + name + '"', 'onClicked:');
    assert.match(card, /networkShowcase: true/);
    assert.match(card, /protocolChoice: true/);
  }
});

test('SetupActionCard single material owns geometry, strokes, fill and focus', () => {
  const card = read('ui/components/setup/SetupActionCard.qml');
  assert.match(card, /property bool protocolChoice: false/);
  assert.match(card, /background: NeonCard \{/);
  assert.match(card, /root.protocolChoice && root.selected\s*\?\s*\(root.down \? Theme.primarySurfaceHover : Theme.primarySurface\)/);
  assert.match(card, /root.protocolChoice \|\| root.setupEmphasis \|\| root.networkShowcase/);
  assert.match(card, /Theme.strokeHairline/);
  assert.match(card, /root.selected \? Theme.accent : "transparent"/);
  assert.match(card, /curveDepth: Theme.curveContainer/);
  assert.match(card, /highQualityEdges: true/);
  assert.match(card, /Accessible.role: root.protocolChoice \? Accessible.RadioButton : Accessible.Button/);
  assert.match(card, /focused: Theme.keyboardFocusVisible\(root\)/);
  assert.match(card, /externalHovered: root.hovered/);
  assert.match(card, /neonColor: root.visuallyDisabled[\s\S]*?root.primary \|\| root.setupEmphasis[\s\S]*?Qt.lighter\(Theme.primaryFill, 1.28\)/);
});
