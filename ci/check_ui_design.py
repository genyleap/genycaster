#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import json, re, sys
from qml_delimiter_balance import parentheses_balanced

root = Path(__file__).resolve().parents[1]
ui = root / 'ui'
cmake = (ui / 'CMakeLists.txt').read_text(encoding='utf-8')
# Qt's default 50 MB WASM initial heap cannot link this application's
# statically bundled QML/Qt Quick resources (observed requirement: ~54.6 MiB).
# The target property is consumed by Qt's finalization; a second linker flag
# would be overridden by Qt's later default option.
if not re.search(
    r'if\(EMSCRIPTEN\).*?set_target_properties\(genycaster PROPERTIES\s+'
    r'QT_WASM_INITIAL_MEMORY\s+"128MB"\)',
    cmake,
    re.S,
):
    raise SystemExit('WASM target must request a 128 MB Qt initial heap')
if '-sINITIAL_MEMORY=' in cmake or '-s INITIAL_MEMORY=' in cmake:
    raise SystemExit('Set QT_WASM_INITIAL_MEMORY instead of duplicate em++ memory flags')
match = re.search(r'set\(farcasterQmlFiles\n(.*?)\n\)', cmake, re.S)
if not match:
    raise SystemExit('farcasterQmlFiles not found')
registered = sorted(line.strip() for line in match.group(1).splitlines() if line.strip())
# Platform-only QML types are registered conditionally in ui/CMakeLists.txt:
# Lottie requires its addon, while Android's system WebView has no desktop
# equivalent. The *source* manifest checks both without forcing them into
# the wrong target's deployable resource table.
registered = sorted(set(registered) | {
    'components/media/AnimatedEmojiLottie.qml',
    'components/apps/AndroidWebView.qml',
})
# The Linux QtWebEngine Quick drawer lives in a separate resource target.
# It must never be registered in the cross-platform Farcaster.Client QML module.
discovered = sorted(str(p.relative_to(ui)) for p in ui.rglob('*.qml')
                    if p.relative_to(ui).as_posix() != 'app/miniapp/EmbeddedBrowser.qml')
errors: list[str] = []
if registered != discovered:
    errors.append(f'QML manifest mismatch: missing={sorted(set(discovered)-set(registered))}, extra={sorted(set(registered)-set(discovered))}')

# The native QML import smoke suite source-loads public QML aliases from this
# mapping. Missing an entry can let a broken component ship even when the
# production CMake list is complete (e.g. ProTokenMark in GenyProDetailsDrawer).
source_map = json.loads((ui / 'components' / 'source_paths.json').read_text(encoding='utf-8'))
source_components = {
    str(path.relative_to(ui / 'components'))
    for path in (ui / 'components').rglob('*.qml')
    if path.name != 'Main.qml'
}
mapped_components = set(source_map.values())
if mapped_components != source_components or len(source_map) != len(mapped_components):
    errors.append('Native QML source map mismatch: '
                  f'missing={sorted(source_components-mapped_components)}, '
                  f'extra={sorted(mapped_components-source_components)}, '
                  f'duplicate={len(source_map)-len(mapped_components)}')

controls = {
    'AbstractButton','Action','ActionGroup','ApplicationWindow','BusyIndicator','Button','ButtonGroup',
    'CheckBox','CheckDelegate','ComboBox','Container','Control','DelayButton','Dial','Dialog','DialogButtonBox',
    'Drawer','Frame','GroupBox','ItemDelegate','Label','Menu','MenuBar','MenuBarItem','MenuItem','MenuSeparator',
    'Page','PageIndicator','Pane','Popup','ProgressBar','RadioButton','RadioDelegate','RangeSlider','RoundButton',
    'ScrollBar','ScrollIndicator','ScrollView','Slider','SpinBox','SplitView','StackView','SwipeDelegate','SwipeView',
    'Switch','SwitchDelegate','TabBar','TabButton','TextArea','TextField','ToolBar','ToolButton','ToolSeparator','ToolTip','Tumbler'
}
def component_source_path(name):
    import json
    mapping=json.loads((ui/'components'/'source_paths.json').read_text(encoding='utf-8'))
    return ui/'components'/mapping[name]

for p in (ui/'components').rglob('*.qml'):
    if p.stem in controls:
        errors.append(f'Local QML type collides with QtQuick.Controls: {p.name}')

theme_contract_text = (ui / 'theme' / 'Theme.qml').read_text(encoding='utf-8')
theme_contract_symbols = set(re.findall(r'\bproperty\s+\w+\s+(\w+)\s*:', theme_contract_text))
theme_contract_symbols.update(re.findall(r'\bfunction\s+(\w+)\s*\(', theme_contract_text))

for p in ui.rglob('*.qml'):
    text = p.read_text(encoding='utf-8')
    rel = p.relative_to(root)
    for theme_symbol in sorted(set(re.findall(r'\bTheme\.(\w+)', text))):
        if theme_symbol not in theme_contract_symbols:
            errors.append(f'Unknown Theme token Theme.{theme_symbol}: {rel}')
    if re.search(r'\bradius\s*:', text):
        errors.append(f'Legacy Rectangle.radius style found: {rel}')
    if 'BusyIndicator {' in text:
        errors.append(f'Native BusyIndicator visual found: {rel}')
    if 'ToolTip.' in text:
        errors.append(f'Native ToolTip visual found: {rel}')
    if 'ScrollIndicator.vertical:' in text or 'ScrollBar.vertical: ScrollBar {' in text:
        errors.append(f'Native scroll chrome found: {rel}')
    if 'control.mirrored' in text:
        errors.append(f'Control mirroring used instead of semantic direction: {rel}')
    if p.name != 'Theme.qml' and re.search(r'#[0-9A-Fa-f]{6,8}', text):
        errors.append(f'Hard-coded color outside Theme.qml: {rel}')
    if re.search(r'font\.pixelSize\s*:\s*\d', text):
        errors.append(f'Raw font size outside semantic type tokens: {rel}')
    if '\\u200E' in text or '\\u200F' in text:
        errors.append(f'Literal bidi mark found; use an explicit helper instead: {rel}')
    # Duplicate property assignments in one QML object are runtime/parser defects,
    # not stylistic warnings. Keep this deliberately small and line-oriented.
    property_scopes: list[dict[str, int]] = [{}]
    qml_object_scopes: list[str | None] = [None]
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        scan_line = raw_line.split('//', 1)[0]
        property_match = re.match(r'^\s*([A-Za-z_][\w]*(?:\.[A-Za-z_][\w]*)*)\s*:', scan_line)
        if property_match:
            property_name = property_match.group(1)
            current_scope = property_scopes[-1]
            if property_name in current_scope:
                errors.append(
                    f'Duplicate QML property assignment {property_name}: {rel}:{line_number} '
                    f'(first at line {current_scope[property_name]})')
            else:
                current_scope[property_name] = line_number
            if qml_object_scopes[-1] == 'Loader' and property_name in {'implicitWidth', 'implicitHeight'}:
                errors.append(
                    f'Loader {property_name} is read-only; rely on Loader intrinsic sizing or Layout.*: '
                    f'{rel}:{line_number}')

        object_match = re.match(r'^\s*([A-Z][A-Za-z0-9_.]*)\s*\{', scan_line)
        object_type = object_match.group(1).split('.')[-1] if object_match else None
        first_open = True
        for character in scan_line:
            if character == '{':
                property_scopes.append({})
                qml_object_scopes.append(object_type if first_open else None)
                first_open = False
            elif character == '}' and len(property_scopes) > 1:
                property_scopes.pop()
                qml_object_scopes.pop()

    # Every project-owned SuperellipseImage must state its image fitting policy
    # explicitly. Its default is PreserveAspectCrop for legacy cover surfaces,
    # but content previews, app icons, avatars, banners and thumbnails have
    # different semantics. Requiring fillMode at each call site prevents a
    # future refactor from silently cropping user-visible content.
    for image_match in re.finditer(r'\bSuperellipseImage\s*\{', text):
        block_start = image_match.end() - 1
        depth = 0
        block_end = None
        for offset in range(block_start, len(text)):
            if text[offset] == '{':
                depth += 1
            elif text[offset] == '}':
                depth -= 1
                if depth == 0:
                    block_end = offset + 1
                    break
        if block_end is None:
            continue
        image_block = text[image_match.start():block_end]
        if 'fillMode:' not in image_block:
            line_number = text[:image_match.start()].count('\n') + 1
            errors.append(f'SuperellipseImage must declare an explicit fillMode: {rel}:{line_number}')

    # Lightweight raw delimiter balance catches accidental edit damage.
    if text.count('{') != text.count('}'):
        errors.append(f'Brace imbalance: {rel}')
    if not parentheses_balanced(text):
        errors.append(f'Parenthesis imbalance: {rel}')
    if text.count('[') != text.count(']'):
        errors.append(f'Bracket imbalance: {rel}')

    # A vertical ScrollView with its horizontal scrollbar hidden must also lock
    # contentWidth to the viewport. Otherwise trackpad/horizontal wheel deltas
    # can pan an oversized implicit child even though no horizontal affordance is
    # visible, producing apparently random left/right layout drift. Code viewers
    # that explicitly switch to ScrollBar.AsNeeded are intentionally excluded.
    for scroll_match in re.finditer(r'\bScrollView\s*\{', text):
        block_start = scroll_match.end() - 1
        depth = 0
        block_end = None
        for offset in range(block_start, len(text)):
            if text[offset] == '{':
                depth += 1
            elif text[offset] == '}':
                depth -= 1
                if depth == 0:
                    block_end = offset + 1
                    break
        if block_end is None:
            continue
        scroll_block = text[scroll_match.start():block_end]
        if ('ScrollBar.horizontal:' in scroll_block
                and 'ScrollBar.AlwaysOff' in scroll_block
                and 'ScrollBar.AsNeeded' not in scroll_block
                and 'contentWidth:' not in scroll_block):
            line_number = text[:scroll_match.start()].count('\n') + 1
            errors.append(
                f'Vertical ScrollView hides horizontal chrome without locking contentWidth: '
                f'{rel}:{line_number}')


