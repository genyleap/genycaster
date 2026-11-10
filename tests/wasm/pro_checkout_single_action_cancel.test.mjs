import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const drawer = readFileSync('ui/components/profile/GenyProDetailsDrawer.qml', 'utf8');

test('reading a Pro membership never silently opens payment or signs a cloud session', () => {
  assert.match(drawer, /property bool purchaseIntent: false/);
  assert.match(drawer, /if \(!root.opened \|\| !root.purchaseIntent/);
  assert.match(drawer, /onOpened:\s*\{[\s\S]*?purchaseIntent = false/);
  assert.doesNotMatch(drawer, /onOpened:\s*\{[^}]*requestCurrentQuote\(\)/);
  assert.match(drawer, /if \(!root.membershipVerified\)[\s\S]*?fetchProEntitlements\(true\)/);
  assert.match(drawer, /if \(!root.checkoutActive\) \{[\s\S]*?root.purchaseIntent = true/);
  assert.doesNotMatch(drawer, /onPaymentQuoteChanged:[\s\S]*?proCheckout.pay\(\)/);
});

test('cancellation is explicit, cancels pending local wallet session and retains on-chain verification', () => {
  assert.match(drawer, /objectName: "genyProCancelCheckout"/);
  assert.match(drawer, /text: qsTrId\("genycaster.wallet.cancel"\)/);
  assert.match(drawer, /function cancelPayment\(\)[\s\S]*?root.close\(\)/);
  assert.match(drawer, /onClosed:[\s\S]*?Qt.callLater\(\(\) => proCheckout.cancel\(\)\)/);
  assert.match(drawer, /onClosed:[\s\S]*?proCheckout.cancel\(\)/);
  assert.match(drawer, /objectName: "genyProChangePaymentMethod"/);
});
