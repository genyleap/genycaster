# Genycaster UI/UX Rebuild — 2026-08-29

This revision replaces the earlier patch-only presentation pass with a product-level UI/UX correction.

## Implemented

- Flat, content-first Farcaster-style cast timeline instead of heavy message-card chrome.
- Reply and quoted-cast surfaces no longer render raw cast ids or parent URLs.
- Missing reply/quote previews are hydrated cache-first in the background and patched into visible rows.
- Explore now uses one universal field for profiles, channels, Farcaster URLs, and canonical cast ids.
- Guest startup opens the public Explore feed and never restores a previously browsed identity as the current user.
- Personal Home/Profile navigation is only exposed for authenticated users.
- Guest interaction controls remain discoverable and route to authentication when used.
- Authenticated restored sessions bind to their verified account identity automatically.
- Profile cover is full-width with a responsive Farcaster-like aspect ratio.
- Wide layouts use adaptive feed/detail widths and avoid reserving a hidden account rail for guests.
- Healthy runtime status chrome is suppressed to preserve content space.
- Persian navigation terminology is normalized, including `کاوش`, `نمایه`, and `بازنشر`.
- UI design guard updated to enforce the new universal-search and flat-timeline contracts.

## Verification

Passed:

- `python3 tools/check_ui_design.py`
- `python3 tools/check_localization_catalog.py`
- `python3 tools/check_architecture.py`

A Qt UI configure was attempted with `cmake --preset ui-debug`. The current build environment does not contain Qt6, so CMake stops at `find_package(Qt6)` before compilation. No generated build directory is included in the archive.