# Latin protocol/content text must never inherit the FaNum glyph family.
theme_text = (ui/'theme'/'Theme.qml').read_text(encoding='utf-8')
if 'latinFontFamily' not in theme_text or 'persianFontFamily' not in theme_text:
    errors.append('Theme must expose separate Latin and Persian font families')
direction_label = (ui/'components'/'controls'/'BidiLabel.qml').read_text(encoding='utf-8')
if 'Theme.fontFamilyForText' not in direction_label:
    errors.append('DirectionAwareLabel must select a script-capable font family from its own content')
main_cpp = (ui/'app'/'main.cpp').read_text(encoding='utf-8')
if 'application.setFont' in main_cpp and 'IRANSansXFaNum' in main_cpp:
    errors.append('Application-wide FaNum font forces Persian digit glyphs into Latin content')

# UI strings must not bake arrow glyphs into labels that also render an icon.
for ts_name in ('qml_en_US.ts', 'qml_fa_IR.ts'):
    ts_text = (ui/'i18n'/ts_name).read_text(encoding='utf-8')
    if any(glyph in ts_text for glyph in ('←', '→', '⇐', '⇒', '⬅', '➡')):
        errors.append(f'Decorative arrow glyph found in translation catalog: {ts_name}')


# Components that claim shape-aware hit testing must expose a shape, not a rectangular Item wrapper.
tab_button = (ui/'components'/'controls'/'GenyTabButton.qml').read_text(encoding='utf-8')
if ('containmentMask: background' not in tab_button
        or 'background: NeonStatePlate' not in tab_button
        or 'cornerRadius: Theme.curveControl' not in tab_button
        or 'property bool filledTabs: false' not in tab_button
        or 'selected: control.filledTabs && control.selected' not in tab_button
        or 'neonOnHover: control.filledTabs' not in tab_button
        or 'hoverFill: control.filledTabs ? Theme.surfaceHover : "transparent"' not in tab_button):
    errors.append('GenyTabButton must preserve curved hit testing and opt-in composer backgrounds')

superellipse = (ui/'components'/'visual'/'Superellipse.qml').read_text(encoding='utf-8')
geny_spinner = (ui/'components'/'controls'/'GenySpinner.qml').read_text(encoding='utf-8')
if 'containmentMask: renderedShape' in superellipse:
    errors.append('Superellipse hit testing must stay on lightweight QQuickItem bounds; shared controls must not install the visual renderer as a containment mask')
if 'cornerSamples: 96' not in superellipse:
    errors.append('Superellipse sampling quality regression')
if ('SceneGraphSuperellipse {' not in superellipse
        or 'id: renderedShape' not in superellipse
        or 'qualityScale:' not in superellipse
        or 'property bool highQualityEdges: false' not in superellipse
        or 'visible: !root.highQualityEdges' not in superellipse
        or 'active: root.highQualityEdges' not in superellipse
        or 'preferredRendererType: Shape.CurveRenderer' not in superellipse):
    errors.append('Shared Superellipse must default to retained QSG geometry and expose only an explicit CurveRenderer precision opt-in')
if ('readonly property real edgeGuard:' not in superellipse
        or 'saturatesWidth' not in superellipse
        or 'saturatesHeight' not in superellipse):
    errors.append('CurveRenderer pill/circle precision path must guard against zero-length edge joins')
if ('readonly property bool animationRequested:' not in geny_spinner
        or 'id: animationGraceStop' not in geny_spinner
        or 'running: root.animationActive' not in geny_spinner
        or 'duration: Theme.reduceMotion ? 1400 : 760' not in geny_spinner):
    errors.append('GenySpinner must retain its grace period and keep essential loading motion under Reduce Motion')

scene_graph_stroke = (ui/'render'/'stroke.cpp').read_text(encoding='utf-8')
scene_graph_surface = (ui/'render'/'superellipse.cpp').read_text(encoding='utf-8')
if ('EdgeGuardPhysicalPixels' not in scene_graph_surface
        or 'saturatesWidth' not in scene_graph_surface
        or 'saturatesHeight' not in scene_graph_surface):
    errors.append('SceneGraphSuperellipse pill/circle paths must guard zero-length feather joins')

for renderer_name, renderer_text in (
    ('SceneGraphStroke', scene_graph_stroke),
    ('SceneGraphSuperellipse', scene_graph_surface),
):
    if 'updatePaintNode' not in renderer_text or 'QSGGeometryNode' not in renderer_text:
        errors.append(f'{renderer_name} must render through QQuickItem::updatePaintNode and QSGGeometryNode')
    if 'QQuickPaintedItem' in renderer_text:
        errors.append(f'{renderer_name} must never fall back to QQuickPaintedItem')
    if 'QSGMaterial::Blending' not in renderer_text:
        errors.append(f'{renderer_name} must enable premultiplied-alpha blending for feathered HiDPI edges')
if 'render/stroke.cpp' not in cmake or 'render/superellipse.cpp' not in cmake:
    errors.append('Scene Graph renderer sources must remain registered in the Farcaster.Client QML module')

# Runtime-sensitive contracts discovered through real Qt 6.11 execution.
# Popup itself is not a QQuickItem, so item-only attached properties must live
# on its content item rather than the GenyModal root object.
locale_popup = (ui/'components'/'settings'/'SettingsPopup.qml').read_text(encoding='utf-8')
locale_root_prefix = locale_popup.split('contentItem:', 1)[0]
# Attached properties on nested visual children are valid. Reject them only
# at the root Popup indentation, where QObject lacks a QQuickItem surface.
if re.search(r'^    Accessible\.', locale_root_prefix, re.M):
    errors.append('LocaleSettingsPopup attaches Accessible to Popup instead of its content Item')
if re.search(r'^    LayoutMirroring\.', locale_root_prefix, re.M):
    errors.append('LocaleSettingsPopup attaches LayoutMirroring to Popup instead of its content Item')
if re.search(r'Layout\.(?:maximumWidth|minimumWidth|preferredWidth)\s*:\s*[^\n]*parent\.width', locale_popup):
    errors.append('LocaleSettingsPopup Layout width constraints must not derive from their RowLayout parent width')
if 'DisplayScaleControl {' not in locale_popup:
    errors.append('Display settings must use the dedicated Genycaster scale control rather than stock slider rows')
if ('component SettingsNavigationTab: Button {' not in locale_popup
        or 'contentItem: RowLayout {' not in locale_popup
        or 'text: navTab.text' not in locale_popup
        or 'id: settingsNavigation' not in locale_popup
        or 'visible: !root.mobilePresentation' not in locale_popup
        or 'id: mobileSettingsIndex' not in locale_popup
        or 'component MobileSettingsGroup: ColumnLayout {' not in locale_popup
        or 'root.currentSection === Number(modelData.section)' not in locale_popup):
    errors.append('Settings navigation must retain full desktop labels and grouped mobile destinations')

app_shell = (ui/'components'/'shell'/'AppShell.qml').read_text(encoding='utf-8')
setup_screen = (ui/'components'/'setup'/'InitialSetup.qml').read_text(encoding='utf-8')
route_label = 'genycaster.shell.farcaster_sign_in_openproof_verification'
route_description = 'genycaster.shell.farcaster_sign_in_openproof_description'
if (route_label not in app_shell or route_description not in app_shell
        or route_label not in setup_screen
        or 'presentationBridge.authenticationRoute === "farcaster"' not in app_shell
        or 'root.authenticationRoute === "farcaster"' not in setup_screen):
    errors.append('Farcaster SIWF route must be distinguishable from OpenProof verification without claiming signer readiness')
wheel_impl = (ui/'app'/'platform'/'wheel.cpp').read_text(encoding='utf-8')
wheel_api = (ui/'app'/'platform'/'wheel.cppm').read_text(encoding='utf-8')
wheel_main = (ui/'app'/'main.cpp').read_text(encoding='utf-8')
if ('browser_wheel::scroll(*window, local, pixels)' not in wheel_impl
        or 'scrollBranch' not in wheel_impl
        or 'property("modal")' not in wheel_impl
        or 'export module genycaster.ui.web.wheel;' not in wheel_api
        or 'import genycaster.ui.web.wheel;' not in wheel_main):
    errors.append('WASM wheel module must scroll Quick Flickables and respect modal popups, not only send synthetic QWheelEvent')

