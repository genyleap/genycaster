# Genycaster Setup UI Contract

This contract applies the project Qt Quick UI rules from
`docs/agent/QT_QUICK_UI.md` in `thecompez/ai-for-modern-cpp` to the six-step
Genycaster onboarding flow.

## Design pass

- **Audience:** first-run Genycaster users on desktop and phone-sized windows.
- **Primary goal:** finish a valid local setup and reach an authenticated,
  Farcaster-linked social session.
- **Hierarchy:** one page heading, one short explanation, one primary task region,
  then the persistent setup footer.
- **Primary action:** one accented CTA per stage. Secondary actions are neutral or
  quiet. Semantic danger/warning colors are reserved for real status.
- **State model:** Welcome -> Interface -> Privacy -> Network -> Account -> Ready.
  Later stages remain gated by consent and authentication state.
- **RTL:** shell order, navigation affordances, controls and UI copy follow
  `Theme.uiDirection`. Mixed-language carousel content keeps a physical track,
  while each text label chooses its own direction/font.
- **Branding:** the approved in-app Genycaster/Geny mark is used as an interface
  asset. Theme accent tokens own interactive color; components do not invent
  black or hard-coded substitute primary colors.
- **Reuse:** Setup-specific behavior is composed from
  `SetupPageHeader`, `SetupLanguageSelector`, `SetupActionCard` and
  `SetupPrimaryButton`, plus the existing standard combo box and switch.
- **Task bounds:** compact content is capped at 640 px and wide desktop content at
  720 px. The desktop step rail appears only above the 1040 px breakpoint.
- **Overflow:** stage content is a vertical Flickable. Phone layouts stack
  multi-column controls. Persistent footer actions remain outside scrolling
  content.
- **Focus/accessibility:** interactive setup controls use keyboard focus,
  visible focus indication, and accessible names/descriptions. Selection is not
  conveyed by color alone.

## Visual acceptance matrix

Review at minimum:

| Width | Direction | Theme | Checks |
| --- | --- | --- | --- |
| 390 px | LTR | Light + Dark | no clipping, stacked controls, safe footer |
| 390 px | RTL | Light + Dark | semantic back/next directions, Persian alignment |
| 900 px | LTR + RTL | Light + Dark | compact desktop without step rail |
| 1280 px | LTR + RTL | Light + Dark | step rail on logical start edge, task column bounded |

Also verify long localized strings, keyboard traversal, legal-document scrolling,
disabled authentication states, node probing, and Ready summaries.

## Verification policy

A QML parse/lint pass and targeted unit tests are required for every change.
A full rendered GUI pass is required before calling the setup polished. If the
configured Qt/C++ toolchain blocks QML type generation or the application build,
rendered GUI status must be reported as **NOT VERIFIED** rather than inferred.

### Current verification state

| Surface | Evidence | Result |
| --- | --- | --- |
| Setup QML syntax | `qmlformat --ignore-settings` on modified setup components | PASS |
| Language selector lint | `qmllint` with the generated QML import root | PASS, except missing generated `farcaster_client.qmltypes` |
| Presentation setup/reset tests | `farcaster_presentation_icon_tests` | PASS |
| Strict module lint | `farcaster_client_qmllint_strict` | NOT VERIFIED: blocked before QML lint by the existing Qt 6.11.2 / GCC 16 C++ module failure in `emoji_runtime.cppm` |
| Rendered 390 / 900 / 1280 px, LTR / RTL, light / dark matrix | requires a successful graphical target | NOT VERIFIED |

The selector's horizontal carousel is intentionally a physical interaction
surface: its left control always moves toward the physically previous item and
its right control toward the physically next item. Surrounding shell layout,
copy alignment, back/continue semantics, sidebar placement, and progress order
follow `Theme.uiDirection`.
