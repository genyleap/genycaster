import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const theme = read('ui/theme/Theme.qml');

const tokens = {
  miniAppWebBackground: '#101018',
  featuredOverlayStart: '#22000000',
  featuredOverlayMiddle: '#bb101119',
  featuredOverlayEnd: '#e9101119',
  featuredTextInk: '#ffffff',
  featuredTextMuted: '#e4e5eb',
  nativeSelectionAccent: '#0A84FF',
  marketFearExtremeFear: '#E64F53',
  marketFearFear: '#F58A2D',
  marketFearNeutral: '#F9CA3A',
  marketFearGreed: '#8BCD34',
  marketFearExtremeGreed: '#26B98B',
  marketChangeDownInk: '#FF9EA4',
  marketChangeUpInk: '#6FDEA4',
  marketSeasonWeak: '#EF9022',
  marketSeasonCooling: '#F8C58E',
  marketSeasonNeutral: '#CCD3E2',
  marketSeasonWarming: '#91A5F4',
  marketTrendDownInk: '#FF9299',
  marketTrendUpInk: '#63D8A4',
  marketTrendDownSurface: '#3D232A',
  marketTrendUpSurface: '#1D3B31',
  dockDarkDisabledInk: '#747781',
  dockDarkSelectedInk: '#ffffff',
  dockDarkIdleInk: '#cbd0d9',
  dockSelectedWash: '#24FFFFFF',
  dockMaskWhite: '#FFFFFF',
  dockShadowTint: '#40000000',
  connectionIconBackground: '#FFFFFF',
  connectionIconBorder: '#E8EAEE',
  pulseTileEmpty: '#282A31',
  pulseFarcasterIconSurface: '#30283E',
  pulseFarcasterIconInk: '#C8B8F8',
  pulseUnobservedTile: '#20232B',
  pulseBrandIconBackground: '#FFFFFF',
  pulseGithubRangeBorder: '#BDECD1',
  feedbackMoodFrustrated: '#E66B66',
  feedbackMoodSad: '#E8A65D',
  feedbackMoodNeutral: '#D3AD55',
  feedbackMoodPositive: '#59B89A',
  feedbackMoodExcited: '#62B7C8',
  setupActionEmphasisInk: '#FCFCFE',
  setupActionEmphasisMutedInk: '#BABBC6',
  setupActionUnavailableSurface: '#23272E',
  setupActionEmphasisSurface: '#24252C',
  setupActionUnavailableBorder: '#3C444E'
};

const files = [
  'ui/app/miniapp/EmbeddedBrowser.qml',
  'ui/components/apps/FeaturedCarousel.qml',
  'ui/components/content/RichInlineText.qml',
  'ui/components/controls/BidiTextArea.qml',
  'ui/components/home/MarketFearGauge.qml',
  'ui/components/home/MarketPanel.qml',
  'ui/components/home/MarketSeasonMeter.qml',
  'ui/components/home/MarketTrend.qml',
  'ui/components/navigation/BottomNavButton.qml',
  'ui/components/navigation/BottomNavigation.qml',
  'ui/components/profile/ConnectionsPopup.qml',
  'ui/components/profile/GenyPulsePanel.qml',
  'ui/components/settings/FeedbackDialog.qml',
  'ui/components/setup/SetupActionCard.qml'
];

test('all moved palette roles exist exactly once and preserve their shipped values', () => {
  for (const [key, value] of Object.entries(tokens)) {
    const match = theme.match(new RegExp('readonly property color ' + key + ':([^\\n]+)'));
    assert.ok(match, 'theme role is missing: ' + key);
    assert.equal(theme.split('readonly property color ' + key + ':').length, 2,
      'theme role is declared once: ' + key);
    assert.ok(match[1].includes('"' + value + '"'), 'changed original role color: ' + key);
  }
  for (const key of ['githubContributionRamp', 'pulseGithubRamp', 'pulseFarcasterRamp']) {
    assert.equal(theme.split('readonly property var ' + key + ':').length, 2,
      'palette array declared exactly once: ' + key);
  }
  assert.match(theme, /githubContributionRamp:[\s\S]*?"#39D353"[\s\S]*?"#216E39"/);
  assert.match(theme, /pulseGithubRamp: \["#B5E8D0","#68CA9B","#32966C"\]/);
  assert.match(theme, /pulseFarcasterRamp: \["#C8B8F8","#9D81EA","#6552BD"\]/);
});

test('every audited QML surface uses semantic Theme roles rather than inline hexadecimal', () => {
  for (const file of files) {
    const source = read(file);
    assert.doesNotMatch(source, /#[0-9a-f]{6,8}/i, 'hard-coded color leaked in ' + file);
    assert.match(source, /Theme\.(?:[A-Za-z]+(?:Color|Ink|Surface|Tint|Accent|Ramp|Fear|Greed|Neutral|Border|Background|Wash|White|Muted|Stroke|End|Start|Middle|Weak|Cooling|Warming|Frustrated|Sad|Positive|Excited)[A-Za-z]*)/,
      'no semantic role used in ' + file);
  }
});

test('wallet and protocol authorizations do not depend on the visual palette', () => {
  const setup = read('ui/components/setup/SetupActionCard.qml');
  const browser = read('ui/app/miniapp/EmbeddedBrowser.qml');
  assert.match(setup, /readonly property bool visuallyDisabled: !enabled && !busy/);
  assert.match(browser, /settings\.javascriptCanOpenWindows: false/);
  assert.match(browser, /WebEngineProfile \{/);
});