market_icon = (ui/'components'/'content'/'MarketAssetIcon.qml').read_text(encoding='utf-8')
if ('showManualLoadIndicator: false' not in market_icon
        or 'forceAutomaticDownload: true' not in market_icon):
    errors.append('Compact token logos must never render a full-size manual-load overlay across adjacent cards')

combo = (ui/'components'/'controls'/'GenyComboBox.qml').read_text(encoding='utf-8')
if 'required property int index' not in combo:
    errors.append('GenyComboBox delegate must declare its injected index role explicitly')

cast_item = (ui/'components'/'feed'/'CastItem.qml').read_text(encoding='utf-8')
if re.search(r'Layout\.(?:maximumWidth|minimumWidth|preferredWidth)\s*:\s*[^\n]*parent\.width', cast_item):
    errors.append('CastItem Layout constraint depends on its RowLayout parent width and can recurse')


# Discovery/profile chrome contracts from the profile polish pass.
side_navigation = (ui/'components'/'navigation'/'SideNavigation.qml').read_text(encoding='utf-8')
app_header = (ui/'components'/'shell'/'AppHeader.qml').read_text(encoding='utf-8')
feed_header = (ui/'components'/'feed'/'FeedHeader.qml').read_text(encoding='utf-8')
profile_surface = (ui/'components'/'profile'/'ProfileSurface.qml').read_text(encoding='utf-8')
feed_surface = (ui/'components'/'feed'/'FeedSurface.qml').read_text(encoding='utf-8')
notification_surface = (ui/'components'/'feed'/'NotificationSurface.qml').read_text(encoding='utf-8')
for model_name, model_text in (
    ('FeedSurface', feed_surface),
    ('ProfileSurface', profile_surface),
    ('NotificationSurface', notification_surface),
):
    if 'dynamicRoles: true' in model_text:
        errors.append(f'{model_name} timeline models must keep stable role types; dynamicRoles is forbidden on scrolling surfaces')
    if 'setProperty(index, "payload"' in model_text or re.search(r'(?:append|insert)\([^\n]*payload\s*:', model_text):
        errors.append(f'{model_name} must keep complex QVariant/JS payloads outside ListModel roles to preserve stable role typing')
    if 'stablePayloads' not in model_text or 'payloadForStableKey' not in model_text:
        errors.append(f'{model_name} must resolve complex row payloads from a stable-key side store')
if 'ProfileTrustCard {' not in side_navigation:
    errors.append('Desktop navigation must include the compact Trust/Profile card required by the social shell')
if 'genycaster.feed_header.search_people_channels_casts' not in feed_header or 'signal searchRequested(string query)' not in feed_header:
    errors.append('Explore must expose one unified search field for profiles, channels, and casts')
search_palette = (ui/'components'/'navigation'/'SearchPalette.qml').read_text(encoding='utf-8')
if 'signal searchRequested(string query)' not in search_palette or 'feed_header.search_people_channels_casts' not in search_palette:
    errors.append('First-class search palette must support people, channels, and casts')
if ('property var resultPayloads: ({})' not in search_palette
        or re.search(r'resultRows\.(?:append|set|setProperty)\([^\n]*payload', search_palette)):
    errors.append('SearchPalette must keep complex result payloads outside type-stable ListModel roles')
if 'showProfileUrl' not in profile_surface or 'showLocation' not in profile_surface or 'metadataKey' not in profile_surface:
    errors.append('Profile metadata must de-duplicate username, URL, and location values')
if re.search(r'RichText\s*\{[^{}]*onAuthenticationRequested\s*:', profile_surface, re.S):
    errors.append('Profile RichText cannot handle authenticationRequested; route auth only through interactive controls')
empty_state = (ui/'components'/'controls'/'EmptyState.qml').read_text(encoding='utf-8')
if 'visible: root.idleHome || root.trendingUnavailable' in empty_state:
    errors.append('Signed-out Explore source failure must not expose identity as a recovery action')

app_shell = (ui/'components'/'shell'/'AppShell.qml').read_text(encoding='utf-8')
if 'passiveSignedOutDiscoveryFailure' not in app_shell:
    errors.append('Signed-out Explore source failures must stay inline instead of becoming a global status warning')
notifications_surface = (ui/'components'/'feed'/'NotificationSurface.qml').read_text(encoding='utf-8')
if ('property bool notificationBusySnapshot: false' not in app_shell
        or 'id: notificationsRouteLoader' not in app_shell
        or 'loadingNotifications: false' not in app_shell
        or 'item.loadingNotifications = root.notificationBusySnapshot' not in app_shell
        or 'function onNotificationsChanged()' not in app_shell
        or 'property bool loadingNotifications: false' not in notifications_surface
        or 'root.busy' in notifications_surface):
    errors.append('Notifications must copy loading state imperatively into the lazy route; never bind it to the bridge notificationsChanged cascade')
if ('const modeChanged = root.explorerPresentationMode !== nextMode' not in app_shell
        or 'const refreshRequested = Boolean(forceRefresh)' not in app_shell
        or 'if (!sameHealthySource)' not in app_shell
        or 'presentationBridge.retry()' not in app_shell
        or 'root.selectExplorerPresentationMode(root.explorerPresentationMode, true)' not in app_shell
        or 'presentationBridge.selectFeed(source)' not in app_shell):
    errors.append('Explorer semantic tabs must keep one live source and refresh it in place when required')
if 'presentationBridge.requestExplorerArt()' in app_shell:
    errors.append('Explorer Visuals must not inject the legacy cached art-channel source')
gallery_layout = (ui/'components'/'media'/'GalleryLayout.qml').read_text(encoding='utf-8')
gallery_tile = (ui/'components'/'media'/'GalleryTile.qml').read_text(encoding='utf-8')
if 'return root.embeds[index] || ({})' not in gallery_layout:
    errors.append('GalleryLayout must normalize sparse/null embed entries before media property access')
if ('readonly property rect cornerTargetRect:' not in gallery_tile
        or '? preview.visibleContentRect' not in gallery_tile
        or 'id: cornerCover' not in gallery_tile
        or 'x: root.cornerTargetRect.x' not in gallery_tile
        or 'y: root.cornerTargetRect.y' not in gallery_tile):
    errors.append('GalleryTile must round the actual painted PreserveAspectFit image rectangle, not only the reserved tile bounds')
if ('id: newPostsSlot' not in feed_surface
        or '+ newPostsSlot.height' not in feed_surface
        or 'id: pinnedHeader' not in feed_surface
        or 'header.mapToItem(root, 0, header.height - newPostsBanner.height).y' not in feed_surface
        or 'const header = readList.headerItem' not in feed_surface
        or 'newPostsSlot.mapToItem(root, 0, 0).y' in feed_surface):
    errors.append('New-post affordance must reserve header space and pin to the viewport instead of overlaying the first cast')
for token in (
    'property bool scrollHostResolved: false',
    'readonly property bool scrollMoving:',
    'property bool admitted: false',
    'active: tileSlot.admitted',
    'interval: 16 + tileSlot.index * 32',
    'useLayeredMask: false',
):
    if token not in gallery_layout:
        errors.append(f'GalleryLayout scroll-time tile admission missing {token!r}')
# Drag overshoot is required for pull-to-refresh on mobile. Ordinary
# wheel/flick movements remain bounded by Qt; the empty state is a measured
# list footer rather than an overlaid negative-height card.
if ('boundsBehavior: Flickable.DragOverBounds' not in profile_surface
        or 'boundsMovement: Flickable.FollowBoundsBehavior' not in profile_surface
        or 'boundsBehavior: Flickable.DragOverBounds' not in feed_surface
        or 'boundsMovement: Flickable.FollowBoundsBehavior' not in feed_surface
        or 'headerPositioning: ListView.InlineHeader' not in feed_surface
        or 'id: feedEmptyState' not in feed_surface
        or 'height: stableRows.count === 0' not in feed_surface
        or 'id: stateContent' not in (root/'ui/components/controls/EmptyState.qml').read_text()):
    errors.append('Feed/profile pull-to-refresh and measurable list footer contracts regressed')
# Timeline cells retain a plain non-painting Item. A GPU/Neon surface is
# allocated only for Magazine cards; normal Timeline rows remain transparent.
# Keep adaptive gutters symmetric instead of reviving hard-coded 58px offsets.
if ('Item {\n            id: messageSurface' not in cast_item
        or 'HoverHandler { id: messageHover }' in cast_item
        or 'visible: root.magazineCard' not in cast_item
        or 'fillColor: root.magazineCard ? Theme.surfaceRaised : "transparent"' not in cast_item
        or 'interactive: root.magazineCard && !root.scrollMoving' not in cast_item
        or 'anchors.leftMargin: root.magazineCard ? Theme.space4 : root.compact ? Theme.space2 : Theme.space4' not in cast_item
        or 'anchors.rightMargin: root.magazineCard ? Theme.space4 : root.compact ? Theme.space2 : Theme.space4' not in cast_item):
    errors.append('Cast timeline must keep a plain non-painting Item and reserve GPU Neon surfaces for Magazine cards')
if 'root.modelData.parentCastId || ""' in cast_item and 'text: root.ltrText(root.modelData.parentCastId' in cast_item:
    errors.append('Reply context must not expose raw parent cast identifiers')

