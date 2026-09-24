# Genycaster — Native Scene Graph & Media Upload Review (V5)

> **Historical note (V11 rendering update):** the V5 custom `SceneGraphStroke` and visual `SceneGraphSuperellipse` triangle/feather paths have been superseded for production visuals by `VectorStroke` and `Superellipse` built on `QtQuick.Shapes` with `Shape.CurveRenderer`. The old QSG types remain only for compatibility and mathematical containment. This document is retained as historical implementation context.

## Scope

This pass targets two concrete defects in the V4 presentation layer:

1. compact dynamic graphics such as calendar progress arcs and token sparklines were still routed through `QtQuick.Shapes`, producing backend/DPI-dependent edge quality and rebuilding path data in QML/JavaScript;
2. media upload presentation was a small status strip rather than a complete attachment workflow.

The work is based on `genycaster-alpha-1.0-flat-context-rail-v4.zip` and keeps the V4 rule that project-owned drop shadows remain disabled.

## Rendering architecture

Qt Quick already uses a retained scene graph for its standard visual items. The correct optimization boundary is therefore not “replace every QML item with C++”. Text, Image, layout, transforms and ordinary simple Qt Quick primitives remain scene-graph-backed. Product-owned vector geometry that requires precise curves, thin strokes, dynamic paths, or custom dashes now goes directly through a `QQuickItem`/QSG boundary.

### SceneGraphStroke

`ui/render/scene_graph_stroke_item.*` implements one retained custom stroke item with three paths:

- Sparkline
- Arc
- RoundedRectangle

The item:

- sets `QQuickItem::ItemHasContents`;
- creates and updates `QSGGeometryNode` geometry only from `updatePaintNode()`;
- emits triangle meshes rather than backend line primitives;
- creates round caps and joins geometrically;
- derives tessellation density from physical path length and `QQuickWindow::devicePixelRatio()`;
- adds a one-physical-pixel feather around stroke edges;
- writes premultiplied per-vertex RGBA values;
- explicitly enables `QSGMaterial::Blending` for the feather;
- reuses the scene-graph node between frames;
- lets spinner rotation remain a scene-graph transform instead of regenerating geometry.

`TokenSparkline`, `GenySpinner`, calendar event/progress arcs, and the media drop-zone dashed border now use this renderer.

### SceneGraphSuperellipse

`ui/render/scene_graph_superellipse_item.*` replaces the previous JavaScript-generated SVG path used by the shared `Superellipse` component.

The renderer:

- evaluates the n=4 Lamé continuous-corner geometry natively;
- adaptively tessellates each corner based on physical radius and DPR;
- builds fill and border triangle geometry directly;
- performs feathered antialiasing with premultiplied alpha and explicit blending;
- preserves per-corner enable/disable flags;
- implements mathematical `contains()` so hit testing follows the same geometry as rendering;
- keeps the old QML `Superellipse` API stable, including deferred elevation properties.

This changes all project-owned continuous-corner controls and surfaces at once because they already consume `Superellipse.qml`.

### Removed legacy drawing paths

The runtime UI/render tree now contains no:

- `import QtQuick.Shapes`
- `Shape { ... }`
- `PathSvg`
- `PathPolyline`
- `PathAngleArc`
- `Canvas { ... }`
- `QQuickPaintedItem`

`tools/check_ui_design.py` now rejects these regressions for product-owned vector drawing and verifies that both scene-graph renderers remain registered and use QSG geometry with blending.

### HiDPI image requests

`SuperellipseImage` now requests a decode size at least equal to:

`logical item size × Screen.devicePixelRatio`

Explicit larger source requests remain authoritative for hero/embed media. This removes the old case where a small preview could be decoded below physical Retina/HiDPI resolution.

## Media upload redesign

A new `MediaUploadPanel.qml` replaces the old single upload status surface.

### Drop target

- large clean drop area;
- continuous-corner neutral surface;
- native scene-graph dashed outline;
- hover/drag visual state;
- click-to-browse;
- external file drag-and-drop;
- existing localized image/video support text;
- no fabricated PDF/audio support beyond the current native upload contract.

### Multiple attachments

The FileDialog now uses multi-file selection. The existing `MediaUploadController` remains intentionally single-flight, so the composer queues selected URLs and uploads them sequentially. This preserves the native upload lifecycle and verification contract while allowing a batch selection.

When inserting after a rich-document block, the queue advances the insertion index after each successful media block so selected file order is retained.

### Active transfer row

The active transfer presents:

- HiDPI local image preview when applicable;
- file icon fallback;
- filename;
- real local file size before the first network progress event;
- lifecycle phase/error;
- slim progress bar;
- cancel;
- retry.

`MediaUploadController::beginUpload()` now records local `QFileInfo::size()` immediately instead of leaving the total byte count at zero until the first native progress callback.

### Attached media

Verified media already inserted into `RichComposerController` is projected back into the panel as attachment rows with:

- stable filename/URL identity;
- media type;
- open action;
- removal from the rich document;
- compact attachment tiles;
- add-more affordance.

The panel stays visible while the rich document contains media so attachment management does not disappear after upload completion.

## Design decisions intentionally not implemented

The visual reference includes generic documents, arbitrary web links, an audio waveform, and identity chips. Genycaster's current native media upload contract is explicitly image/video-oriented. Those unsupported capabilities were not faked in the UI. Links continue through the existing embed flow; PDF/audio upload needs an explicit application/domain/upload contract before it becomes selectable.

Project-owned shadows remain disabled as requested in V4.

## Verification

Static/runtime-contract checks executed after the change:

- `tools/check_ui_design.py` — PASS (79 QML files)
- `tools/check_localization_catalog.py` — PASS (519 referenced IDs, 12 locales)
- `tools/check_architecture.py` — PASS
- `tools/check_media_upload_contract.py` — PASS
- `tools/check_r5_release_contract.py` — PASS
- `tools/check_r7_release_contract.py` — PASS
- strict renderer audit for QtQuick.Shapes / Canvas / QQuickPaintedItem / legacy QML paths — PASS

`cmake --preset ui-debug` was also attempted. Configure reaches the project Qt dependency and stops because this sandbox does not contain a Qt 6 package configuration (`Qt6Config.cmake` / `qt6-config.cmake`). Consequently, a real Qt 6.11 compile, `qmllint`, and GPU screenshot pass still need to run on a machine with the project Qt SDK.

## Runtime qualification checklist

On the target Qt 6.11 setup, verify at minimum:

1. calendar arcs at 1× and Retina/2× DPR;
2. positive/negative token sparklines at 60–80 logical px width;
3. superellipse borders during hover/focus color animation;
4. spinner rotation with threaded render loop;
5. dashed drop target while dragging external files;
6. multi-file FileDialog selection and sequential upload order;
7. local preview sharpness at 1×/2×/3× DPR;
8. cancel/retry state transitions;
9. attached-media removal and draft persistence;
10. light/dark plus Persian RTL composer layout.
