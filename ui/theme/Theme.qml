pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme
    // Palette changes are one atomic UI operation, not hundreds of independent
    // surface color animations competing with a frozen backdrop snapshot.
    property bool paletteSwitchInProgress: false
    property Timer paletteSwitchTimer: Timer {
        interval: 280
        repeat: false
        onTriggered: theme.paletteSwitchInProgress = false
    }
    function beginPaletteSwitch() {
        paletteSwitchInProgress = true
        paletteSwitchTimer.restart()
    }
    // Also cover system-wide appearance and palette changes outside Settings.
    onDarkChanged: beginPaletteSwitch()
    onColorThemeIdChanged: beginPaletteSwitch()
    // User-owned, device-local media preferences. Keep them independent of
    // reduced UI motion: accessibility and battery-saving have different goals.
    property Settings energyPreferences: Settings {
        category: "MediaPowerSave"
        property bool enabled: false
        property bool animateAvatars: true
        // Distinct from avatar GIFs and OS accessibility Reduce Motion.
        property bool animateEmoji: true
    }
    // The Neon Kit is a decorative, user-controlled surface treatment.
    // Neon is a distinct appearance choice, never silently disabled by a
    // separate energy-saving preference. Power saving suppresses animation
    // and dynamic sampling while retaining a static rim so the user can
    // actually see the selected visual style.
    property Settings neonPreferences: Settings {
        category: "NeonSurfaces"
        property bool enabled: true
    }
    readonly property bool neonEnabled: neonPreferences.enabled
    readonly property bool neonMotionEnabled: neonEnabled && !reduceMotion && !powerSave
    // Kit v0.4.5 material semantics, backed by GenyCaster's active theme.
    // A neutral ambient rim is intentionally quieter than an active glint.
    readonly property real neonControlStrength: dark ? 0.52 : 0.38
    readonly property real neonCardStrength: dark ? 0.78 : 0.48
    readonly property real neonPanelStrength: dark ? 0.48 : 0.32
    readonly property real neonAmbientStrength: dark ? 0.38 : 0.32
    readonly property real neonActivityStrength: dark ? 0.90 : 0.65
    readonly property real neonEdgeFalloff: 32
    readonly property real neonOrbitSpeed: 260
    readonly property real neonLightLuminance: 0.96
    readonly property real neonDarkLuminance: 0.10
    // GenyHub Kit's separate selection/hover timelines: no stale hover trail
    // when sweeping across adjacent tabs or menu entries.
    readonly property int materialSurfaceDuration: reduceMotion ? 0 : 105
    readonly property int materialHoverInDuration: reduceMotion ? 0 : 100
    readonly property int materialHoverOutDuration: reduceMotion ? 0 : 55
    readonly property int materialSelectionDuration: reduceMotion ? 0 : 145
    readonly property int neonSurfaceDuration: neonMotionEnabled ? 105 : 0
    readonly property int neonEnterDuration: neonMotionEnabled ? 100 : 0
    readonly property int neonExitDuration: neonMotionEnabled ? 90 : 0
    function setNeonEnabled(enabled) { neonPreferences.enabled = Boolean(enabled) }
    // Device-local, user-customizable mobile Action Button.
    // Persist only allowlisted capabilities, never executable commands.
    property Settings mobileActionPreferences: Settings {
        category: "MobileActionButton"
        property string action: "compose"
        // New installations follow the app accent; blue/coral remain opt-in.
        property string colorPreset: "theme"
    }
    readonly property string mobileAction: ["compose","photo","search","settings"].indexOf(
        mobileActionPreferences.action) >= 0 ? mobileActionPreferences.action : "compose"
    readonly property string mobileActionColorPreset: ["blue","coral","theme"].indexOf(
        mobileActionPreferences.colorPreset) >= 0 ? mobileActionPreferences.colorPreset : "blue"
    readonly property color mobileActionBlue: "#3266FA"
    readonly property color mobileActionCoral: "#FF5267"
    readonly property color mobileActionColor: mobileActionColorPreset === "coral"
        ? mobileActionCoral : mobileActionColorPreset === "theme" ? presetFill : mobileActionBlue
    readonly property string mobileActionIcon: mobileAction === "photo" ? iconImage
        : mobileAction === "search" ? iconSearch
        : mobileAction === "settings" ? iconSettings : iconCompose
    function setMobileAction(action) {
        if(["compose","photo","search","settings"].indexOf(action)<0) return false
        mobileActionPreferences.action=action
        return true
    }
    function setMobileActionColor(preset) {
        if(["blue","coral","theme"].indexOf(preset)<0) return false
        mobileActionPreferences.colorPreset=preset
        return true
    }
    readonly property bool powerSave: energyPreferences.enabled
    readonly property bool animateAvatars: energyPreferences.animateAvatars
    readonly property bool animateEmoji: energyPreferences.animateEmoji
    function setPowerSave(enabled) { energyPreferences.enabled = Boolean(enabled) }
    function setAnimateAvatars(enabled) { energyPreferences.animateAvatars = Boolean(enabled) }
    function setAnimateEmoji(enabled) { energyPreferences.animateEmoji = Boolean(enabled) }

    readonly property bool dark: appearanceController.effectiveDark
    // Onboarding artwork and native account decoration share the same
    // semantic palette in Light/Dark; no raw palette escapes into page QML.
    readonly property color setupHeroTop: dark ? "#111622" : "#F7F9FC"
    readonly property color setupHeroMiddle: dark ? "#1A2232" : "#EDF1F8"
    readonly property color setupHeroBottom: dark ? "#111824" : "#E6ECF6"
    readonly property color setupHeroPulseStroke: "#5385A5"
    readonly property color setupHeroTraceStroke: "#83B7D0"
    readonly property color setupHeroTitleInk: "#C5E9FA"
    readonly property color setupHeroCardBorder: dark ? "#35435C" : "#C8D2E3"
    readonly property color setupFooterDivider: dark ? "#57758F" : "#63829F"
    readonly property color setupFooterCopyrightInk: dark ? "#E2EAF4" : "#DFE8F4"
    readonly property color setupFooterVersionInk: dark ? "#A3B8CB" : "#AFC5D9"
    readonly property color verifiedGenyBadgeBackground: dark ? "#342812" : "#FFF6DE"
    readonly property color verifiedGenyBadgeBorder: "#D6A435"
    readonly property color verifiedGenyBadgeInk: dark ? "#F5CD66" : "#956A0B"
    // Product-specific visual ramps are design tokens, not private colors
    // scattered across QML components. Their values intentionally preserve
    // the previously shipped Light/Dark appearance.
    readonly property color miniAppWebBackground: "#101018"
    readonly property color featuredOverlayStart: "#22000000"
    readonly property color featuredOverlayMiddle: "#bb101119"
    readonly property color featuredOverlayEnd: "#e9101119"
    readonly property color featuredTextInk: "#ffffff"
    readonly property color featuredTextMuted: "#e4e5eb"
    readonly property color nativeSelectionAccent: dark ? "#0A84FF" : "#007AFF"
    readonly property color marketFearExtremeFear: "#E64F53"
    readonly property color marketFearFear: "#F58A2D"
    readonly property color marketFearNeutral: "#F9CA3A"
    readonly property color marketFearGreed: "#8BCD34"
    readonly property color marketFearExtremeGreed: "#26B98B"
    readonly property color marketChangeDownInk: dark ? "#FF9EA4" : "#DB4853"
    readonly property color marketChangeUpInk: dark ? "#6FDEA4" : "#168F6A"
    readonly property color marketSeasonWeak: "#EF9022"
    readonly property color marketSeasonCooling: "#F8C58E"
    readonly property color marketSeasonNeutral: "#CCD3E2"
    readonly property color marketSeasonWarming: "#91A5F4"
    readonly property color marketTrendDownInk: dark ? "#FF9299" : "#E64B57"
    readonly property color marketTrendUpInk: dark ? "#63D8A4" : "#1BAC7F"
    readonly property color marketTrendDownSurface: dark ? "#3D232A" : "#FFF0F1"
    readonly property color marketTrendUpSurface: dark ? "#1D3B31" : "#E9F8F0"
    readonly property color dockDarkDisabledInk: "#747781"
    readonly property color dockDarkSelectedInk: "#ffffff"
    readonly property color dockDarkIdleInk: "#cbd0d9"
    readonly property color dockSelectedWash: dark ? "#24FFFFFF" : "#1E252C3C"
    readonly property color dockMaskWhite: "#FFFFFF"
    readonly property color dockShadowTint: dark ? "#40000000" : "#1B192236"
    readonly property color connectionIconBackground: "#FFFFFF"
    readonly property color connectionIconBorder: "#E8EAEE"
    readonly property var githubContributionRamp: dark
        ? ["#232A27","#0E4429","#006D32","#26A641","#39D353"]
        : ["#E8EBE9","#9BE9A8","#40C463","#30A14E","#216E39"]
    readonly property color pulseTileEmpty: dark ? "#282A31" : "#E6E8EC"
    readonly property var pulseGithubRamp: ["#B5E8D0","#68CA9B","#32966C"]
    readonly property var pulseFarcasterRamp: ["#C8B8F8","#9D81EA","#6552BD"]
    readonly property color pulseFarcasterIconSurface: dark ? "#30283E" : "#EEE9FA"
    readonly property color pulseFarcasterIconInk: dark ? "#C8B8F8" : "#745BC8"
    readonly property color pulseUnobservedTile: dark ? "#20232B" : "#EFF0F3"
    readonly property color pulseBrandIconBackground: "#FFFFFF"
    readonly property color pulseGithubRangeBorder: dark ? "#BDECD1" : "#28865E"
    readonly property color feedbackMoodFrustrated: "#E66B66"
    readonly property color feedbackMoodSad: "#E8A65D"
    readonly property color feedbackMoodNeutral: "#D3AD55"
    readonly property color feedbackMoodPositive: "#59B89A"
    readonly property color feedbackMoodExcited: "#62B7C8"
    readonly property color setupActionEmphasisInk: "#FCFCFE"
    readonly property color setupActionEmphasisMutedInk: "#BABBC6"
    readonly property color setupActionUnavailableSurface: dark ? "#23272E" : "#F5F6F8"
    readonly property color setupActionEmphasisSurface: dark ? "#24252C" : "#22232A"
    readonly property color setupActionUnavailableBorder: dark ? "#3C444E" : "#E4E7EB"
    readonly property bool reduceMotion: appearanceController.reduceMotion
    readonly property string mode: appearanceController.themeMode
    readonly property string colorThemeId: appearanceController.colorThemeId
    readonly property string platformName: appearanceController.platformName
    readonly property color platformAccentColor: appearanceController.platformAccentColor
    readonly property string platformAccentHex: appearanceController.platformAccentColor
    readonly property string platformIcon: appearanceController.platformIcon
    readonly property real fontScale: appearanceController.fontScale
    readonly property real interfaceScale: appearanceController.interfaceScale
    readonly property bool rtl: localizationController.rightToLeft
    readonly property int uiDirection: rtl ? Qt.RightToLeft : Qt.LeftToRight
    readonly property int textStartAlignment: rtl ? Text.AlignRight : Text.AlignLeft
    readonly property int textEndAlignment: rtl ? Text.AlignLeft : Text.AlignRight

    // Mouse/touch focus must not create a bright outline after a click.
    // Keep a visible ring for keyboard navigation on desktop and WASM.
    function keyboardFocusVisible(control) {
        return control.activeFocus
            && (control.focusReason === Qt.TabFocusReason
                || control.focusReason === Qt.BacktabFocusReason)
    }

    function contrastInk(backgroundColor) {
        const luminance = 0.2126 * backgroundColor.r + 0.7152 * backgroundColor.g + 0.0722 * backgroundColor.b
        return luminance > 0.72 ? "#17181B" : "#FFFFFF"
    }

    // Visual System 3.0: neutral materials, crisp ink and restrained chroma.
    // The references consistently use optical depth and tonal hierarchy instead of
    // tinted dashboard panels, so canvas/surface roles stay deliberately neutral.
    readonly property color dayCanvas: "#F1F1F3"
    readonly property color daySurface: "#FFFFFF"
    readonly property color daySurfaceMuted: "#F7F7F8"
    readonly property color daySurfaceRaised: "#FFFFFF"
    readonly property color daySurfaceElevated: "#FFFFFF"
    readonly property color daySurfaceHover: "#F1F2F4"
    readonly property color daySurfacePressed: "#E9EAED"
    readonly property color dayInk: "#17181B"
    readonly property color dayInkMuted: "#60636A"
    readonly property color dayInkSubtle: "#898D95"
    readonly property color dayInkDisabled: "#B4B7BE"
    readonly property color dayBorder: "#DFE1E5"
    readonly property color dayBorderMuted: "#ECEDEF"
    readonly property color dayBorderStrong: "#CDD0D6"
    readonly property color dayDivider: "#ECEDEF"

    readonly property color nightCanvas: "#0B0C0F"
    readonly property color nightSurface: "#121317"
    readonly property color nightSurfaceMuted: "#17191E"
    readonly property color nightSurfaceRaised: "#1B1D23"
    readonly property color nightSurfaceElevated: "#202229"
    readonly property color nightSurfaceHover: "#23262D"
    readonly property color nightSurfacePressed: "#2A2D35"
    readonly property color nightInk: "#F7F7F8"
    readonly property color nightInkMuted: "#AFB2BA"
    readonly property color nightInkSubtle: "#858A94"
    readonly property color nightInkDisabled: "#5F646E"
    readonly property color nightBorder: "#292C33"
    readonly property color nightBorderMuted: "#22252B"
    readonly property color nightBorderStrong: "#3A3E47"
    readonly property color nightDivider: "#22252B"

    // Accent presets are saturated only for interaction, status and focus.
    readonly property color accentGenyDay: "#315CF5"
    readonly property color accentGenyNight: "#829AFF"
    readonly property color accentOceanDay: "#1289A8"
    readonly property color accentOceanNight: "#63C6DE"
    readonly property color accentVioletDay: "#7047EB"
    readonly property color accentVioletNight: "#AB91FF"
    readonly property color accentEmberDay: "#C95B35"
    readonly property color accentEmberNight: "#F39A77"
    readonly property color accentRoseDay: "#D83E8A"
    readonly property color accentRoseNight: "#FF79B7"
    readonly property color accentMintDay: "#0F8F78"
    readonly property color accentMintNight: "#61D7BD"
    readonly property color accentMonoDay: "#17181B"
    readonly property color accentMonoNight: "#F4F5F8"
    readonly property color customAccent: appearanceController.customAccentColor
    readonly property color customAccentQuickRose: "#FF4FA3"
    readonly property color customAccentQuickBlush: "#FF73C4"
    readonly property color customAccentQuickOrchid: "#B45CFF"
    readonly property color customAccentQuickMint: "#28C6A5"
    readonly property color customAccentQuickAmber: "#F7B32B"
    readonly property color customAccentQuickInk: "#15161A"
    readonly property color colorPickerWhite: "#FFFFFF"
    readonly property color colorPickerTransparentWhite: "#00FFFFFF"
    readonly property color colorPickerTransparentBlack: "#00000000"
    readonly property color colorPickerBlack: "#FF000000"
    readonly property color colorPickerRed: "#FF0000"
    readonly property color colorPickerYellow: "#FFFF00"
    readonly property color colorPickerGreen: "#00FF00"
    readonly property color colorPickerCyan: "#00FFFF"
    readonly property color colorPickerBlue: "#0000FF"
    readonly property color colorPickerMagenta: "#FF00FF"
    readonly property color colorPickerShadow: "#66000000"

    readonly property color fillGenyDay: "#315CF5"
    readonly property color fillGenyNight: "#526FFF"
    readonly property color fillOceanDay: "#1289A8"
    readonly property color fillOceanNight: "#2798B2"
    readonly property color fillVioletDay: "#7047EB"
    readonly property color fillVioletNight: "#8165EE"
    readonly property color fillEmberDay: "#C95B35"
    readonly property color fillEmberNight: "#D17250"
    readonly property color fillRoseDay: "#D83E8A"
    readonly property color fillRoseNight: "#E85E9D"
    readonly property color fillMintDay: "#0F8F78"
    readonly property color fillMintNight: "#2FAE96"
    readonly property color fillMonoDay: "#17181B"
    readonly property color fillMonoNight: "#F4F5F8"

    readonly property color presetAccent: colorThemeId === "platform" ? platformAccentColor
                                                 : colorThemeId === "ocean" ? (dark ? accentOceanNight : accentOceanDay)
                                                : colorThemeId === "violet" ? (dark ? accentVioletNight : accentVioletDay)
                                                : colorThemeId === "ember" ? (dark ? accentEmberNight : accentEmberDay)
                                                : colorThemeId === "rose" ? (dark ? accentRoseNight : accentRoseDay)
                                                : colorThemeId === "mint" ? (dark ? accentMintNight : accentMintDay)
                                                : colorThemeId === "mono" ? (dark ? accentMonoNight : accentMonoDay)
                                                : colorThemeId === "custom" ? customAccent
                                                : (dark ? accentGenyNight : accentGenyDay)
    readonly property color presetFill: colorThemeId === "platform" ? platformAccentColor
                                               : colorThemeId === "ocean" ? (dark ? fillOceanNight : fillOceanDay)
                                              : colorThemeId === "violet" ? (dark ? fillVioletNight : fillVioletDay)
                                              : colorThemeId === "ember" ? (dark ? fillEmberNight : fillEmberDay)
                                              : colorThemeId === "rose" ? (dark ? fillRoseNight : fillRoseDay)
                                              : colorThemeId === "mint" ? (dark ? fillMintNight : fillMintDay)
                                              : colorThemeId === "mono" ? (dark ? fillMonoNight : fillMonoDay)
                                              : colorThemeId === "custom" ? customAccent
                                              : (dark ? fillGenyNight : fillGenyDay)

    readonly property color swatchPlatform: platformAccentColor
    readonly property color swatchGeny: dark ? accentGenyNight : accentGenyDay
    readonly property color swatchOcean: dark ? accentOceanNight : accentOceanDay
    readonly property color swatchViolet: dark ? accentVioletNight : accentVioletDay
    readonly property color swatchEmber: dark ? accentEmberNight : accentEmberDay
    readonly property color swatchRose: dark ? accentRoseNight : accentRoseDay
    readonly property color swatchMint: dark ? accentMintNight : accentMintDay
    readonly property color swatchMono: dark ? accentMonoNight : accentMonoDay
    readonly property color swatchCustom: customAccent

    readonly property color background: dark ? nightCanvas : dayCanvas
    readonly property color canvas: background
    readonly property color surface: dark ? nightSurface : daySurface
    readonly property color surfaceMuted: dark ? nightSurfaceMuted : daySurfaceMuted
    readonly property color surfaceRaised: dark ? nightSurfaceRaised : daySurfaceRaised
    readonly property color surfaceElevated: dark ? nightSurfaceElevated : daySurfaceElevated
    // Pair material and foreground: a page can contain black media in Light
    // mode or white media in Dark mode. Keep readable ink at both extremes
    // while retaining the blurred backdrop's color and movement.
    readonly property color navigationGlassTint: dark ? "#C21B1D23" : "#B8FFFFFF"
    readonly property color navigationGlassInk: dark ? "#F7F7F8" : "#17181B"
    readonly property color navigationGlassInkMuted: dark ? "#C7CBD3" : "#424750"
    readonly property color surfaceHover: dark ? nightSurfaceHover : daySurfaceHover
    readonly property color surfacePressed: dark ? nightSurfacePressed : daySurfacePressed
    readonly property color contextRailCanvas: dark ? "#0F1014" : "#E7E8EF"
    readonly property color contextRailCard: dark ? "#17191E" : "#FFFFFF"
    readonly property color contextRailInset: dark ? "#20232A" : "#F4F5F8"
    readonly property color contextActivityAccent: dark ? "#60D7B2" : "#34C99A"
    readonly property color contextActivitySurface: dark ? "#173129" : "#E8F8F2"
    readonly property color cryptoBitcoinSurface: "#F7931A"
    readonly property color cryptoEthereumSurface: dark ? "#29324D" : "#E7EBF8"
    readonly property color cryptoEthereumGlyph: dark ? "#A9B7E8" : "#52628F"
    readonly property color cryptoGenySurface: dark ? "#071B1C" : "#DDF7F3"
    readonly property color surfaceSelected: dark
        ? Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.24)
        : Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.09)
    readonly property color messageSurface: surface
    readonly property color messageSurfaceHover: dark ? "#181A20" : "#FCFCFD"
    readonly property color ownMessageSurface: messageSurface
    readonly property color ownMessageSurfaceHover: messageSurfaceHover
    readonly property color messageBorder: dark ? "#24272E" : "#ECEDEF"
    readonly property color messageBorderHover: dark ? "#343840" : "#DADCE1"

    readonly property color ink: dark ? nightInk : dayInk
    readonly property color inkMuted: dark ? nightInkMuted : dayInkMuted
    readonly property color inkSubtle: dark ? nightInkSubtle : dayInkSubtle
    readonly property color inkDisabled: dark ? nightInkDisabled : dayInkDisabled
    readonly property color inkInverse: "#FFFFFF"

    readonly property color primary: presetFill
    readonly property color primaryFill: presetFill
    readonly property color accent: presetAccent
    readonly property color accentHover: dark ? Qt.lighter(presetAccent, 1.05) : Qt.darker(presetAccent, 1.04)
    readonly property color primaryFillHover: dark ? Qt.lighter(presetFill, 1.06) : Qt.darker(presetFill, 1.035)
    readonly property color primaryPressed: Qt.darker(presetFill, 1.10)
    readonly property color accentInk: contrastInk(presetFill)
    readonly property color primarySurface: dark
        ? Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.18)
        : Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.095)
    readonly property color primarySurfaceHover: dark
        ? Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.27)
        : Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.15)

    readonly property color focus: presetAccent
    readonly property color focusHalo: dark
        ? Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.34)
        : Qt.rgba(presetAccent.r, presetAccent.g, presetAccent.b, 0.20)
    readonly property color focusOnAccent: accentInk

    // Keep the primary analytics series on the configured accent. Secondary
    // series use neutral greys with separate light/dark contrast, so charts
    // remain legible without adding unrelated saturated accent colors.
    readonly property color analyticsRecasts: dark ? "#98A0AE" : "#6B7280"
    readonly property color analyticsReplies: dark ? "#C2C7D0" : "#9CA3AF"

    readonly property color warning: dark ? "#F2B75E" : "#A76208"
    readonly property color warningSurface: dark ? "#2E281E" : "#FFF4E5"
    // Verified achievements use a distinct ember-orange palette, not the
    // global warning semantic or the configurable primary accent.
    readonly property color streak: dark ? "#FF995F" : "#B94716"
    readonly property color streakSurface: dark ? "#38251F" : "#FFF0E7"
    readonly property color streakTrack: dark ? "#634234" : "#F1D5C6"
    readonly property color streakGlow: dark ? "#FFAA76" : "#E76E36"
    // Verified Pro uses a quieter copper tone beside the ember Streak.
    // These semantic colors remain consistent when the user changes accents.
    readonly property color proAccent: dark ? "#FFB184" : "#AA451C"
    readonly property color proSurface: dark ? "#35251F" : "#FFF3EA"
    readonly property color proSurfaceHover: dark ? "#433026" : "#FFEBDC"
    readonly property color proBorder: dark ? "#885741" : "#E6B99E"
    readonly property color success: dark ? "#61D194" : "#22845A"
    readonly property color successSurface: dark ? "#1B3026" : "#EAF7F0"
    readonly property color danger: dark ? "#FF7B83" : "#D94D57"
    readonly property color dangerInk: contrastInk(danger)
    readonly property color dangerSurface: dark ? "#342126" : "#FDEDEF"

    // Notification kind accents keep each activity type instantly recognizable,
    // both in the kind chip and in the sub-message card that carries the cast text.
    readonly property color notificationReply: dark ? "#8FB4FF" : "#3B5BDB"
    readonly property color notificationMention: dark ? "#C9A6FF" : "#7A4FD0"
    readonly property color notificationReaction: dark ? "#FF97AC" : "#E25574"
    // Recasts are shares, not likes: amber keeps them visually distinct from
    // pink reactions and green follows in the notification stream.
    readonly property color notificationRecast: dark ? "#F0BE6B" : "#B2650E"
    readonly property color notificationFollow: dark ? "#61D194" : "#22845A"
    readonly property color notificationOther: dark ? "#9AA3B2" : "#5D6470"

    readonly property color border: dark ? nightBorder : dayBorder
    readonly property color borderMuted: dark ? nightBorderMuted : dayBorderMuted
    readonly property color borderStrong: dark ? nightBorderStrong : dayBorderStrong
    readonly property color divider: dark ? nightDivider : dayDivider
    readonly property color overlay: dark ? "#A6000000" : "#5C101114"
    readonly property color drawerOverlay: dark ? "#B8000000" : "#73E6E7EA"
    readonly property color shadow: dark ? "#B8000000" : "#590C0E13"
    readonly property color shadowSoft: dark ? "#8A000000" : "#330C0E13"
    readonly property color mediaOverlay: dark ? "#B8000000" : "#CCFFFFFF"
    readonly property color mediaOverlayBorder: dark ? "#44FFFFFF" : border
    // Full-screen media viewer intentionally uses a stable dark environment in both themes.
    readonly property color mediaViewerModal: "#D9000000"
    readonly property color mediaViewerBackground: "#F20A0B0E"
    readonly property color mediaViewerInk: "#E8FFFFFF"
    readonly property color mediaViewerInkMuted: "#B8FFFFFF"
    readonly property color mediaViewerControl: "#24FFFFFF"
    readonly property color mediaViewerControlHover: "#3DFFFFFF"
    readonly property color mediaViewerBorder: "#55FFFFFF"
    readonly property color mediaViewerThumbnail: "#221F2228"
    // Inline video keeps stable dark chrome in both themes. These tokens are
    // deliberately distinct from generic overlays, which can be light in the
    // day theme and lose contrast over bright video frames.
    readonly property color mediaPlayerBackground: "#FF080A0D"
    readonly property color mediaPlayerChrome: "#C716181D"
    readonly property color mediaPlayerChromeHover: "#E01D2027"
    readonly property color mediaPlayerBorder: "#48FFFFFF"
    readonly property color mediaPlayerInk: "#F5FFFFFF"
    readonly property color mediaPlayerInkMuted: "#B8FFFFFF"
    readonly property color mediaPlayerScrim: "#D9000000"
    readonly property color mediaPlayerFrame: "#24FFFFFF"
    readonly property color mediaPlayerAction: "#EB20242C"
    readonly property color mediaPlayerActionHover: "#F52C313B"
    readonly property real mediaPlayerControlSize: 40
    readonly property real mediaPlayerControlIconSize: 20
    readonly property real mediaPlayerActionHeight: 52
    readonly property real mediaPlayerInset: 16
    // Hover is intentionally a state layer rather than a replacement card color.
    readonly property color castHover: dark ? "#10FFFFFF" : "#080D1016"
    readonly property color editorSurface: dark ? "#15171C" : "#FAFAFB"
    readonly property color editorFocusedSurface: dark ? "#1A1D23" : "#FFFFFF"
    readonly property color toolbarSurface: dark ? "#17191E" : "#F8F8F9"
    readonly property color previewSurface: dark ? "#14161B" : "#FCFCFD"
    readonly property color codeSurface: dark ? "#101217" : "#F5F5F7"
    readonly property color codeInlineSurface: dark ? "#22252C" : "#EAEBEE"
    readonly property color codeBorder: dark ? "#2A2D35" : "#DFE1E5"
    readonly property color codeInk: dark ? "#E6E7EB" : "#202228"
    readonly property color codeLabel: dark ? "#AEB4C0" : "#5D6470"
    // QML eight-digit colors use #AARRGGBB (alpha first), unlike CSS #RRGGBBAA.
    readonly property color backdropScrim: dark ? "#4807111B" : "#38DCE5EC"
    readonly property color drawerModalScrim: dark ? "#3207111B" : "#28DCE5EC"

    readonly property color syntaxKeyword: dark ? "#C7A0FF" : "#7652B8"
    readonly property color syntaxType: dark ? "#76D6C4" : "#147E70"
    readonly property color syntaxFunction: dark ? "#8DCBFF" : "#146FA8"
    readonly property color syntaxVariable: codeInk
    readonly property color syntaxString: dark ? "#A9D884" : "#4F7C32"
    readonly property color syntaxNumber: dark ? "#F0BB7A" : "#A36015"
    readonly property color syntaxComment: dark ? "#708597" : "#71808E"
    readonly property color syntaxOperator: dark ? "#D8E2EA" : "#4A5965"
    readonly property color syntaxAttribute: dark ? "#F0A9C1" : "#A84C70"
    readonly property color syntaxPreprocessor: dark ? "#E6A871" : "#A45B21"
    readonly property color syntaxNamespace: dark ? "#83C8DF" : "#24758F"
    readonly property color syntaxProperty: dark ? "#9ABCE8" : "#3B69A1"
    readonly property color syntaxLiteral: syntaxNumber
    readonly property color syntaxTag: dark ? "#E594A8" : "#A23F59"
    readonly property color syntaxCommand: syntaxKeyword
    readonly property color syntaxUrl: accent
    readonly property color syntaxHeader: syntaxType
    readonly property color syntaxParameter: dark ? "#D7C08D" : "#876A28"

    readonly property string latinFontFamily: typeof platformTextFontFamily !== "undefined"
                                               && platformTextFontFamily.length > 0
                                               ? platformTextFontFamily : "Sans Serif"
    readonly property string persianFontFamily: typeof bundledPersianFontFamily !== "undefined"
                                                 && bundledPersianFontFamily.length > 0
                                                 ? bundledPersianFontFamily
                                                 : (typeof platformRtlFontFamily !== "undefined"
                                                    && platformRtlFontFamily.length > 0
                                                    ? platformRtlFontFamily : latinFontFamily)
    readonly property string devanagariFontFamily: typeof bundledDevanagariFontFamily !== "undefined"
                                                   && bundledDevanagariFontFamily.length > 0
                                                   ? bundledDevanagariFontFamily
                                                   : (typeof platformDevanagariFontFamily !== "undefined"
                                                      && platformDevanagariFontFamily.length > 0
                                                      ? platformDevanagariFontFamily : latinFontFamily)
    readonly property string japaneseFontFamily: typeof bundledJapaneseFontFamily !== "undefined"
                                                  && bundledJapaneseFontFamily.length > 0
                                                  ? bundledJapaneseFontFamily
                                                  : (typeof platformJapaneseFontFamily !== "undefined"
                                                     && platformJapaneseFontFamily.length > 0
                                                     ? platformJapaneseFontFamily : latinFontFamily)
    readonly property string koreanFontFamily: typeof bundledKoreanFontFamily !== "undefined"
                                                && bundledKoreanFontFamily.length > 0
                                                ? bundledKoreanFontFamily
                                                : (typeof platformKoreanFontFamily !== "undefined"
                                                   && platformKoreanFontFamily.length > 0
                                                   ? platformKoreanFontFamily : latinFontFamily)
    readonly property string chineseFontFamily: typeof bundledChineseFontFamily !== "undefined"
                                                 && bundledChineseFontFamily.length > 0
                                                 ? bundledChineseFontFamily
                                                 : (typeof platformChineseFontFamily !== "undefined"
                                                    && platformChineseFontFamily.length > 0
                                                    ? platformChineseFontFamily : latinFontFamily)
    readonly property string codeFontFamily: Qt.platform.os === "osx" ? "Menlo" : (Qt.platform.os === "windows" ? "Cascadia Mono" : "monospace")
    readonly property string iconFontFamily: typeof bundledIconFontFamily !== "undefined"
                                              && bundledIconFontFamily.length > 0
                                              ? bundledIconFontFamily : latinFontFamily
    readonly property string iconRegularFamily: typeof bundledIconRegularFontFamily !== "undefined"
                                                && bundledIconRegularFontFamily.length > 0
                                                ? bundledIconRegularFontFamily : iconFontFamily

    function iconFamily(name) {
        return iconUsesRegular(name) ? iconRegularFamily : iconFontFamily
    }

    readonly property real localeFontRevision: Number(localizationController.revision)
    readonly property string fontFamily: {
        const revision = localeFontRevision
        return languageFamily(localizationController.effectiveLanguageTag)
    }

    function languageFamily(languageTag) {
        const tag = String(languageTag || "").toLowerCase()
        if (tag === "fa" || tag === "ar") return persianFontFamily
        if (tag === "hi") return devanagariFontFamily
        if (tag === "ja") return japaneseFontFamily
        if (tag === "ko") return koreanFontFamily
        if (tag === "zh") return chineseFontFamily
        return latinFontFamily
    }

    function fontFamilyForText(value) {
        const revision = localeFontRevision
        const text = String(value || "")
        for (let i = 0; i < text.length; ++i) {
            const code = text.charCodeAt(i)
            if ((code >= 0x0600 && code <= 0x06ff) || (code >= 0x0750 && code <= 0x077f) ||
                (code >= 0x08a0 && code <= 0x08ff)) return persianFontFamily
            if (code >= 0x0900 && code <= 0x097f) return devanagariFontFamily
            if ((code >= 0x3040 && code <= 0x30ff) || (code >= 0x31f0 && code <= 0x31ff)) return japaneseFontFamily
            if ((code >= 0xac00 && code <= 0xd7af) || (code >= 0x1100 && code <= 0x11ff)) return koreanFontFamily
            if ((code >= 0x3400 && code <= 0x4dbf) || (code >= 0x4e00 && code <= 0x9fff)) {
                const uiTag = String(localizationController.effectiveLanguageTag || "")
                if (uiTag === "ja") return japaneseFontFamily
                if (uiTag === "ko") return koreanFontFamily
                return chineseFontFamily
            }
        }
        return fontFamily
    }

    readonly property string iconHome: "home"
    readonly property string iconCalendar: "calendar"
    readonly property string iconUsers: "users"
    readonly property string iconChart: "chart"
    readonly property string iconExplore: "compass"
    readonly property string iconApps: "apps"
    readonly property string iconCompass: "compass"
    readonly property string iconProfile: "user"
    readonly property string iconSettings: "settings"
    readonly property string iconBack: "back"
    readonly property string iconClose: "close"
    readonly property string iconCheck: "check"
    readonly property string iconCompose: "compose"
    readonly property string iconReply: "reply"
    readonly property string iconComment: "message"
    readonly property string iconHeart: "heart"
    readonly property string iconRepost: "repeat"
    readonly property string iconShare: "share"
    readonly property string iconGlobe: "globe"
    readonly property string iconPalette: "palette"
    readonly property string iconLanguage: "language"
    readonly property string iconSun: "sun"
    readonly property string iconMoon: "moon"
    readonly property string iconSystem: "monitor"
    readonly property string iconCpu: "system/cpu"
    readonly property string iconAccessibility: "accessibility"
    readonly property string iconChevronDown: "chevron-down"
    readonly property string iconArrowRight: "arrow-right"
    readonly property string iconArrowLeft: "arrow-left"
    readonly property string iconRefresh: "refresh"
    readonly property string iconWifi: "wifi"
    readonly property string iconCloud: "cloud"
    readonly property string iconWarning: "warning"
    readonly property string iconInfo: "info"
    readonly property string iconLeaf: "brands/geny"
    readonly property string iconSignIn: "sign-in"
    readonly property string iconMore: "more"
    readonly property string iconSort: "sort"
    readonly property string iconWallet: "wallet"
    readonly property string iconPlay: "play"
    readonly property string iconPause: "pause"
    readonly property string iconCopy: "copy"
    readonly property string iconUser: "user"
    readonly property string iconImage: "image"
    readonly property string iconSearch: "search"
    readonly property string iconArrowUpRight: "external-link"
    readonly property string iconLink: "link"
    readonly property string iconMention: "mention"
    readonly property string iconLocation: "pin"
    readonly property string iconSpark: "spark"
    readonly property string iconBookmark: "bookmark"
    readonly property string iconPriorityStar: "priority-star"
    readonly property string iconStore: "store"
    readonly property string iconNotification: "bell"
    readonly property string iconMessage: "message"
    readonly property string iconBusiness: "briefcase"
    readonly property string iconBold: "bold"
    readonly property string iconItalic: "italic"
    readonly property string iconCode: "code"
    readonly property string iconQuote: "quote"
    readonly property string iconList: "list"
    readonly property string iconOrderedList: "ordered-list"
    readonly property string iconHeading: "heading"
    readonly property string iconSeparator: "separator"
    readonly property string iconChevronUp: "chevron-up"
    readonly property string iconPreview: "preview"
    readonly property string iconSend: "send"
    readonly property string iconPlus: "plus"
    readonly property string iconVolume: "volume"
    readonly property string iconVolumeMuted: "volume-muted"
    // A distinct, stroke-consistent status icon family. Do Not Disturb is a
    // prohibition sign rather than audio mute; Away and Idle differ visually.
    readonly property string iconStatusAvailable: "status/available"
    readonly property string iconStatusBusy: "status/busy"
    readonly property string iconStatusIdle: "status/idle"
    readonly property string iconStatusAway: "status/away"
    readonly property string iconStatusFocused: "status/focusing"
    readonly property string iconStatusCreating: "status/creating"
    readonly property string iconStatusDoNotDisturb: "status/dnd"
    readonly property string iconStatusCustom: "status/custom"
    readonly property string iconStatusClear: "status/none"
    readonly property string iconExpand: "expand"
    readonly property string iconCompress: "compress"
    readonly property string iconReplay: "replay"
    readonly property string iconUpload: "upload"
    readonly property string iconDownload: "download"
    readonly property string iconFile: "file"
    readonly property string iconDocument: "document"
    readonly property string iconReorder: "reorder"
    readonly property string iconTrash: "trash"
    readonly property string iconEmoji: "smile"
    readonly property string iconFeedback: "social/smile"
    readonly property string iconEnergy: "bolt"
    readonly property string iconStreakFlame: "system/flame"
    readonly property string iconFidelity: "social/sparkles"
    readonly property string iconMoodFrustrated: "social/mood-frustrated"
    readonly property string iconMoodSad: "social/mood-sad"
    readonly property string iconMoodNeutral: "social/mood-neutral"
    readonly property string iconMoodPositive: "social/smile"
    readonly property string iconMoodExcited: "social/mood-excited"

    function iconSource(name, filled) {
        const key = String(name || "")
        const direct = key.indexOf("/") >= 0 ? key : ""
        if (direct.length > 0)
            return "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/" + direct + ".svg"

        let path = ""
        switch (key) {
        case "home": path = filled ? "navigation/home-filled" : "navigation/home"; break
        case "calendar": path = "system/calendar"; break
        case "users": path = "social/users"; break
        case "chart": path = "system/chart"; break
        case "compass": path = "navigation/explore"; break
        case "apps": path = "navigation/apps"; break
        case "user": path = filled ? "navigation/profile-filled" : "navigation/profile"; break
        case "settings": path = filled ? "system/settings-filled" : "system/settings"; break
        case "back": path = rtl ? "navigation/arrow-right" : "navigation/arrow-left"; break
        case "close": path = "system/close"; break
        case "check": path = "system/check"; break
        case "compose": path = "actions/edit"; break
        case "reply": path = "actions/reply"; break
        case "message": path = "social/message"; break
        case "heart": path = filled ? "actions/heart-filled" : "actions/heart"; break
        case "repeat": path = "actions/repost"; break
        case "share": path = "actions/share"; break
        case "globe": path = "social/globe"; break
        case "palette": path = "system/palette"; break
        case "language": path = "system/language"; break
        case "sun": path = "system/sun"; break
        case "moon": path = "system/moon"; break
        case "monitor": path = "system/monitor"; break
        case "smile": path = "social/smile"; break
        case "bolt": path = "system/bolt"; break
        case "accessibility": path = "system/accessibility"; break
        case "chevron-down": path = "navigation/chevron-down"; break
        case "arrow-right": path = "navigation/arrow-right"; break
        case "arrow-left": path = "navigation/arrow-left"; break
        case "refresh": path = "actions/repost"; break
        case "wifi": path = "system/wifi"; break
        case "cloud": path = "system/cloud"; break
        case "warning": path = "system/warning"; break
        case "info": path = "system/info"; break
        case "sign-in": path = "social/user-add"; break
        case "more": path = "system/more-horizontal"; break
        case "play": path = "media/play"; break
        case "pause": path = "media/pause"; break
        case "copy": path = "actions/copy"; break
        case "image": path = "media/image"; break
        case "search": path = "navigation/search"; break
        case "external-link": path = "system/external-link"; break
        case "link": path = "system/link"; break
        case "mention": path = "social/at"; break
        case "pin": path = "system/pin"; break
        case "bookmark": path = filled ? "actions/bookmark-filled" : "actions/bookmark"; break
        case "priority-star": path = filled ? "actions/star-filled" : "actions/star"; break
        case "bell": path = filled ? "navigation/notifications-filled" : "navigation/notifications"; break
        case "store": path = "social/wallet"; break
        case "briefcase": path = "social/users"; break
        case "chevron-up": path = "navigation/chevron-up"; break
        case "send": path = "actions/send"; break
        case "plus": path = "actions/plus"; break
        case "volume": path = "media/volume"; break
        case "volume-muted": path = "media/mute"; break
        case "upload": path = "actions/upload"; break
        case "download": path = "actions/download"; break
        case "file": path = "media/attachment"; break
        case "document": path = "system/document"; break
        case "reorder": path = "system/reorder"; break
        case "trash": path = "actions/trash"; break
        case "bold": path = "system/text-bold"; break
        case "italic": path = "system/text-italic"; break
        case "code": path = "system/format-code"; break
        case "quote": path = "system/format-quote"; break
        case "list": path = "system/format-list"; break
        case "ordered-list": path = "system/format-ordered-list"; break
        case "heading": path = "system/format-heading"; break
        case "separator": path = "system/format-separator"; break
        case "preview": path = "system/eye"; break
        case "filter": path = "navigation/filter"; break
        case "sort": path = "navigation/sort"; break
        case "wallet": path = "social/wallet"; break
        default: return ""
        }
        return "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/" + path + ".svg"
    }

    function iconGlyph(name) {
        switch (String(name || "")) {
        // Font Awesome 6.7.2 codepoints. These are deliberately mapped to
        // glyphs present in the embedded faces; legacy FA4 aliases render as
        // tofu/square boxes with the current font files.
        case "home": return "\uf015"
        case "calendar": return "\uf133"
        case "users": return "\uf0c0"
        case "chart": return "\uf201"
        case "compass": return "\uf14e"
        case "apps": return "\uf00a"
        case "user": return "\uf007"
        case "settings": return "\uf013"
        case "back": return rtl ? "\uf061" : "\uf060"
        case "close": return "\uf00d"
        case "check": return "\uf00c"
        case "compose": return "\uf044"
        case "reply": return "\uf3e5"
        case "message": return "\uf27a"
        case "heart": return "\uf004"
        case "repeat": return "\uf363"
        case "share": return "\uf064"
        case "globe": return "\uf0ac"
        case "palette": return "\uf53f"
        case "language": return "\uf1ab"
        case "sun": return "\uf185"
        case "moon": return "\uf186"
        case "monitor": return "\uf390"
        case "accessibility": return "\uf29a"
        case "chevron-down": return "\uf078"
        case "arrow-right": return "\uf061"
        case "arrow-left": return "\uf060"
        case "refresh": return "\uf01e"
        case "wifi": return "\uf1eb"
        case "cloud": return "\uf0c2"
        case "warning": return "\uf071"
        case "info": return "\uf05a"
        case "spark": return "\uf005"
        case "sign-in": return "\uf2f6"
        case "more": return "\uf141"
        case "play": return "\uf04b"
        case "pause": return "\uf04c"
        case "copy": return "\uf0c5"
        case "image": return "\uf03e"
        case "search": return "\uf002"
        case "external-link": return "\uf08e"
        case "link": return "\uf0c1"
        case "mention": return "\uf1fa"
        case "pin": return "\uf3c5"
        case "bookmark": return "\uf02e"
        case "store": return "\uf54e"
        case "bell": return "\uf0f3"
        case "briefcase": return "\uf0b1"
        case "bold": return "\uf032"
        case "italic": return "\uf033"
        case "code": return "\uf121"
        case "quote": return "\uf10d"
        case "list": return "\uf03a"
        case "ordered-list": return "\uf0cb"
        case "heading": return "\uf1dc"
        case "separator": return "\uf068"
        case "chevron-up": return "\uf077"
        case "preview": return "\uf06e"
        case "send": return "\uf1d8"
        case "plus": return "\uf067"
        case "volume": return "\uf028"
        case "volume-muted": return "\uf6a9"
        case "expand": return "\uf065"
        case "compress": return "\uf066"
        case "replay": return "\uf01e"
        case "upload": return "\uf093"
        case "download": return "\uf019"
        case "file": return "\uf15b"
        case "document": return "\uf15c"
        case "reorder": return "\uf58d"
        case "trash": return "\uf1f8"
        default: return "\uf111"
        }
    }


    function iconUsesRegular(name) {
        switch (String(name || "")) {
        case "home":
        case "calendar":
        case "compass":
        case "user":
        case "globe":
        case "message":
        case "heart":
        case "sun":
        case "moon":
        case "copy":
        case "image":
        case "bookmark":
        case "bell":
        case "preview":
        case "send":
        case "info":
        case "close":
            return true
        default:
            return false
        }
    }
    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightDemiBold: Font.DemiBold
    readonly property int weightBold: Font.Bold
    readonly property int weightBlack: Font.Black

    // Qt Quick coordinates are density-independent on Android. A restrained
    // native-phone optical adjustment keeps the 360dp-wide emulator readable
    // without scaling with physical 720px pixels or overriding user choices.
    readonly property real androidTextAdjustment: Qt.platform.os === "android" ? 0.95 : 1.0
    readonly property real androidUiAdjustment: Qt.platform.os === "android" ? 0.95 : 1.0

    function scaleText(value) {
        const scaled = Math.round(value * fontScale * androidTextAdjustment)
        // 12sp remains the default readable minimum for mobile captions;
        // explicit accessibility text scaling above/below 1 still applies.
        return Math.max(Qt.platform.os === "android" && fontScale >= 1.0 ? 12 : 1, scaled)
    }

    function scaleUi(value) {
        return Math.max(1, Math.round(value * interfaceScale * androidUiAdjustment))
    }

    readonly property int typeCaptionSize: scaleText(12)
    readonly property real typeCaptionLineHeight: 1.45
    readonly property int typeMetaSize: scaleText(13)
    readonly property real typeMetaLineHeight: 1.45
    readonly property int typeLabelSize: scaleText(14)
    readonly property real typeLabelLineHeight: 1.42
    readonly property int typeBodySize: scaleText(16)
    readonly property real typeBodyLineHeight: 1.55
    readonly property int typeTitle3Size: scaleText(18)
    readonly property real typeTitle3LineHeight: 1.38
    readonly property int typeTitle2Size: scaleText(22)
    readonly property real typeTitle2LineHeight: 1.32
    readonly property int typeTitle1Size: scaleText(28)
    readonly property real typeTitle1LineHeight: 1.28
    readonly property int typeEmojiPreviewSize: scaleText(42)
    readonly property int typeDisplaySize: scaleText(34)
    readonly property real typeDisplayLineHeight: 1.20

    // Optical icon grid. Actions use 18px, persistent navigation uses 20px,
    // and changing state never changes the glyph's apparent scale.
    readonly property int iconSizeSmall: scaleUi(16)
    readonly property int iconSize: scaleUi(18)
    readonly property int iconSizeNavigation: scaleUi(20)
    readonly property int iconSizeLarge: scaleUi(24)
    readonly property int iconSizeHero: scaleUi(32)

    // Stroke scale stays pixel-stable so density changes do not blur hairlines.
    readonly property real strokeHairline: 0.75
    readonly property real strokeStandard: 1.0
    readonly property real strokeEmphasis: 1.5

    readonly property real superellipseExponent: 4.0
    readonly property int curveTiny: scaleUi(6)
    readonly property int curveCompact: scaleUi(10)
    readonly property int curveControl: scaleUi(14)
    readonly property int curveContainer: scaleUi(18)
    readonly property int curveLarge: scaleUi(22)
    readonly property int curveModal: scaleUi(28)
    readonly property int curveDrawer: scaleUi(32)
    readonly property int curveSheet: scaleUi(26)
    readonly property int curveSheetFloating: scaleUi(36)

    readonly property int radiusSmall: curveCompact
    readonly property int radiusMedium: curveControl
    readonly property int radiusLarge: curveContainer
    readonly property int radiusXLarge: curveModal

    readonly property int space1: scaleUi(4)
    readonly property int space2: scaleUi(8)
    readonly property int space3: scaleUi(12)
    readonly property int space4: scaleUi(16)
    readonly property int space5: scaleUi(20)
    readonly property int space6: scaleUi(24)
    readonly property int space7: scaleUi(32)
    readonly property int space8: scaleUi(40)
    readonly property int space9: scaleUi(48)

    readonly property int controlHeight: scaleUi(44)
    readonly property int compactControlHeight: scaleUi(38)
    readonly property int touchTarget: Math.max(44, scaleUi(46))
    readonly property int appBarHeight: scaleUi(60)
    readonly property int navigationWidth: 352
    readonly property int contextRailWidth: 416
    readonly property int readingWidth: 760
    readonly property int readingWidthWide: 1180
    readonly property int detailWidth: 980
    readonly property int detailWidthWide: 1520
    readonly property int workspaceWidth: 1536
    readonly property int contentGutter: scaleUi(20)
    readonly property int mobileGutter: scaleUi(10)
    readonly property int gutterCompact: scaleUi(12)
    readonly property int gutterStandard: scaleUi(18)
    readonly property int gutterWide: scaleUi(20)
    readonly property int storyRailHeight: scaleUi(88)

    // Optical elevation is intentionally sparse: controls, popovers and modals only.
    readonly property int elevationNone: 0
    readonly property int elevationControl: 1
    readonly property int elevationPopover: 2
    readonly property int elevationModal: 3

    function shadowBlurForElevation(level) {
        return level >= elevationModal ? 0.78 : level >= elevationPopover ? 0.62 : level >= elevationControl ? 0.40 : 0.0
    }

    function shadowBlurMaxForElevation(level) {
        return level >= elevationModal ? 32 : level >= elevationPopover ? 24 : level >= elevationControl ? 12 : 2
    }

    function shadowOpacityForElevation(level) {
        if (dark)
            return level >= elevationModal ? 0.44 : level >= elevationPopover ? 0.34 : level >= elevationControl ? 0.22 : 0.0
        return level >= elevationModal ? 0.34 : level >= elevationPopover ? 0.26 : level >= elevationControl ? 0.18 : 0.0
    }

    function shadowVerticalOffsetForElevation(level) {
        return level >= elevationModal ? 10 : level >= elevationPopover ? 5 : level >= elevationControl ? 2 : 0
    }

    function shadowScaleForElevation(level) {
        return level >= elevationModal ? 1.018 : level >= elevationPopover ? 1.012 : level >= elevationControl ? 1.004 : 1.0
    }

    // Compact canvases use nearly all available width. Space grows gradually at
    // larger compositions instead of stacking a page gutter on top of card padding.
    function gutter(width) {
        return width < 420 ? mobileGutter
             : width < breakpointMobile ? gutterCompact
             : width < breakpointNarrow ? 16
             : width < breakpointCompactDesktop ? gutterStandard
             : gutterWide
    }

    readonly property int motionFast: reduceMotion ? 0 : 90
    readonly property int motionStandard: reduceMotion ? 0 : 150
    readonly property int motionEmphasis: reduceMotion ? 0 : 210

    // Product breakpoints are semantic composition thresholds, not scale factors.
    readonly property int breakpointMobile: 600
    readonly property int breakpointNarrow: 900
    readonly property int breakpointCompactDesktop: 1200
    readonly property int breakpointLargeDesktop: 1440

    // Semantic aliases used by the redesigned social architecture.
    readonly property color backgroundPrimary: background
    readonly property color backgroundSecondary: surfaceMuted
    readonly property color surfacePrimary: surface
    readonly property color surfaceSecondary: surfaceMuted
    readonly property color surfaceInteractive: surfaceHover
    readonly property color noticeSurface: dark ? "#E61A1D23" : "#EEFFFFFF"
    readonly property color noticeBorder: dark ? "#3AFFFFFF" : "#1F111827"
    readonly property color heatmapEmpty: dark ? "#24272E" : "#E9EBEF"
    readonly property color heatmapLow: dark ? "#254735" : "#DDF3E7"
    readonly property color heatmapMedium: dark ? "#2F7650" : "#9DDEB9"
    readonly property color heatmapHigh: dark ? "#3FA96F" : "#55BF83"
    readonly property color heatmapPeak: dark ? "#61D194" : "#22845A"
    readonly property color textPrimary: ink
    readonly property color textSecondary: inkMuted
    readonly property color textTertiary: inkSubtle
    readonly property color textDisabled: inkDisabled
    readonly property color borderSubtle: borderMuted
    readonly property color borderDefault: border
    readonly property color positive: success
    readonly property color negative: danger
    readonly property color accentPrimary: accent
    readonly property color accentSecondary: dark ? accentOceanNight : accentOceanDay
    readonly property color info: accent
    readonly property color live: danger
    readonly property color story: accent
    readonly property color verified: accent

    readonly property int shapeControlSmall: curveCompact
    readonly property int shapeControlMedium: curveControl
    readonly property int shapeSurface: curveContainer
    readonly property int shapeMedia: curveSheet
    readonly property int shapeDialog: curveModal
}