# Genycaster 2026 native social redesign contracts.
if 'readonly property real superellipseExponent: 4.0' not in theme_text:
    errors.append('Genycaster primary shape language must use mathematical superellipse exponent n=4')
for token in (
    'breakpointMobile: 600',
    'breakpointNarrow: 900',
    'breakpointCompactDesktop: 1200',
    'breakpointLargeDesktop: 1440',
    'navigationWidth: 352',
    'contextRailWidth: 416',
):
    if token not in theme_text:
        errors.append(f'Native adaptive design contract is missing {token}')
if 'HomeFeedHeader {' not in app_shell or 'recentPosterProfiles(presentationBridge.rows' not in app_shell:
    errors.append('Home must preserve the real feed bridge and derive recent-profile chrome from live feed rows')
if 'MobileCreateMenu {' not in app_shell:
    errors.append('Mobile Create must expand into the Genycaster-specific creation surface')
if 'ContextRail {' not in app_shell or 'contextDrawer.open()' not in app_shell:
    errors.append('Compact desktop must keep the context rail available through a collapsible drawer')
for component in ('EmbedPreview.qml', 'LinkPreview.qml', 'VideoPreview.qml', 'CollectiblePreview.qml', 'GalleryLayout.qml'):
    if not component_source_path(component).exists():
        errors.append(f'Semantic media presentation component missing: {component}')
media_gallery = (ui/'components'/'media'/'MediaGallery.qml').read_text(encoding='utf-8')
link_preview_media = (ui/'components'/'media'/'LinkPreview.qml').read_text(encoding='utf-8')
if 'id: fullImage' not in media_gallery or 'fillMode: Image.PreserveAspectFit' not in media_gallery:
    errors.append('Fullscreen MediaGallery must PreserveAspectFit so user media is never cropped')
if ('id: miniAppPoster' not in link_preview_media
        or 'previewImageAspect' not in link_preview_media
        or not re.search(r'id:\s*miniAppPoster[\s\S]{0,900}fillMode:\s*Image\.PreserveAspectFit', link_preview_media)):
    errors.append('Mini App discovery artwork must use metadata-aware PreserveAspectFit presentation')
home_dashboard = (ui/'components'/'home'/'HomeDashboard.qml').read_text(encoding='utf-8')
if 'recentActivity' in home_dashboard or 'component ActivityRow' in home_dashboard:
    errors.append('Home dashboard must not instantiate the removed duplicate Recent Activity widget column')
if 'readonly property bool compactActions: width < 620' not in home_dashboard:
    errors.append('Home composer actions must collapse secondary labels before the primary Cast action can overflow')
if 'readonly property bool curateHomeForYou: false' not in feed_surface:
    errors.append('For You pagination must preserve stable source order; semantic presentation must not reorder existing casts')
cast_embeds_panel = (ui/'components'/'media'/'CastEmbedsPanel.qml').read_text(encoding='utf-8')
if ('CastEmbedsPanel {' not in cast_item
        or 'GalleryLayout {' not in cast_embeds_panel
        or 'EmbedPreview {' not in cast_embeds_panel):
    errors.append('Cast presentation must route semantic embeds and multi-image composition through CastEmbedsPanel')

remote_provider = ((ui/'app'/'media'/'remote_image.hpp').read_text(encoding='utf-8') +
                   (ui/'app'/'media'/'remote_image.cpp').read_text(encoding='utf-8'))
if ('QQuickAsyncImageProvider' not in remote_provider
        or 'transparentFailureImage' not in remote_provider
        or 'failedRequests' not in remote_provider):
    errors.append('Remote image provider must remain async, back off repeated failures, and return a quiet fallback')

# RTL/bidi contracts: UI geometry is explicit; content direction stays independent.
main_qml = (ui/'components'/'shell'/'Main.qml').read_text(encoding='utf-8')
if 'LayoutMirroring.enabled: localizationController.rightToLeft' in main_qml:
    errors.append('Main.qml must not globally mirror the QML tree; it double-mirrors explicit UI layouts')
if 'LayoutMirroring.enabled: false' not in main_qml:
    errors.append('Main.qml must explicitly disable global tree mirroring')

for chrome_name in ('AppHeader.qml', 'SideNavigation.qml', 'BottomNavigation.qml', 'FeedHeader.qml', 'StatusStrip.qml'):
    chrome_text = component_source_path(chrome_name).read_text(encoding='utf-8')
    # Header chrome intentionally anchors its action cluster separately from RTL
    # wordmark geometry and keeps its icon order LTR. Both are explicit.
    if 'RowLayout {' in chrome_text and 'layoutDirection:' not in chrome_text:
        errors.append(f'{chrome_name} must direct application-chrome RowLayouts explicitly')

# Every horizontal application layout must declare a direction. This catches
# accidental dependence on inherited/global mirroring before it reaches runtime.
for qml_path in ui.rglob('*.qml'):
    qml_lines = qml_path.read_text(encoding='utf-8').splitlines()
    for line_index, line in enumerate(qml_lines):
        if not any(token in line for token in ('RowLayout {', 'GridLayout {', 'Flow {')):
            continue
        nearby = '\n'.join(qml_lines[line_index:line_index + 20])
        if 'layoutDirection:' not in nearby:
            errors.append(f'Horizontal layout lacks explicit direction: {qml_path.relative_to(root)}:{line_index + 1}')
        # QQuickGridLayout supports rowSpacing and columnSpacing, but unlike
        # RowLayout/ColumnLayout has NO `spacing` property. QML compiles, yet
        # instantiation fails and the entire AppShell becomes unavailable.
        if re.match(r'^\s*GridLayout\s*\{', line):
            nesting = 1
            for child_index in range(line_index + 1, len(qml_lines)):
                code = qml_lines[child_index].split('//', 1)[0]
                if nesting == 1 and re.match(r'^\s*spacing\s*:', code):
                    errors.append(
                        f'GridLayout has no spacing property; use rowSpacing/columnSpacing: '
                        f'{qml_path.relative_to(root)}:{child_index + 1}')
                nesting += code.count('{') - code.count('}')
                if nesting <= 0:
                    break

combo = (ui/'components'/'controls'/'GenyComboBox.qml').read_text(encoding='utf-8')
if 'measuredTextWidth' not in combo or 'naturalPopupWidth' not in combo:
    errors.append('GenyComboBox popup must size from the widest model text')
if 'x: Theme.rtl ? control.width - width : 0' not in combo:
    errors.append('GenyComboBox popup must align to logical start in RTL and LTR')
if 'elide: Text.ElideNone' not in combo:
    errors.append('GenyComboBox popup entries must not elide valid option labels')

cast_item = (ui/'components'/'feed'/'CastItem.qml').read_text(encoding='utf-8')
if cast_item.count('layoutDirection: root.contentRtl ? Qt.RightToLeft : Qt.LeftToRight') < 4:
    errors.append('CastItem nested horizontal layouts must consistently follow cast-content direction')

# R5 navigation, typography and client-shell contracts.
theme = (ui/'theme'/'Theme.qml').read_text(encoding='utf-8')
if 'bundledPersianFontFamily' not in theme:
    errors.append('R5 must prefer the application-bundled Persian font over system availability')
font_module = (ui/'app'/'bootstrap'/'fonts.cpp').read_text(encoding='utf-8')
if 'QFontDatabase::addApplicationFont' not in font_module or 'Vazirmatn-Regular.ttf' not in font_module:
    errors.append('R5 must load the embedded Persian font family through QFontDatabase')
if 'GENYCASTER_FETCH_UI_FONT' not in cmake or 'qt_add_resources(genycaster genycaster_ui_fonts' not in cmake:
    errors.append('R5 CMake must acquire and embed the pinned Persian UI font family')
if 'iconFontFamily' not in theme or 'iconGlyph' not in theme:
    errors.append('R6 icon system must use the bundled canonical icon font through semantic names')
if 'GENYCASTER_FETCH_ICON_FONT' not in cmake or 'fa-regular-400.ttf' not in cmake or 'fa-solid-900.ttf' not in cmake:
    errors.append('R6 must acquire and embed Font Awesome Free Regular + Solid icon fonts')
if 'GENYCASTER_FETCH_SCRIPT_FONTS' not in cmake or 'NotoSansDevanagari.ttf' not in cmake:
    errors.append('R6 must bundle script-specific shaping coverage for multilingual UI')
side_nav = (ui/'components'/'navigation'/'SideNavigation.qml').read_text(encoding='utf-8')

# Custom navigation signal handlers must match signals exported by SideNavigation.
# This catches QML load failures such as stale onFollowingListRequested handlers
# after a navigation contract refactor.
side_nav_signals = set(re.findall(r'^\s*signal\s+([A-Za-z_]\w*)\s*\(', side_nav, re.M))
try:
    side_nav_binding = app_shell.split('SideNavigation {', 1)[1].split('\n        }\n\n        Item {', 1)[0]
except IndexError:
    errors.append('AppShell SideNavigation binding block could not be located')
else:
    for handler_name in re.findall(r'\bon([A-Z][A-Za-z0-9_]*Requested)\s*:', side_nav_binding):
        signal_name = handler_name[0].lower() + handler_name[1:]
        if signal_name not in side_nav_signals:
            errors.append(f'AppShell handles SideNavigation.{signal_name}, but SideNavigation does not export that signal')

