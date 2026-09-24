#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
errors: list[str] = []

def read(rel: str) -> str:
    return (root / rel).read_text(encoding='utf-8')

cmake = read('CMakeLists.txt')
ui_cmake = read('ui/CMakeLists.txt')
main = read('ui/mobile/main.cpp') + read('ui/mobile/runtime_composition.cppm') + read('ui/mobile/runtime_composition.cpp')
theme = read('ui/theme/Theme.qml')
side = read('ui/components/SideNavigation.qml')
bottom = read('ui/components/BottomNavigation.qml')
icon = read('ui/components/Icon.qml')
feed = read('src/providers/hypersnap/feed.cpp')

if not re.search(r'set\(FARCASTER_DEFAULT_APP_FID\s+"866408"', cmake):
    errors.append('Shipping source must compile the canonical Genycaster/Genyleap App FID 866408.')
if 'FARCASTER_DEFAULT_APP_FID STREQUAL "256" OR FARCASTER_DEFAULT_APP_FID STREQUAL "0"' not in cmake:
    errors.append('Legacy cached App FID values must migrate to the canonical 866408 identity.')
if 'requireExternalAppAttribution = true' not in main:
    errors.append('Shipping signer policy must reject viewer-owned/self client attribution.')
if 'protocol writes stay disabled rather than being attributed to the signed-in user' not in main:
    errors.append('Product signer wiring must fail closed when client attribution is not configured.')
if 'bundledPersianFontResources' not in main or 'QFontDatabase::addApplicationFont' not in main:
    errors.append('The Persian UI family must be loaded from application resources.')
if 'GENYCASTER_FETCH_UI_FONT' not in ui_cmake or 'qt_add_resources(farcaster_client genycaster_ui_fonts' not in ui_cmake:
    errors.append('CMake must acquire and embed the pinned UI font family.')
if 'bundledPersianFontFamily' not in theme:
    errors.append('Theme must prefer the bundled Persian family.')
for stale in (
    'Ignoring invalid runtime SIWF configuration; using the compiled deployment identity.',
    'Ignoring invalid Optimism RPC override; using the built-in public RPC default.',
):
    if stale in main:
        errors.append(f'Stale environment-warning noise remains: {stale}')
for route in ('channels', 'notifications', 'store', 'business', 'messages', 'bookmarks'):
    if f'routeRequested("{route}")' not in side:
        errors.append(f'Desktop roadmap route missing: {route}')
for token in ('bottom_nav.home', 'side_nav.explorer', 'bottom_nav.create', 'apps.title', 'settings.title'):
    if token not in bottom:
        errors.append(f'Five-destination mobile navigation is missing: {token}')
if ('Theme.iconSource(root.text, root.filled)' not in icon
        or 'icon.source: root.source' not in icon
        or 'Theme.iconGlyph(root.text)' not in icon):
    errors.append('Primary iconography must prefer semantic Solar SVGs and retain the bounded font fallback.')
if 'GENYCASTER_FETCH_ICON_FONT' not in ui_cmake or 'fa-regular-400.ttf' not in ui_cmake or 'fa-solid-900.ttf' not in ui_cmake:
    errors.append('CMake must acquire and embed the Regular + Solid Font Awesome icon families.')
if 'GENYCASTER_FETCH_SCRIPT_FONTS' not in ui_cmake or 'NotoSansDevanagari.ttf' not in ui_cmake or 'NotoSansCJK-Regular.ttc' not in ui_cmake:
    errors.append('Shipping UI must bundle Devanagari and CJK shaping coverage.')
if "viewerReactionRequestTimeout{7'500}" not in feed or '.timeout = viewerReactionRequestTimeout' not in feed:
    errors.append('Viewer reaction refresh must use the relaxed opportunistic timeout.')

if errors:
    print('vNext release contract: FAILED')
    for error in errors:
        print(' -', error)
    sys.exit(1)
print('vNext release contract: PASS')
