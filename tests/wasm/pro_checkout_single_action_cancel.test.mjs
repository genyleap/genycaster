import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const drawer = readFileSync('ui/components/profile/GenyProDetailsDrawer.qml', 'utf8');

test('a prefetched quote lets the first Continue initiate the wallet directly', () => {
  assert.match(drawer, /readonly property bool quoteForSelection: quotePresent && quoteAsset === selectedAsset && quoteValid/);
  assert.match(drawer, /onOpened:\s*\{[\s\S]*?prepareQuote\(\)/);
  assert.match(drawer, /onSelectedAssetChanged:[\s\S]*?prepareQuote\(\)/);
  assert.match(drawer, /if \(!root.checkoutActive\) \{[\s\S]*?if \(!root.quoteForSelection\)[\s\S]*?root.checkoutOpen = true[\s\S]*?proCheckout.pay\(\)/);
  assert.doesNotMatch(drawer, /onPaymentQuoteChanged:[\s\S]*?proCheckout.pay\(\)/);
});

test('cancellation is explicit, cancels pending local wallet session and retains on-chain verification', () => {
  assert.match(drawer, /objectName: "genyProCancelCheckout"/);
  assert.match(drawer, /text: qsTrId\("genycaster.wallet.cancel"\)/);
  assert.match(drawer, /function cancelPayment\(\)[\s\S]*?if \(!root.txSubmitted && proCheckout\) proCheckout.cancel\(\)/);
  assert.match(drawer, /onClosed:[\s\S]*?proCheckout.cancel\(\)/);
  assert.match(drawer, /objectName: "genyProChangePaymentMethod"/);
});