# Store, Business, and Messages are intentionally hidden from desktop
# navigation until those surfaces are production-ready.
for nav_id in ('channels', 'notifications', 'bookmarks'):
    if f'routeRequested("{nav_id}")' not in side_nav:
        errors.append(f'R5 sidebar is missing active destination: {nav_id}')
bottom_nav = (ui/'components'/'navigation'/'BottomNavigation.qml').read_text(encoding='utf-8')
for token in ('bottom_nav.home', 'side_nav.explorer', 'bottom_nav.create', 'apps.title', 'settings.appearance'):
    if token not in bottom_nav:
        errors.append(f'vNext five-destination mobile navigation is missing {token}')
icon = (ui/'components'/'controls'/'Icon.qml').read_text(encoding='utf-8')
if ('Theme.iconSource(root.text, root.filled)' not in icon
        or 'icon.source: root.source' not in icon
        or 'Theme.iconGlyph(root.text)' not in icon):
    errors.append('vNext icons must prefer semantic Solar SVGs while retaining the bounded font fallback')

composer_drawer = (ui/'components'/'composer'/'ComposerDrawer.qml').read_text(encoding='utf-8')
geny_drawer = (ui/'components'/'controls'/'GenyDrawer.qml').read_text(encoding='utf-8')
if ('bottomSheet: true' not in composer_drawer
        or 'RichTextEditor' not in composer_drawer
        or 'FarcasterPreview' not in composer_drawer
        or 'richComposer.safeText' not in composer_drawer):
    errors.append('R7 composer must be a bottom drawer backed by the native rich-document engine and safe preview')
if 'FloatingComposeButton' not in app_shell or 'onComposeRequested: root.openComposer(null)' not in app_shell:
    errors.append('R6 compose entry point must be a floating action, not sidebar chrome')
floating_compose = (ui/'components'/'navigation'/'FloatingComposeButton.qml').read_text(encoding='utf-8')
floating_block = app_shell.split('FloatingComposeButton {', 1)[1].split('\n    Shortcut {', 1)[0]
if ('width: implicitWidth' not in floating_compose
        or 'height: implicitHeight' not in floating_compose
        or re.search(r'anchors\.(?:left|right)\s*:\s*Theme\.rtl', floating_block)
        or 'x: Theme.rtl' not in floating_block):
    errors.append('Floating compose geometry must use explicit size/x positioning and never swap left/right anchors during live RTL changes')
if 'highQualityEdges: true' not in floating_compose:
    errors.append('Floating compose must opt into CurveRenderer edge quality without forcing Shape cost onto timeline surfaces')
if 'header.new_cast' in side_nav:
    errors.append('R6 sidebar must not contain the New Cast primary action')
backdrop_blur_path = ui/'components'/'BackdropBlur.qml'
if backdrop_blur_path.exists():
    errors.append('Drawers must use the shared GenyDrawer frost pipeline instead of an ad-hoc BackdropBlur component')
drawer_backdrop = (ui/'components'/'visual'/'DrawerBackdrop.qml').read_text(encoding='utf-8')
if 'DrawerBackdrop {' not in geny_drawer or 'blurSourceItem' not in geny_drawer:
    errors.append('GenyDrawer must route environment blur through the shared DrawerBackdrop')
if 'ShaderEffectSource {' not in drawer_backdrop or 'MultiEffect {' not in drawer_backdrop:
    errors.append('DrawerBackdrop must blur the application environment through ShaderEffectSource + MultiEffect')
if 'fillColor: Theme.surfaceElevated' not in geny_drawer:
    errors.append('GenyDrawer surface must remain opaque while the environment behind it is blurred')
if 'PopupSurface {' not in geny_drawer:
    errors.append('GenyDrawer must retain a tinted PopupSurface over the frosted backdrop')
if ('enter: Transition {' not in geny_drawer
        or 'exit: Transition {' not in geny_drawer
        or geny_drawer.count('property: "position"') != 2
        or 'Theme.reduceMotion ? 0 : Theme.motionStandard' not in geny_drawer):
    errors.append('All drawers must share interruptible edge-position enter/exit motion with reduced-motion support')
# The verified personal custody signer is intentionally supported even
# without an app-attribution attestor. The UI may offer retry/approval only
# for the current authenticated identity, and cannot bypass busy/ready gates.
if ('visible: presentationBridge.isAuthenticated && !presentationBridge.writeReady' not in app_shell
        or 'enabled: !presentationBridge.signerBusy' not in app_shell
        or 'presentationBridge.provisionSigner()' not in app_shell):
    errors.append('Account drawer signer activation must honor authentication, readiness and busy gates')
# Read-only code must use TextEdit when mouse selection is enabled. Qt Quick Text
# does not expose selectByMouse; using it makes the entire QML type chain fail at runtime.
code_block_view = (ui/'components'/'content'/'CodeBlockView.qml').read_text(encoding='utf-8')
if re.search(r'\bText\s*\{(?:(?!\n\s*\}).)*?\bid\s*:\s*codeText(?:(?!\n\s*\}).)*?\bselectByMouse\s*:', code_block_view, re.S):
    errors.append('CodeBlockView codeText must be a read-only TextEdit when selectByMouse is enabled')
if 'TextEdit {' not in code_block_view or 'id: codeText' not in code_block_view or 'readOnly: true' not in code_block_view:
    errors.append('CodeBlockView codeText must remain a selectable read-only TextEdit')
code_text_match = re.search(r'\bTextEdit\s*\{\s*\n\s*id\s*:\s*codeText(?P<body>.*?)(?=\n\s*\})', code_block_view, re.S)
if code_text_match and re.search(r'^\s*lineHeight\s*:', code_text_match.group('body'), re.M):
    errors.append('Qt 6.11 TextEdit does not expose lineHeight; encode code-view line spacing in the sanitized rich-text <pre> style instead')
if "line-height:1.5" not in code_block_view:
    errors.append('CodeBlockView must preserve code line spacing through its sanitized <pre> rich-text style')

# The current user-approved Settings experience fills the viewport on
# desktop and mobile: this avoids an expensive blurred backdrop.
if ('GenyAdaptiveDialog {' not in locale_popup
        or 'desktopFullScreen: true' not in locale_popup
        or 'mobileFullScreen: true' not in locale_popup
        or 'id: mobileSettingsIndex' not in locale_popup):
    errors.append('Settings must keep a full-viewport responsive presentation')

# Mini App multi-window lifecycle and media hot-path regression contracts.
miniapp_runtime = (ui/'app'/'miniapp'/'runtime.cpp').read_text(encoding='utf-8')
miniapp_browser = (ui/'app'/'miniapp'/'browser.hpp').read_text(encoding='utf-8')
miniapp_macos = (ui/'app'/'miniapp'/'browser_macos.m').read_text(encoding='utf-8')
miniapp_macos_bridge = (ui/'app'/'miniapp'/'browser_macos.cpp').read_text(encoding='utf-8')
if ('supportsParallelWindowVisibility' not in miniapp_runtime
        or 'activate(*target, false)' not in miniapp_runtime
        or 'QStringLiteral("background")' not in miniapp_runtime):
    errors.append('Mini App Window presentation must preserve parallel visible sessions while tracking one active task')
if ('std::function<void()> activated' not in miniapp_browser
        or 'windowDidBecomeKey' not in miniapp_macos
        or 'GENY_MAC_ACTIVATED' not in miniapp_macos):
    errors.append('Native Mini App windows must report activation so foreground authority follows the focused window')
video_preview = (ui/'components'/'media'/'VideoPreview.qml').read_text(encoding='utf-8')
media_preview = (ui/'components'/'media'/'MediaPreview.qml').read_text(encoding='utf-8')
video_player = (ui/'components'/'media'/'GenyVideoPlayer.qml').read_text(encoding='utf-8')
if 'framePrimeArmed' in video_preview or 'eager: true' in video_preview or 'primeFrame: false' not in video_preview:
    errors.append('Feed video previews must stay decoder-free until explicit playback')
if 'eager: true' in media_preview or 'primeFrame: false' not in media_preview:
    errors.append('Composer media previews must not decode hidden video frames to manufacture thumbnails')
if 'QtQuick.Effects' in video_player or 'layer.effect: MultiEffect' in video_player:
    errors.append('Canonical video playback must keep decoded frames off the offscreen MultiEffect hot path')
if 'return stableRows.count < root.filterTargetCount' not in feed_surface:
    errors.append('Filtered feed prefetch must reuse stableRows instead of reclassifying sourceRows during scroll')
superellipse_image = (ui/'components'/'visual'/'SuperellipseImage.qml').read_text(encoding='utf-8')
if ('if (current !== requested || (!root.ready && !root.loadFailed))' not in superellipse_image
        or 'if (!root.ready && !root.loadFailed)' not in superellipse_image
        or 'root.activeSource = ""' not in superellipse_image):
    errors.append('SuperellipseImage must detach unfinished QML image consumers during flicks so texture preparation cannot enter the scroll frame')
