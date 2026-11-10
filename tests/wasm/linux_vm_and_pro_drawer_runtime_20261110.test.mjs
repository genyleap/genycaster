import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');
const source = read('ui/app/main.cpp');
const drawer = read('ui/components/profile/GenyProDetailsDrawer.qml');
const smoke = read('ci/smoke_linux_install.sh');
const installer = read('properties/Windows/Configure.cmake');
const nsis = read('properties/Windows/NSISOptions.cmake.in');

test('Parallels Linux virtual GPU selects CPU renderer before Qt initializes', () => {
  const main = source.slice(source.indexOf('int main(int argc'), source.indexOf('QGuiApplication application(argc'));
  assert.match(main, /defined\(Q_OS_LINUX\)/);
  assert.match(main, /qEnvironmentVariableIsEmpty\("QT_QUICK_BACKEND"\)/);
  assert.match(main, /qEnvironmentVariableIsEmpty\("QSG_RHI_BACKEND"\)/);
  assert.match(main, /product_name/);
  assert.match(main, /sys_vendor/);
  assert.match(main, /toLower\(\)\.contains\("parallels"\)/);
  assert.match(main, /qputenv\("QSG_RHI_BACKEND", "opengl"\)/);
  assert.match(main, /qputenv\("LIBGL_ALWAYS_SOFTWARE", "1"\)/);
  assert.doesNotMatch(main, /qputenv\("QT_QUICK_BACKEND", "software"\)/);
  assert.match(smoke, /--visual-test --snapshot/);
  assert.match(smoke, /Black\/blank Qt Quick frame/);
  assert.match(smoke, /QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1/);
  assert.doesNotMatch(smoke, /QT_QUICK_BACKEND=software/);
});

test('Pro drawer never presents payment before FID-scoped entitlement is verified', () => {
  assert.match(drawer, /readonly property bool membershipVerified: membership.status === "verified"/);
  assert.match(drawer, /readonly property bool canRequestQuote: membershipVerified && !eligible/);
  assert.match(drawer, /property bool purchaseIntent: false/);
  assert.match(drawer, /!root.opened \|\| !root.purchaseIntent/);
  assert.match(drawer, /visible: !root.checkoutActive && !root.eligible && root.membershipVerified/);
  assert.match(drawer, /objectName: "genyProMembershipUnknownNotice"/);
  assert.match(drawer, /height: parent \? \(!root.compact \|\| root.fullScreen \? parent.height/);
  assert.match(drawer, /edge: root.compact \? Qt.BottomEdge : Qt.RightEdge/);
  assert.match(drawer, /interactive: false/);
  assert.match(drawer, /onClicked: root.cancelPayment\(\)/);
  assert.match(drawer, /onClosed:[\s\S]*?Qt.callLater\(\(\) => proCheckout.cancel\(\)\)/);
  const opened = drawer.slice(drawer.indexOf('onOpened: {'), drawer.indexOf('onClosed: {'));
  assert.doesNotMatch(opened, /retryProEntitlements\(\)/);
  assert.doesNotMatch(opened, /requestProPaymentQuote\(/);
});

test('Windows installer separates Welcome from GPL acceptance without forcing reboot', () => {
  assert.match(installer, /set\(CPACK_NSIS_WELCOME_TITLE/);
  assert.match(installer, /set\(CPACK_RESOURCE_FILE_LICENSE/);
  assert.match(installer, /file\(WRITE "\$\{_genycasterLicensePage\}" "\$\{_genycasterGplText\}"\)/);
  assert.doesNotMatch(nsis, /SetRebootFlag true/);
  assert.match(nsis, /StrCmp \$0 3010 genycaster_vc_runtime_done/);
  assert.match(nsis, /\/norestart/);
});
