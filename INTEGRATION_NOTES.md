# Integration Notes

## Safe merge order

1. Add `ui/foundations` and `ui/primitives`.
2. Replace existing scattered theme literals incrementally with `Theme` tokens.
3. Introduce `SuperellipseSurface` around composer/buttons/selected surfaces first.
4. Integrate `DesktopSidebar`, `ContentTabs`, and `MobileBottomNavigation` into the existing shell.
5. Replace the existing Home composition with `pages/HomePage.qml`, preserving the current page model and signals.
6. Replace `DemoSocialData` bindings section-by-section with real models.
7. Keep existing auth gating; connect visual action signals to current auth/write commands.
8. Only then roll the semantic presentation components across Explorer / Visuals / Conversations / Devs / Following.

## Model adapter recommendation

Do not make delegates know about Hypersnap/Farcaster transport DTO shapes directly. Normalize each existing model to presentation roles such as:

```text
identity.displayName
identity.username
identity.avatar
content.text
content.media
context.channel
metrics.replies
metrics.recasts
metrics.likes
state.liked
state.recasted
permissions.canInteract
```

Existing C++ models can expose these roles or a thin presentation proxy can map them.

## RTL / LTR details

- Global UI follows `Application.layoutDirection`.
- Rows and alignment mirror automatically.
- Directional icons mirror with the shell where appropriate.
- User-generated text should use `Text.AutoText`/natural direction where possible.
- Code, repository paths, hashes, URLs, token symbols, and numeric sparkline axes remain LTR.
- Do not reverse media itself.
- Prefer `Layout.*` and implicit sizing over fixed text widths.