remote_cancel_block = remote_provider.split('void cancel() override', 1)[1].split('void setOperation', 1)[0]
if ('m_cancelled.store(true' not in remote_cancel_block
        or 'm_control->cancel()' in remote_cancel_block
        or 'operation->cancel()' in remote_cancel_block):
    errors.append('QML image cancellation must detach presentation without aborting already-admitted bounded provider work; post-fling reads should hit provider cache')
embed_preview = (ui/'components'/'media'/'EmbedPreview.qml').read_text(encoding='utf-8')
# Timeline previews incubate asynchronously; Magazine previews are created
# synchronously only after the *outer* card's viewport admission to avoid
# destroying pending QML incubators on rapid magazine recycling.
if ('property bool metadataUpdatePending: false' not in embed_preview
        or 'if (root.scrollMoving)' not in embed_preview
        or 'root.metadataUpdatePending = true' not in embed_preview
        or 'root.acceptLinkMetadata(preview)' not in embed_preview
        or 'if (root.metadataUpdatePending)' not in embed_preview
        or 'asynchronous: !root.magazineCard' not in embed_preview):
    errors.append('EmbedPreview must defer moving-scroll metadata and asynchronously incubate Timeline (not admitted Magazine) previews')
if ('asynchronous: !root.magazineCard' not in link_preview_media
        or 'property bool resolvedPreviewPending: false' not in link_preview_media
        or 'if (root.resolvedPreviewPending)' not in link_preview_media
        or 'root.pendingResolvedPreview = preview' not in link_preview_media
        or 'enabled: !root.scrollMoving' not in link_preview_media):
    errors.append('LinkPreview must defer scroll metadata and hover churn while preserving Timeline/Magazine loader admission')
if ('asynchronous: true' not in profile_surface
        or 'rowIncubationHeight' not in profile_surface
        or 'if (!profileList.moving)' not in profile_surface):
    errors.append('Profile timeline must asynchronously incubate rich CastItem delegates and freeze measured heights during flicks')
if ('property bool previewSyncPending: false' not in feed_surface
        or 'if (readList.moving)' not in feed_surface
        or 'root.previewSyncPending = true' not in feed_surface):
    errors.append('Feed metadata-driven semantic reconciliation must stay deferred while kinetic scrolling is active')
rich_inline_text = (ui/'components'/'content'/'RichInlineText.qml').read_text(encoding='utf-8')
if ('id: spriteEmojiLoader' not in rich_inline_text
        or 'active: emojiOverlay.spriteFallbackArmed && !lottieEmoji.animationReady' not in rich_inline_text
        or 'visible: active && !lottieEmoji.animationReady' not in rich_inline_text
        or 'spriteFallbackTimer' in rich_inline_text
        or 'asynchronous: false' not in rich_inline_text
        or 'textLayout.setEmojiAnimated(emojiPosition, emojiGeneration, false)' not in rich_inline_text):
    errors.append('Inline emoji must use bounded synchronous sprite fallback only after Lottie failure and detach ownership on delegate destruction')
animated_emoji_lottie = (ui/'components'/'media'/'AnimatedEmojiLottie.qml').read_text(encoding='utf-8')
if ('animations.paused: !root.animationActive && !root.playbackCompleted' not in animated_emoji_lottie
        or 'animations.restart()' in animated_emoji_lottie):
    errors.append('Animated emoji scrolling must pause/resume the current VectorImage frame instead of restarting animation on every flick')
link_preview_resolver = (ui/'app'/'network'/'link_preview.cpp').read_text(encoding='utf-8')
if ('QThreadPool parsePool' not in link_preview_resolver
        or 'parsePool.setThreadPriority(QThread::LowPriority)' not in link_preview_resolver
        or 'parseHtmlAsync' not in link_preview_resolver):
    errors.append('Link preview HTML parsing must stay on the bounded low-priority worker instead of the GUI thread')
for token in (
    'maximumConcurrentPreviewRequests = 2',
    'QQueue<PendingPreview> pending',
    'QSet<QString> queued',
    'class LinkPreviewNetworkWorker final : public QObject',
    'QThread networkThread',
    'networkWorker->moveToThread(&networkThread)',
    'networkThread.start(QThread::LowPriority)',
    'std::make_shared<QByteArray>()',
    'payload->append(',
    'if (*truncated && reply->isRunning())',
):
    if token not in link_preview_resolver:
        errors.append(f'Link preview queue/buffer hot-path contract missing {token!r}')
if 'QNetworkAccessManager network{}' in link_preview_resolver:
    errors.append('Link preview networking must not keep QNetworkAccessManager on the GUI-owned resolver object')
if ('&QThread::finished' not in link_preview_resolver
        or '&QObject::deleteLater' not in link_preview_resolver
        or 'QObject::disconnect(reply, nullptr, this, nullptr)' not in link_preview_resolver
        or "networkThread.wait(2'000)" not in link_preview_resolver
        or '*stopped = true' not in link_preview_resolver):
    errors.append('Link preview shutdown must disconnect/abort replies on their worker thread and bound thread exit without late TLS reads')
if 'genycasterPayload' in link_preview_resolver:
    errors.append('Link preview readyRead must not round-trip the accumulated HTML through QObject dynamic properties')
# Keep this strict UI audit consistent with the separately checked canonical
# Explorer pagination acceptance. The bounded 8/16 budget provides sparse tabs
# with enough provider pages without unbounded load loops.
if ('readonly property int filterPrefetchLimit: 8' not in feed_surface
        or 'readonly property int automaticPrefetchLimit: 16' not in feed_surface):
    errors.append('Sparse Explorer semantic tabs must retain the bounded eight-page/per-filter and sixteen-page global prefetch budget')
# A single shared caption Loader now precedes media for every cast, rather
# than duplicating caption subtrees for visual vs text-only casts.
# Rich text is a small native document subtree and deliberately synchronous:
# rapid row recycling otherwise invalidates its QML incubator mid-frame.
# Large media/supporting cards stay async on Timeline; Magazine only admits
# the outer delegate near the viewport and creates nested cards synchronously.
if (cast_item.count('asynchronous: !root.magazineCard') < 3
        or 'id: castTextBody' not in cast_item
        or 'asynchronous: false' not in cast_item
        or 'active: root.mediaFirst && root.supportingEmbeds.length > 0' not in cast_item
        or '&& root.supportingAdmitted' not in cast_item
        or 'active: !root.mediaFirst && root.supportingEmbeds.length > 0' not in cast_item):
    errors.append('CastItem must admit only supporting embeds and incubate heavy Timeline media without recycled Magazine incubator churn')
if ('objectName: "castTextBody"' not in cast_item or
        'objectName: "castVisualMedia"' not in cast_item or
        cast_item.index('objectName: "castTextBody"') >= cast_item.index('objectName: "castVisualMedia"')):
    errors.append('CastItem must render caption before embedded image media on desktop and WASM')
for token in (
    'property bool supportingAdmitted: false',
    'readonly property bool scrollMoving:',
    'onSupportingEmbedsChanged:',
    '&& root.supportingAdmitted',
):
    if token not in cast_item:
        errors.append(f'CastItem mixed-media supporting admission contract missing {token!r}')
if ('/https?:\\/\\//i.test(String(value.text || ""))' not in feed_surface
        or 'supportingEmbedCount' not in feed_surface):
    errors.append('Feed row reservation must account for inferred links and mixed image+supporting-card geometry before async admission')
if 'id: messageSurface' in cast_item and re.search(r'Superellipse\s*\{\s*\n\s*id:\s*messageSurface', cast_item):
    errors.append('CastItem transparent messageSurface must remain a plain Item; invisible scene-graph surfaces are forbidden per-row')

appearance_header = (root/'src'/'presentation'/'qt'/'appearance_controller.hpp').read_text(encoding='utf-8')
appearance_source = (root/'src'/'presentation'/'qt'/'appearance_controller.cpp').read_text(encoding='utf-8')
if ('Q_PROPERTY(qreal fontScale' not in appearance_header
        or 'Q_PROPERTY(qreal interfaceScale' not in appearance_header
        or 'appearance/fontScale' not in appearance_source
        or 'appearance/interfaceScale' not in appearance_source):
    errors.append('Appearance must persist independent text and interface scaling through the presentation controller')
if ('readonly property real fontScale: appearanceController.fontScale' not in theme_text
        or 'readonly property real interfaceScale: appearanceController.interfaceScale' not in theme_text):
    errors.append('Theme must apply persisted text/interface scales through semantic tokens')
miniapp_preview = (ui/'components'/'apps'/'MiniAppPreview.qml').read_text(encoding='utf-8')
if ('presentationCard' not in locale_popup
        or 'MiniAppPresentationPreview {' not in locale_popup
        or 'readonly property bool sidePanel' not in miniapp_preview
        or 'readonly property bool bottomPanel' not in miniapp_preview
        or 'readonly property bool floatingWindow' not in miniapp_preview
        or 'id: titleBar' not in miniapp_preview
        or 'id: navRail' not in miniapp_preview
        or 'id: miniAppPanel' not in miniapp_preview):
    errors.append('Mini App presentation settings must use detailed graphical placement previews instead of a text-only selector')
if re.search(r'anchors\.(?:left|right|top|bottom)Margin\s*:\s*-', miniapp_preview):
    errors.append('Mini App presentation previews must keep selection and chrome inside the card bounds')
if 'root.bottomPanel ? workspace.width * 0.56' not in miniapp_preview:
    errors.append('Bottom Mini App preview must communicate the constrained vertical modal width')

display_scale = (ui/'components'/'settings'/'DisplayScaleControl.qml').read_text(encoding='utf-8')
if 'Theme.success' in display_scale:
    errors.append('Display scale controls must use one accent language and never mix success green into sizing state')
if 'LocaleFormat.formatInteger(root.valuePercent)' not in display_scale:
    errors.append('Display scale percentage labels must react to runtime locale digit changes')

superellipse_qml = (ui/'components'/'visual'/'Superellipse.qml').read_text(encoding='utf-8')
if ('SceneGraphSuperellipse {' not in superellipse_qml
        or 'qualityScale:' not in superellipse_qml
        or 'property bool highQualityEdges: false' not in superellipse_qml
        or 'active: root.highQualityEdges' not in superellipse_qml
        or 'preferredRendererType: Shape.CurveRenderer' not in superellipse_qml
        or 'PathSvg {' in superellipse_qml
        or 'containmentMask: renderedShape' in superellipse_qml):
    errors.append('Superellipse must keep retained QSG as the default and isolate CurveRenderer to the explicit high-quality edge opt-in')
if 'allowsBackForwardNavigationGestures = disableNativeGestures' in miniapp_macos:
    errors.append('Farcaster disableNativeGestures must not be misapplied to WebKit history-navigation gestures')
show_status_block = miniapp_macos.split('- (void)showStatus:', 1)[1].split('- (void)showReady', 1)[0]
if ('genycasterHostWindow()' not in miniapp_macos
        or 'static __weak NSWindow* cachedHostWindow' not in miniapp_macos):
    errors.append('Native Mini App drawers must remain anchored to the stable Qt host window instead of NSApp.mainWindow after another Mini App becomes key')
if ('m_web = nil;' in show_status_block
        or 'removeScriptMessageHandlerForName' in show_status_block):
    errors.append('WKWebView teardown must not happen reentrantly inside WebKit failure callbacks; cleanup belongs to dispose')
if ('[[nodiscard]] bool valid() const noexcept' not in miniapp_macos_bridge
        or 'if (!browser->valid())' not in miniapp_macos_bridge
        or 'geny_mac_browser_valid' not in miniapp_macos_bridge):
    errors.append('macOS Mini App browser construction must fail closed before dereferencing an unavailable native window/WebView')

# Dynamic strokes update far more often than card silhouettes. Keep them on
# Qt Shapes GeometryRenderer with a small local 4x MSAA layer; this avoids
# CurveRenderer preprocessing churn while retaining high-quality edges.
vector_stroke = (ui/'components'/'visual'/'VectorStroke.qml').read_text(encoding='utf-8')
if ('import QtQuick.Shapes' not in vector_stroke
        or vector_stroke.count('preferredRendererType: Shape.GeometryRenderer') < 3
        or vector_stroke.count('layer.samples: 4') < 3
        or 'asynchronous: true' in vector_stroke
        or 'PathAngleArc {' not in vector_stroke
        or 'PathPolyline {' not in vector_stroke
        or 'PathCubic {' not in vector_stroke):
    errors.append('VectorStroke must use synchronous Qt Shapes GeometryRenderer plus local 4x MSAA for dynamic arcs, sparklines, and rounded outlines')
for visual_path in (ui/'components').rglob('*.qml'):
    if visual_path.name == 'VectorStroke.qml':
        continue
    if re.search(r'\bSceneGraphStroke\s*\{', visual_path.read_text(encoding='utf-8')):
        errors.append(f'Legacy SceneGraphStroke visual remains in production QML: {visual_path.relative_to(root)}')

progress_ring = (ui/'components'/'controls'/'ProgressRing.qml').read_text(encoding='utf-8')
profile_trust = (ui/'components'/'profile'/'ProfileTrustCard.qml').read_text(encoding='utf-8')
if ('preferredRendererType: Shape.CurveRenderer' not in progress_ring
        or 'id: streakRing' not in profile_trust
        or 'ProgressRing {' not in profile_trust
        or ('id: streakRing' in profile_trust
            and 'VectorStroke {' in profile_trust.split('id: streakRing', 1)[1].split('}', 1)[0])):
    errors.append('Profile trust ring must use the mostly-static CurveRenderer progress primitive instead of dynamic VectorStroke geometry')

animated_emoji = (ui/'components'/'media'/'AnimatedEmoji.qml').read_text(encoding='utf-8')
if ('readonly property bool insideViewport: {' in animated_emoji
        or 'while (node !== null)' in animated_emoji
        or 'Qt.callLater(() =>' in animated_emoji
        or 'property var scrollHost' not in animated_emoji
        or 'id: viewportTimer' not in animated_emoji):
    errors.append('Animated emoji must sample viewport state outside the contentY hot path and avoid delayed closures that can outlive recycled delegates')

if ('deduplicatedEmbeds' not in cast_embeds_panel
        or 'supportingWithoutVisualDuplicates' not in cast_embeds_panel):
    errors.append('Cast embed panel must deduplicate repeated URL/media embeds before laying out the grid')
rich_cast_renderer = (ui/'components'/'content'/'RichCastRenderer.qml').read_text(encoding='utf-8')
if ('suppressedEmbedUrls' not in rich_cast_renderer
        or 'blockSuppressed' not in rich_cast_renderer
        or 'suppressedEmbedUrls: root.protocolEmbedUrls' not in cast_item):
    errors.append('Protocol embed cards must suppress matching local rich-sidecar embed blocks so links are rendered once')

# Locale changes are live state changes. Both explicit Theme font bindings and
# controls inheriting the application font must invalidate without a restart.
if ('localeFontRevision' not in theme_text
        or 'const revision = localeFontRevision' not in theme_text):
    errors.append('Theme font bindings must depend on localization revision for live script/font changes')
if ('&LocalizationController::localeChanged' not in main_cpp
        or 'application.setFont(localeFont);' not in main_cpp):
    errors.append('Application default font must follow LocalizationController::localeChanged at runtime')
locale_format = (ui/'components'/'controls'/'LocaleFormat.qml').read_text(encoding='utf-8')
if ('pragma Singleton' not in locale_format
        or 'localizationController.revision' not in locale_format
        or 'function formatInteger' not in locale_format
        or 'function formatNumber' not in locale_format):
    errors.append('LocaleFormat must expose revision-aware reactive number/time formatting for runtime language changes')
for qml_path in ui.rglob('*.qml'):
    if qml_path.name == 'LocaleFormat.qml':
        continue
    qml_text = qml_path.read_text(encoding='utf-8')
    if re.search(r'localizationController\.(?:formatIdentifier|formatNumber|formatInteger|formatCompactNumber|calendarMonthModel|formatTimestamp|formatRelativeTime|formatDateTime|formatCurrency|formatMonetaryUnit|formatAssetQuote)\s*\(', qml_text):
        errors.append(f'Locale-sensitive formatter bypasses LocaleFormat revision facade: {qml_path.relative_to(root)}')
localization_cpp = (root/'src'/'presentation'/'qt'/'localization_controller.cpp').read_text(encoding='utf-8')
if 'qFatal(' in localization_cpp:
    errors.append('User-triggered locale/catalog recovery must never terminate the process with qFatal')

# Profile/cast/detail surfaces must not reintroduce the large empty vertical
# reservations that made wide profile and conversation views feel sparse.
profile_surface = (ui/'components'/'profile'/'ProfileSurface.qml').read_text(encoding='utf-8')
detail_surface = (ui/'components'/'feed'/'DetailSurface.qml').read_text(encoding='utf-8')
if ': (root.compact ? 56 : 64)' not in profile_surface:
    errors.append('Profile no-banner state must stay compact instead of reserving a decorative hero void')
if 'Theme.space9 + 72' in detail_surface:
    errors.append('DetailSurface must not append the legacy oversized bottom dead-space reservation')
if ('implicitHeight: messageRow.implicitHeight + Theme.space2' not in cast_item
        or 'implicitHeight: messageContent.implicitHeight + Theme.space2 * 2' not in cast_item):
    errors.append('CastItem vertical rhythm must retain the compact desktop density contract')

rendering_popup = (ui/'components'/'settings'/'RenderingPopup.qml').read_text(encoding='utf-8')
adaptive_dialog = (ui/'components'/'controls'/'GenyAdaptiveDialog.qml').read_text(encoding='utf-8')
accent_studio = (ui/'components'/'settings'/'AccentColorStudio.qml').read_text(encoding='utf-8')
feedback_dialog = (ui/'components'/'settings'/'FeedbackDialog.qml').read_text(encoding='utf-8')
if ('contentItem: ScrollView {' not in rendering_popup
        or 'readonly property int qualityColumns: compactLayout ? 1 : 3' not in rendering_popup
        or 'readonly property bool recommended: modelData.id === "adaptive"' not in rendering_popup
        or 'root.hostWidth < Theme.breakpointNarrow ? 620 : 780' not in rendering_popup
        or 'GenyAdaptiveDialog {' not in rendering_popup):
    errors.append('Rendering settings must remain responsive, mobile-drawer aware, and identify Adaptive as the recommended Qt-aligned default')
if ('GenyModal {' not in adaptive_dialog
        or 'GenyDrawer {' not in adaptive_dialog
        or 'bottomSheet: true' not in adaptive_dialog
        or 'compactPresentation: parent ? parent.width < Theme.breakpointMobile' not in adaptive_dialog
        or 'GenyAdaptiveDialog {' not in accent_studio
        or 'GenyAdaptiveDialog {' not in feedback_dialog):
    errors.append('Transient settings/feedback dialogs must use a true bottom Drawer on mobile and a centered modal on desktop')
locale_settings = (ui/'components'/'settings'/'SettingsPopup.qml').read_text(encoding='utf-8')
if ('readonly property bool mobilePresentation: viewportWidth > 0' not in locale_settings
        or 'viewportWidth < Theme.breakpointMobile' not in locale_settings
        or 'desktopFullScreen: true' not in locale_settings
        or 'mobileFullScreen: true' not in locale_settings):
    errors.append('Settings must use the semantic mobile breakpoint in both fullscreen layouts')

action_button = (ui/'components'/'controls'/'ActionButton.qml').read_text(encoding='utf-8')
apps_surface = (ui/'components'/'apps'/'AppsSurface.qml').read_text(encoding='utf-8')
task_manager = (ui/'components'/'apps'/'TaskManagerPopup.qml').read_text(encoding='utf-8')
if ('property bool positive: false' not in action_button
        or 'Theme.success' not in action_button
        or 'positive: cardCell.runtimeState === "foreground"' not in apps_surface
        or ('positive: taskCard.foreground' not in task_manager
            and 'positive: Boolean(taskCard.modelData.foreground)' not in task_manager)):
    errors.append('Focused Mini App actions must retain the positive/success visual state')
if ('compactPresentation: parent ? parent.width < Theme.breakpointMobile' not in task_manager
        or 'GenyModal {' not in task_manager
        or 'GenyDrawer {' not in task_manager
        or 'bottomSheet: true' not in task_manager):
    errors.append('Task Manager must present as a bottom drawer on mobile and a dialog on desktop')
if 'Layout.preferredHeight: contentRoot.listHeight' not in task_manager:
    errors.append('Task Manager list must remain content-driven instead of reserving a tall empty modal')

if ('const CGFloat panelWidth = MIN(width, 424.0);' not in miniapp_macos
        or 'MIN(755.0' not in miniapp_macos):
    errors.append('Native Mini App surface must preserve the Farcaster 424x695 web reference plus host chrome')
if 'kCACornerCurveContinuous' not in miniapp_macos:
    errors.append('Native macOS Mini App drawers must use continuous native corner curves')
if ('session.browser->ready(disableNativeGestures);' not in miniapp_runtime
        or 'QStringLiteral("disableNativeGestures")' not in miniapp_runtime):
    errors.append('Mini App ready() must honor the Farcaster disableNativeGestures option')
if ('QStringLiteral("open_miniapp")' not in miniapp_runtime
        or 'referrerDomain' not in miniapp_runtime
        or 'completeOpenMiniAppHandoff' not in miniapp_runtime):
    errors.append('openMiniApp must provide referral context and close the source only after successful target readiness')
capabilities_block = miniapp_runtime.split('[[nodiscard]] QJsonArray capabilities', 1)[1].split('[[nodiscard]] QJsonArray chains', 1)[0]
if 'actions.addMiniApp' in capabilities_block:
    errors.append('Host must not advertise addMiniApp until signed Farcaster server events and notification delivery exist')

profile_provider = (root/'src'/'providers'/'hypersnap'/'profile.cpp').read_text(encoding='utf-8')
async_profile_provider = (root/'src'/'providers'/'hypersnap'/'async_gateway.cpp').read_text(encoding='utf-8')
for provider_text in (profile_provider, async_profile_provider):
    if 'bannerUserDataType{"10"}' not in provider_text or 'currentBannerUserDataType{"11"}' in provider_text:
        errors.append('Farcaster banner enrichment must use canonical UserDataType 10 and must not reinterpret type 11 as a banner')
remote_image_provider = (ui/'app'/'media'/'remote_image.cpp').read_text(encoding='utf-8')
if 'appendIfSupported("image/avif")' in remote_image_provider or 'appendIfSupported("image/heic")' in remote_image_provider:
    errors.append('Remote social-image negotiation must stay on conservative WebP/PNG/JPEG-class formats instead of optional AVIF/HEIC readers that can fail on CDN variants')
if ('mediaCompatibilityAcceptHeader' not in remote_image_provider
        or 'compatibilityAttempt' not in remote_image_provider):
    errors.append('Remote social images must retry decode failures with a conservative JPEG/PNG-first CDN negotiation before returning the transparent sentinel')
superellipse_image = (ui/'components'/'visual'/'SuperellipseImage.qml').read_text(encoding='utf-8')
if ('property bool showLoadingIndicator: true' not in superellipse_image
        or 'readonly property bool loading:' not in superellipse_image
        or 'GenySpinner {' not in superellipse_image
        or 'layer.effect: root.maskActive ? maskEffectComponent : null' not in superellipse_image
        or 'id: maskEffectComponent' not in superellipse_image
        or 'MultiEffect {' not in superellipse_image
        or 'retainPreviousWhileLoading' not in superellipse_image):
    errors.append('Remote image surfaces must expose stable loading states, direct image-layer masking, and retain decoded content when supported')
if 'source: imageSource' in superellipse_image:
    errors.append('SuperellipseImage must not restore the hidden-image sibling MultiEffect proxy path used before Fixed 9')
if ('popupZ: 10000' not in accent_studio
        or 'Qt.callLater(function() { root.open() })' not in accent_studio
        or 'z: control.popupZ' not in adaptive_dialog
        or 'AccentColorStudio {' not in app_shell
        or 'onCustomAccentStudioRequested: color => accentColorStudio.openWith(color)' not in app_shell
        or 'AccentColorStudio {' in locale_settings):
    errors.append('Custom Accent must open as a top-level overlay sibling rather than a nested settings Drawer child')
link_preview = (ui/'components'/'media'/'LinkPreview.qml').read_text(encoding='utf-8')
embed_preview = (ui/'components'/'media'/'EmbedPreview.qml').read_text(encoding='utf-8')
if ('metadataManagedExternally' not in link_preview
        or 'externalPreview: root.linkMetadata' not in embed_preview
        or 'metadataManagedExternally: true' not in embed_preview):
    errors.append('Embed and Link previews must share one metadata resolution result instead of issuing duplicate requests')
if 'width / 6.0' in profile_surface or 'bannerAspectRatio' not in profile_surface:
    errors.append('Profile banners must keep a readable cover aspect instead of the legacy ultra-wide 6:1 strip')

# Setup sign-in ceremonies are independent of the selected read provider.
setup = (ui / 'components' / 'setup' / 'InitialSetup.qml').read_text(encoding='utf-8')
setup_card = (ui / 'components' / 'setup' / 'SetupActionCard.qml').read_text(encoding='utf-8')
if ('root.authenticationInProgress && root.authenticationRoute === "openproof"' not in setup
        or 'root.authenticationInProgress && root.authenticationRoute === "farcaster"' not in setup
        or 'readonly property string activeSignInRoute:' not in setup
        or 'busy: root.authenticationInProgress\n' in setup):
    errors.append('Setup sign-in spinner must be bound to its own auth route, not shared across providers')
# This onboarding uses a single shared eight-step animated progress indicator
# instead of the removed sidebar step-button list. Require visible selection,
# accessible progress and keyboard-navigable back action on the actual surface.
indicator = (ui / 'components' / 'setup' / 'SetupStepIndicator.qml').read_text(encoding='utf-8')
if ('id: setupProgress' not in setup
        or 'stepCount: root.progressCount' not in setup
        or 'currentStep: root.currentStep' not in setup
        or 'readonly property bool active: index === root.activeIndex' not in indicator
        or 'fillColor: marker.active ? Theme.accent' not in indicator
        or 'duration: Theme.reduceMotion ? 0 : Theme.motionStandard' not in indicator
        or 'id: mobileSetupNav' not in setup
        or 'anchors.top: root.phone ? mobileSetupNav.top : parent.top' not in setup):
    errors.append('Setup progress indicator and accessible back-navigation contract regressed')
if ('readonly property bool visuallyDisabled: !enabled && !busy' not in setup_card):
    errors.append('In-flight sign-in action must retain its primary visual state while non-clickable')
if (not re.search(
        r'qsTrId\("genycaster\.setup\.network_farcaster_category"\)\s*'
        r'\.arg\(qsTrId\("genycaster\.setup\.centralized_service_label"\)\)', setup)
        or not re.search(
        r'qsTrId\("genycaster\.setup\.network_hypersnap_category"\)\s*'
        r'\.arg\(qsTrId\("genycaster\.setup\.decentralized_label"\)\)', setup)):
    errors.append('Setup must distinguish centralized Farcaster from decentralized Hypersnap in translated network labels')

if errors:
    print('UI design guard failed:')
    for e in errors:
        print(' -', e)
    sys.exit(1)
print(f'UI design guard passed: {len(discovered)} QML files audited.')
