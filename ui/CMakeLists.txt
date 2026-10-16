# Keep the optional renderer out of the compiled QML graph when unavailable.
find_package(Qt6 QUIET COMPONENTS Lottie)

set(farcasterQmlFiles
    mobile/Main.qml
    demo/DemoSocialData.qml
    components/ActionButton.qml
    components/DisplayScaleControl.qml
    components/PanelWidthControl.qml
    components/AnimatedEmoji.qml
    components/SpriteEmoji.qml
    components/AppHeader.qml
    components/AppShell.qml
    components/AppsSurface.qml
    components/CompactNavigationRail.qml
    components/ContentTabs.qml
    components/ConversationRow.qml
    components/DataStoragePopup.qml
    components/CollectiblePreview.qml
    components/EmbedPreview.qml
    components/DeveloperPost.qml
    components/DrawerBackdrop.qml
    components/Avatar.qml
    components/AudioPreview.qml
    components/BottomNavButton.qml
    components/BottomNavigation.qml
    components/CodeBlockEditor.qml
    components/CodeBlockView.qml
    components/CastEmbedsPanel.qml
    components/ComposerToolbar.qml
    components/CastItem.qml
    components/CastSkeleton.qml
    components/ContextRail.qml
    components/ComposerDrawer.qml
    components/EmojiPicker.qml
    components/FeedbackDialog.qml
    components/ComposerInsertPanel.qml
    components/MediaUploadPanel.qml
    components/MediaPreview.qml
    components/FloatingComposeButton.qml
    components/FarcasterPreview.qml
    components/FeedPulseCard.qml
    components/FormattingToolbar.qml
    components/GenyDrawer.qml
    components/GenyAdaptiveDialog.qml
    components/GalleryLayout.qml
    components/GalleryTile.qml
    components/MediaGallery.qml
    components/ComingSoonSurface.qml
    components/EmbedCard.qml
    components/EngagementButton.qml
    components/EngagementBar.qml
    components/AccentColorStudio.qml
    components/AmbientMediaBackdrop.qml
    components/LocaleSettingsPopup.qml
    components/LocaleFormat.qml
    components/MiniAppPresentationPreview.qml
    components/LanguageSelector.qml
    components/SetupLanguageSelector.qml
    components/SetupActionCard.qml
    components/SetupPrimaryButton.qml
    components/SetupPageHeader.qml
    components/ModalHeader.qml
    components/LocalizedPrice.qml
    components/MarketAssetIcon.qml
    components/Icon.qml
    components/Superellipse.qml
    components/SuperellipseImage.qml
    components/GenyTabButton.qml
    components/DetailSurface.qml
    components/ThreadReplyItem.qml
    components/DirectionAwareLabel.qml
    components/DirectionAwareTextArea.qml
    components/DirectionAwareTextField.qml
    components/EmptyState.qml
    components/ExplorerFeedHeader.qml
    components/FeedHeader.qml
    components/FeedSurface.qml
    components/FilterDropdown.qml
    components/FilePreview.qml
    components/HomeComposer.qml
    components/HomeDashboard.qml
    components/HomeFeedHeader.qml
    components/InitialSetup.qml
    components/LinkPreview.qml
    components/MobileCreateMenu.qml
    components/GenyComboBox.qml
    components/GenyProgressBar.qml
    components/GenySlider.qml
    components/GenyVideoPlayer.qml
    components/GenyModal.qml
    components/GenyScrollBar.qml
    components/GenySpinner.qml
    components/GenySwitch.qml
    components/GenyRadioIndicator.qml
    components/NavItem.qml
    components/NotificationSurface.qml
    components/GenyTabStrip.qml
    components/ProfileListItem.qml
    components/ProgressRing.qml
    components/ProfileSurface.qml
    components/ProfileTrustCard.qml
    components/SearchPalette.qml
    components/SemanticFeedItem.qml
    components/SidebarProfileSummary.qml
    components/SocialSidebar.qml
    components/StoryItem.qml
    components/StoryStrip.qml
    components/PopupSurface.qml
    components/RichCastRenderer.qml
    components/RichInlineText.qml
    components/RichText.qml
    components/RichTextEditor.qml
    components/RichTextView.qml
    components/SectionHeader.qml
    components/SideNavigation.qml
    components/StatusStrip.qml
    components/StatusFooter.qml
    components/TaskManagerPopup.qml
    components/RenderingSettingsPopup.qml
    components/SystemStatusCard.qml
    components/TokenSparkline.qml
    components/TokenSemantic.qml
    components/VectorStroke.qml
    components/TokenAddressPreview.qml
    components/VideoPreview.qml
    components/VisualPost.qml
    theme/Theme.qml
)

set_source_files_properties(theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/LocaleFormat.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/TokenSemantic.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

if(Qt6Lottie_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.11)
    list(APPEND farcasterQmlFiles components/AnimatedEmojiLottie.qml)
endif()

# Verify the manifest, allowing the optional renderer to be absent from the module.
file(GLOB_RECURSE farcasterDiscoveredQmlFiles
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "*.qml"
)
if(NOT Qt6Lottie_FOUND OR Qt6_VERSION VERSION_LESS 6.11)
    list(REMOVE_ITEM farcasterDiscoveredQmlFiles components/AnimatedEmojiLottie.qml)
endif()
list(SORT farcasterDiscoveredQmlFiles)
set(farcasterRegisteredQmlFiles ${farcasterQmlFiles})
list(SORT farcasterRegisteredQmlFiles)
if(NOT farcasterDiscoveredQmlFiles STREQUAL farcasterRegisteredQmlFiles)
    message(FATAL_ERROR
        "QML manifest mismatch. Every .qml file under ui/ must be registered in "
        "farcasterQmlFiles. Discovered='${farcasterDiscoveredQmlFiles}' "
        "Registered='${farcasterRegisteredQmlFiles}'"
    )
endif()


# Keep project QML component names out of the public Qt Quick Controls namespace.
# An unqualified local type such as TabButton would otherwise resolve ambiguously
# and can silently bind to QtQuick.Controls.TabButton at runtime.
set(farcasterQtQuickControlsPublicTypes
    AbstractButton Action ActionGroup ApplicationWindow BusyIndicator Button ButtonGroup
    CheckBox CheckDelegate ComboBox Container Control DelayButton Dial Dialog DialogButtonBox
    Drawer Frame GroupBox ItemDelegate Label Menu MenuBar MenuBarItem MenuItem MenuSeparator
    Page PageIndicator Pane Popup ProgressBar RadioButton RadioDelegate RangeSlider RoundButton
    ScrollBar ScrollIndicator ScrollView Slider SpinBox SplitView StackView SwipeDelegate
    SwipeView Switch SwitchDelegate TabBar TabButton TextArea TextField ToolBar ToolButton
    ToolSeparator ToolTip Tumbler
)

foreach(qmlFile IN LISTS farcasterQmlFiles)
    if(qmlFile MATCHES "^components/(.+)\\.qml$")
        get_filename_component(qmlTypeName "${qmlFile}" NAME_WE)
        if(qmlTypeName IN_LIST farcasterQtQuickControlsPublicTypes)
            message(FATAL_ERROR
                "QML component '${qmlFile}' collides with public QtQuick.Controls type "
                "'${qmlTypeName}'. Prefix or rename the project component."
            )
        endif()
    endif()
endforeach()

foreach(qmlFile IN LISTS farcasterQmlFiles)
    if(qmlFile MATCHES "^mobile/(.+)$")
        set(qmlResourceAlias "${CMAKE_MATCH_1}")
    else()
        set(qmlResourceAlias "${qmlFile}")
    endif()

    set_source_files_properties(
        "${qmlFile}"
        PROPERTIES
            QT_RESOURCE_ALIAS "${qmlResourceAlias}"
    )
endforeach()

aimcpp_reject_final_qml_creatable_types(
    SOURCES
        "${CMAKE_SOURCE_DIR}/src/presentation/qt/presentation_bridge.hpp"
        "${CMAKE_SOURCE_DIR}/src/presentation/qt/media_upload_controller.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/render/scene_graph_stroke_item.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/render/scene_graph_superellipse_item.hpp"
)

qt_add_executable(farcaster_client
    mobile/main.cpp
    mobile/data_storage_controller.cpp
    mobile/data_storage_controller.hpp
    mobile/geo_location_resolver.cpp
    mobile/geo_location_resolver.hpp
    mobile/link_preview_resolver.cpp
    mobile/link_preview_resolver.hpp
    mobile/miniapp_host_controller.cpp
    mobile/miniapp_host_controller.hpp
    mobile/media_web_host_controller.cpp
    mobile/media_web_host_controller.hpp
    mobile/miniapp_catalog_controller.cpp
    mobile/miniapp_catalog_controller.hpp
    mobile/miniapp_task_manager_controller.cpp
    mobile/miniapp_task_manager_controller.hpp
    mobile/live_insights_controller.cpp
    mobile/live_insights_controller.hpp
    mobile/node_selection_controller.cpp
    mobile/node_selection_controller.hpp
    mobile/update_controller.cpp
    mobile/update_controller.hpp
    mobile/remote_image_provider.cpp
    mobile/remote_image_provider.hpp
)

target_sources(farcaster_client
    PRIVATE
        mobile/runtime_composition.cpp
    PRIVATE FILE_SET CXX_MODULES FILES
        mobile/runtime_composition.cppm
)
set_property(TARGET farcaster_client PROPERTY CXX_SCAN_FOR_MODULES ON)
target_compile_features(farcaster_client PRIVATE cxx_std_26)

# This optional leaf is composition-owned: core/application/presentation never
# import WebKit or the Mini App runtime. Other platforms remain explicitly
# unavailable rather than silently launching an external browser.
if(FARCASTER_ENABLE_MINIAPPS AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT CMAKE_OBJCXX_COMPILER)
        # Keep the ARC adapter on the selected C++ toolchain/standard library.
        set(CMAKE_OBJCXX_COMPILER "${CMAKE_CXX_COMPILER}")
    endif()
    enable_language(OBJCXX)
    find_library(genycasterWebKitFramework WebKit REQUIRED)
    find_library(genycasterAppKitFramework AppKit REQUIRED)
    find_library(genycasterQuartzCoreFramework QuartzCore REQUIRED)
    add_library(farcaster_miniapp_browser STATIC mobile/miniapp_browser_macos.mm)
    set_target_properties(farcaster_miniapp_browser PROPERTIES
        OBJCXX_STANDARD 26 OBJCXX_STANDARD_REQUIRED ON OBJCXX_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES OFF AUTOMOC OFF)
    target_compile_options(farcaster_miniapp_browser PRIVATE -fobjc-arc)
    target_link_libraries(farcaster_miniapp_browser PRIVATE
        Qt6::Gui Qt6::Network "${genycasterWebKitFramework}" "${genycasterAppKitFramework}"
        "${genycasterQuartzCoreFramework}")

    add_library(farcaster_miniapp_runtime STATIC)
    target_sources(farcaster_miniapp_runtime
        PUBLIC FILE_SET CXX_MODULES FILES mobile/miniapp_runtime.cppm
        PRIVATE mobile/miniapp_runtime.cpp)
    set_property(TARGET farcaster_miniapp_runtime PROPERTY CXX_SCAN_FOR_MODULES ON)
    target_compile_features(farcaster_miniapp_runtime PUBLIC cxx_std_26)
    target_link_libraries(farcaster_miniapp_runtime PUBLIC Qt6::Core farcaster::capabilities
        PRIVATE Qt6::Network farcaster_miniapp_browser)
    farcaster_enable_warnings(farcaster_miniapp_runtime)
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_MINIAPP_HOST=1)
    target_link_libraries(farcaster_client PRIVATE farcaster_miniapp_runtime)
else()
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_MINIAPP_HOST=0)
endif()

add_library(farcaster_emoji_runtime STATIC)
target_sources(farcaster_emoji_runtime
    PUBLIC FILE_SET CXX_MODULES FILES mobile/emoji_runtime.cppm
    PRIVATE mobile/emoji_runtime.cpp
)
target_link_libraries(farcaster_emoji_runtime PUBLIC farcaster::presentation
    PRIVATE farcaster::provider_emoji farcaster::transport_qt_network farcaster::execution Qt6::Gui)
set_property(TARGET farcaster_emoji_runtime PROPERTY CXX_SCAN_FOR_MODULES ON)
target_compile_features(farcaster_emoji_runtime PUBLIC cxx_std_26)
farcaster_enable_warnings(farcaster_emoji_runtime)
target_link_libraries(farcaster_client PRIVATE farcaster_emoji_runtime)

set_target_properties(farcaster_client
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../bin"
)

target_compile_definitions(farcaster_client
    PRIVATE
        FARCASTER_QT_QUICK_CONTROLS_STYLE="Basic"
        GENYCASTER_APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(farcaster_client
    PRIVATE
        farcaster::presentation
        farcaster::application
        farcaster::application_write
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Network
        Qt6::Multimedia
        Qt6::Concurrent
        farcaster::execution
        farcaster::routing
        farcaster::storage
        farcaster::transport
        farcaster::transport_qt_network
)


if(EMSCRIPTEN)
    # IDBFS must populate the virtual app directory before SQLite starts.
    # Emscripten provides the asynchronous IndexedDB file system.
    target_link_options(farcaster_client PRIVATE
        "-sFORCE_FILESYSTEM=1"
        "-lidbfs.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/persistence-pre.js"
    )
endif()

if(GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER)
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_PUBLIC_DATA_PROVIDER=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_public_data)
else()
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_PUBLIC_DATA_PROVIDER=0)
endif()

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_HYPERSNAP_PROVIDER=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_hypersnap)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_HYPERSNAP_PROVIDER=0)
endif()

if(GENYCASTER_ENABLE_OPENPROOF_AUTH)
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_OPENPROOF_AUTH=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_openproof_auth)
else()
    target_compile_definitions(farcaster_client PRIVATE GENYCASTER_HAS_OPENPROOF_AUTH=0)
endif()

if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_FARCASTER_AUTH=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_compatibility_auth)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_FARCASTER_AUTH=0)
endif()

if(FARCASTER_ENABLE_PROTOCOL_SIGNING)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_PROTOCOL_SIGNING=1)
    target_link_libraries(farcaster_client PRIVATE
        farcaster::platform_signer
        farcaster::protocol_write
        farcaster::storage_write
    )
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_PROTOCOL_SIGNING=0)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SNAPCHAIN_SIGNER=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_snapchain_signer)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SNAPCHAIN_SIGNER=0)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SNAPCHAIN_WRITE=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_snapchain_write)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SNAPCHAIN_WRITE=0)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_compatibility_browser_wallet)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER=0)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE AND FARCASTER_ENABLE_MINIAPPS)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_MINIAPP_EVM_WALLET=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_compatibility_browser_wallet)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_MINIAPP_EVM_WALLET=0)
endif()

if(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SIGNER_ATTESTATION=1)
    target_link_libraries(farcaster_client PRIVATE farcaster::provider_compatibility_signer_attestation)
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_SIGNER_ATTESTATION=0)
endif()

if(FARCASTER_ENABLE_MEDIA_UPLOAD)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_MEDIA_UPLOAD=1)
    target_link_libraries(farcaster_client PRIVATE
        farcaster::application_media
        farcaster::transport_qt_media
    )
else()
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_MEDIA_UPLOAD=0)
endif()

target_compile_features(farcaster_client PRIVATE cxx_std_26)
set_property(TARGET farcaster_client PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_client)

# Typography is an application dependency, not an operating-system assumption.
# A shipping Genycaster binary always embeds its Persian/Arabic UI family. The
# source tree may provide the pinned Vazirmatn files directly, a developer may
# point GENYCASTER_UI_FONT_DIR at a local copy, or CMake may acquire the pinned
# open-source release during configure. The runtime never depends on the font
# being installed globally on the user's machine.
set(GENYCASTER_UI_FONT_DIR "" CACHE PATH
    "Directory containing the pinned Vazirmatn TTF files for Genycaster")
option(GENYCASTER_FETCH_UI_FONT
    "Fetch the pinned open-source UI font when no local font directory is supplied" ON)
set(GENYCASTER_UI_FONT_TAG "v33.003" CACHE STRING
    "Pinned Vazirmatn release tag used for reproducible UI font acquisition")

set(_genycasterUiFontNames
    Vazirmatn-Regular.ttf
    Vazirmatn-Medium.ttf
    Vazirmatn-SemiBold.ttf
    Vazirmatn-Bold.ttf
    Vazirmatn-Black.ttf
)

set(_genycasterUiFontSourceDir "")
set(_genycasterVendoredFontDir "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")
set(_genycasterVendoredFontsComplete TRUE)
foreach(_fontName IN LISTS _genycasterUiFontNames)
    if(NOT EXISTS "${_genycasterVendoredFontDir}/${_fontName}")
        set(_genycasterVendoredFontsComplete FALSE)
        break()
    endif()
endforeach()

if(GENYCASTER_UI_FONT_DIR)
    set(_genycasterUiFontSourceDir "${GENYCASTER_UI_FONT_DIR}")
elseif(_genycasterVendoredFontsComplete)
    set(_genycasterUiFontSourceDir "${_genycasterVendoredFontDir}")
elseif(GENYCASTER_FETCH_UI_FONT)
    set(_genycasterUiFontSourceDir
        "${CMAKE_BINARY_DIR}/_deps/genycaster-ui-fonts/${GENYCASTER_UI_FONT_TAG}")
    file(MAKE_DIRECTORY "${_genycasterUiFontSourceDir}")
    foreach(_fontName IN LISTS _genycasterUiFontNames)
        set(_fontPath "${_genycasterUiFontSourceDir}/${_fontName}")
        if(NOT EXISTS "${_fontPath}")
            set(_fontUrl
                "https://raw.githubusercontent.com/rastikerdar/vazirmatn/${GENYCASTER_UI_FONT_TAG}/fonts/ttf/${_fontName}")
            file(DOWNLOAD
                "${_fontUrl}"
                "${_fontPath}"
                TLS_VERIFY ON
                TIMEOUT 30
                INACTIVITY_TIMEOUT 15
                STATUS _fontDownloadStatus
                LOG _fontDownloadLog
            )
            list(GET _fontDownloadStatus 0 _fontDownloadCode)
            if(NOT _fontDownloadCode EQUAL 0)
                file(REMOVE "${_fontPath}")
                message(FATAL_ERROR
                    "Unable to acquire ${_fontName} from pinned Vazirmatn ${GENYCASTER_UI_FONT_TAG}. "
                    "Provide -DGENYCASTER_UI_FONT_DIR=/path/to/fonts for offline builds. "
                    "Download status: ${_fontDownloadStatus}")
            endif()
        endif()
    endforeach()
else()
    message(FATAL_ERROR
        "Genycaster requires an embedded Persian/Arabic UI font. Provide "
        "-DGENYCASTER_UI_FONT_DIR=/path/to/Vazirmatn/fonts/ttf or enable "
        "GENYCASTER_FETCH_UI_FONT.")
endif()

set(_genycasterUiFontFiles)
foreach(_fontName IN LISTS _genycasterUiFontNames)
    set(_fontPath "${_genycasterUiFontSourceDir}/${_fontName}")
    if(NOT EXISTS "${_fontPath}")
        message(FATAL_ERROR
            "GENYCASTER_UI_FONT_DIR is incomplete; missing ${_fontPath}")
    endif()
    set_source_files_properties("${_fontPath}" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/${_fontName}")
    list(APPEND _genycasterUiFontFiles "${_fontPath}")
endforeach()

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-Vazirmatn.txt"
    PROPERTIES QT_RESOURCE_ALIAS "assets/fonts/OFL-Vazirmatn.txt")

qt_add_resources(farcaster_client genycaster_ui_fonts
    PREFIX "/qt/qml/Farcaster/Client"
    FILES
        ${_genycasterUiFontFiles}
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-Vazirmatn.txt"
)

# Additional script coverage is optional for developer/source builds. Production
# packages may embed pinned Noto families, but configuring Genycaster must never
# require network access. When a script font is unavailable the runtime selects a
# native platform family with shaping support (Kohinoor/PingFang/Apple SD Gothic
# on macOS, Nirmala/Yu Gothic/Malgun/YaHei on Windows, Noto families on Linux).
#
# Set GENYCASTER_REQUIRE_SCRIPT_FONTS=ON in release/CI packaging if missing
# Devanagari/CJK resources should be treated as a packaging failure.
option(GENYCASTER_FETCH_SCRIPT_FONTS
    "Best-effort fetch of pinned open-source Devanagari/CJK fonts" OFF)
option(GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS
    "Permit configure-time network access for optional script fonts" OFF)
option(GENYCASTER_REQUIRE_SCRIPT_FONTS
    "Require Devanagari/CJK fonts to be embedded (recommended for release packaging)" OFF)
set(GENYCASTER_SCRIPT_FONT_DIR "" CACHE PATH
    "Optional directory containing NotoSansDevanagari.ttf and NotoSansCJK-Regular.ttc")

set(_genycasterDevanagariFontName "NotoSansDevanagari.ttf")
set(_genycasterCjkFontName "NotoSansCJK-Regular.ttc")
set(_genycasterDevanagariFontPath "")
set(_genycasterCjkFontPath "")

if(GENYCASTER_SCRIPT_FONT_DIR)
    set(_candidateDevanagariFontPath "${GENYCASTER_SCRIPT_FONT_DIR}/${_genycasterDevanagariFontName}")
    set(_candidateCjkFontPath "${GENYCASTER_SCRIPT_FONT_DIR}/${_genycasterCjkFontName}")
    if(EXISTS "${_candidateDevanagariFontPath}")
        set(_genycasterDevanagariFontPath "${_candidateDevanagariFontPath}")
    endif()
    if(EXISTS "${_candidateCjkFontPath}")
        set(_genycasterCjkFontPath "${_candidateCjkFontPath}")
    endif()
endif()

if(NOT _genycasterDevanagariFontPath AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${_genycasterDevanagariFontName}")
    set(_genycasterDevanagariFontPath
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${_genycasterDevanagariFontName}")
endif()
if(NOT _genycasterCjkFontPath AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${_genycasterCjkFontName}")
    set(_genycasterCjkFontPath
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${_genycasterCjkFontName}")
endif()

if(GENYCASTER_FETCH_SCRIPT_FONTS AND GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS AND
   (NOT _genycasterDevanagariFontPath OR NOT _genycasterCjkFontPath))
    set(_genycasterScriptFontDir "${CMAKE_BINARY_DIR}/_deps/genycaster-script-fonts")
    file(MAKE_DIRECTORY "${_genycasterScriptFontDir}")

    if(NOT _genycasterDevanagariFontPath)
        set(_downloadDevanagariFontPath
            "${_genycasterScriptFontDir}/${_genycasterDevanagariFontName}")
        if(NOT EXISTS "${_downloadDevanagariFontPath}")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/google/fonts/main/ofl/notosansdevanagari/NotoSansDevanagari%5Bwdth%2Cwght%5D.ttf"
                "${_downloadDevanagariFontPath}"
                TLS_VERIFY ON TIMEOUT 20 INACTIVITY_TIMEOUT 10
                STATUS _devanagariFontDownloadStatus)
        else()
            set(_devanagariFontDownloadStatus 0 "cached")
        endif()
        list(GET _devanagariFontDownloadStatus 0 _devanagariFontDownloadCode)
        if(_devanagariFontDownloadCode EQUAL 0 AND EXISTS "${_downloadDevanagariFontPath}")
            set(_genycasterDevanagariFontPath "${_downloadDevanagariFontPath}")
        else()
            file(REMOVE "${_downloadDevanagariFontPath}")
            message(WARNING
                "Noto Sans Devanagari could not be fetched; continuing with the native platform fallback. "
                "Set GENYCASTER_SCRIPT_FONT_DIR to embed it. Status: ${_devanagariFontDownloadStatus}")
        endif()
    endif()

    if(NOT _genycasterCjkFontPath)
        set(_downloadCjkFontPath "${_genycasterScriptFontDir}/${_genycasterCjkFontName}")
        if(NOT EXISTS "${_downloadCjkFontPath}")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTC/NotoSansCJK-Regular.ttc"
                "${_downloadCjkFontPath}"
                TLS_VERIFY ON TIMEOUT 30 INACTIVITY_TIMEOUT 15
                STATUS _cjkFontDownloadStatus)
        else()
            set(_cjkFontDownloadStatus 0 "cached")
        endif()
        list(GET _cjkFontDownloadStatus 0 _cjkFontDownloadCode)
        if(_cjkFontDownloadCode EQUAL 0 AND EXISTS "${_downloadCjkFontPath}")
            set(_genycasterCjkFontPath "${_downloadCjkFontPath}")
        else()
            file(REMOVE "${_downloadCjkFontPath}")
            message(WARNING
                "Noto Sans CJK could not be fetched; continuing with the native platform fallback. "
                "Set GENYCASTER_SCRIPT_FONT_DIR to embed it. Status: ${_cjkFontDownloadStatus}")
        endif()
    endif()
endif()

if(GENYCASTER_REQUIRE_SCRIPT_FONTS)
    if(NOT _genycasterDevanagariFontPath OR NOT EXISTS "${_genycasterDevanagariFontPath}")
        message(FATAL_ERROR
            "Release packaging requires Noto Sans Devanagari. Set GENYCASTER_SCRIPT_FONT_DIR "
            "or enable GENYCASTER_FETCH_SCRIPT_FONTS + GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS with network access.")
    endif()
    if(NOT _genycasterCjkFontPath OR NOT EXISTS "${_genycasterCjkFontPath}")
        message(FATAL_ERROR
            "Release packaging requires Noto Sans CJK. Set GENYCASTER_SCRIPT_FONT_DIR "
            "or enable GENYCASTER_FETCH_SCRIPT_FONTS + GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS with network access.")
    endif()
endif()

set(_genycasterScriptFontFiles)
if(_genycasterDevanagariFontPath AND EXISTS "${_genycasterDevanagariFontPath}")
    set_source_files_properties("${_genycasterDevanagariFontPath}" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/${_genycasterDevanagariFontName}")
    list(APPEND _genycasterScriptFontFiles "${_genycasterDevanagariFontPath}")
endif()
if(_genycasterCjkFontPath AND EXISTS "${_genycasterCjkFontPath}")
    set_source_files_properties("${_genycasterCjkFontPath}" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/${_genycasterCjkFontName}")
    list(APPEND _genycasterScriptFontFiles "${_genycasterCjkFontPath}")
endif()
if(_genycasterScriptFontFiles)
    qt_add_resources(farcaster_client genycaster_script_fonts
        PREFIX "/qt/qml/Farcaster/Client"
        FILES ${_genycasterScriptFontFiles})
endif()

# Canonical UI iconography uses Font Awesome Free. Regular/outline glyphs are
# preferred whenever the free set exposes one; Solid is retained only for icons
# that do not have a Regular counterpart. Both fonts are embedded in the app.
set(GENYCASTER_ICON_FONT_DIR "" CACHE PATH
    "Directory containing fa-regular-400.ttf and fa-solid-900.ttf")
option(GENYCASTER_FETCH_ICON_FONT
    "Fetch the pinned Font Awesome Free icon fonts when no local copy is supplied" ON)
set(GENYCASTER_ICON_FONT_TAG "6.7.2" CACHE STRING
    "Pinned Font Awesome Free release used for reproducible icon rendering")
set(_genycasterIconFontNames
    fa-regular-400.ttf
    fa-solid-900.ttf)
set(_genycasterIconFontFiles)
foreach(_iconFontName IN LISTS _genycasterIconFontNames)
    if(GENYCASTER_ICON_FONT_DIR)
        set(_iconFontPath "${GENYCASTER_ICON_FONT_DIR}/${_iconFontName}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/${_iconFontName}")
        set(_iconFontPath "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/${_iconFontName}")
    elseif(GENYCASTER_FETCH_ICON_FONT)
        set(_genycasterIconFontDir "${CMAKE_BINARY_DIR}/_deps/genycaster-icon-font/${GENYCASTER_ICON_FONT_TAG}")
        file(MAKE_DIRECTORY "${_genycasterIconFontDir}")
        set(_iconFontPath "${_genycasterIconFontDir}/${_iconFontName}")
        if(NOT EXISTS "${_iconFontPath}")
            string(REPLACE " " "%20" _encodedIconFontName "${_iconFontName}")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/${GENYCASTER_ICON_FONT_TAG}/webfonts/${_encodedIconFontName}"
                "${_iconFontPath}"
                TLS_VERIFY ON TIMEOUT 30 INACTIVITY_TIMEOUT 15 STATUS _iconFontDownloadStatus)
            list(GET _iconFontDownloadStatus 0 _iconFontDownloadCode)
            if(NOT _iconFontDownloadCode EQUAL 0)
                file(REMOVE "${_iconFontPath}")
                message(FATAL_ERROR "Unable to acquire Font Awesome Free ${GENYCASTER_ICON_FONT_TAG}. Provide GENYCASTER_ICON_FONT_DIR for offline builds. Status: ${_iconFontDownloadStatus}")
            endif()
        endif()
    else()
        message(FATAL_ERROR "Genycaster requires its embedded icon fonts. Provide GENYCASTER_ICON_FONT_DIR or enable GENYCASTER_FETCH_ICON_FONT.")
    endif()
    if(NOT EXISTS "${_iconFontPath}")
        message(FATAL_ERROR "Missing Genycaster icon font: ${_iconFontPath}")
    endif()
    set_source_files_properties("${_iconFontPath}" PROPERTIES QT_RESOURCE_ALIAS "assets/icons/${_iconFontName}")
    list(APPEND _genycasterIconFontFiles "${_iconFontPath}")
endforeach()
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/LICENSE.Font-Awesome.txt"
    PROPERTIES QT_RESOURCE_ALIAS "assets/icons/LICENSE.Font-Awesome.txt")
qt_add_resources(farcaster_client genycaster_icon_fonts
    PREFIX "/qt/qml/Farcaster/Client"
    FILES ${_genycasterIconFontFiles} "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/LICENSE.Font-Awesome.txt")

# Approved Genyleap SVG sources are embedded unchanged. Theme/Icon resolve the
# stable semantic names at runtime; Font Awesome remains a bounded fallback for
# glyphs not yet present in the supplied system.
file(GLOB_RECURSE _genyleapIconSystemFiles
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap/*.js"
)
list(SORT _genyleapIconSystemFiles)
qt_add_resources(farcaster_client genyleap_icon_system
    PREFIX "/qt/qml/Farcaster/Client/assets/icons/genyleap"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap"
    FILES ${_genyleapIconSystemFiles}
)

# Bundled Noto Animated Emoji bootstrap set. The JSON files are Lottie assets
# licensed CC BY 4.0 (see assets/emoji/LICENSE.NotoAnimatedEmoji.txt); they are
# application data, not source code, and live under the same qrc prefix the
# emoji controller resolves bundled assets from.
file(GLOB _notoEmojiLottieFiles
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/lottie/*.json"
)
list(SORT _notoEmojiLottieFiles)
qt_add_resources(farcaster_client noto_emoji_lottie
    PREFIX "/qt/qml/Farcaster/Client/assets/emoji/lottie"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/lottie"
    FILES ${_notoEmojiLottieFiles}
)
qt_add_resources(farcaster_client noto_emoji_metadata
    PREFIX "/qt/qml/Farcaster/Client/assets/emoji"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/bootstrap_manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/LICENSE.NotoAnimatedEmoji.txt"
)

qt_add_qml_module(farcaster_client
    URI Farcaster.Client
    VERSION 1.0
    QML_FILES
        ${farcasterQmlFiles}
    SOURCES
        render/scene_graph_stroke_item.cpp
        render/scene_graph_stroke_item.hpp
        render/scene_graph_superellipse_item.cpp
        render/scene_graph_superellipse_item.hpp
    RESOURCES
)

target_include_directories(farcaster_client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/render"
)

# VectorImage track-matte composition needs Qt 6.11+ and the Qt Lottie vector-format plugin.
# Older/renderer-free SDKs retain the Unicode fallback without loading this type.
find_package(Qt6 QUIET COMPONENTS Lottie)
if(Qt6Lottie_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.11)
    target_compile_definitions(farcaster_client PRIVATE FARCASTER_HAS_LOTTIE=1)
    qt_import_plugins(farcaster_client INCLUDE Qt6::QLottieVectorImagePlugin)
endif()

set(farcasterTranslationFiles
    i18n/qml_en_US.ts
    i18n/qml_fa_IR.ts
    i18n/qml_de_DE.ts
    i18n/qml_ja_JP.ts
    i18n/qml_ko_KR.ts
    i18n/qml_zh_CN.ts
    i18n/qml_hi_IN.ts
    i18n/qml_ar_SA.ts
    i18n/qml_es_ES.ts
    i18n/qml_fr_FR.ts
    i18n/qml_pt_BR.ts
    i18n/qml_ru_RU.ts
)

set(farcasterTranslationSources
    ${farcasterQmlFiles}
    mobile/main.cpp
    mobile/data_storage_controller.cpp
    mobile/data_storage_controller.hpp
    mobile/miniapp_host_controller.cpp
    mobile/miniapp_catalog_controller.cpp
    mobile/miniapp_task_manager_controller.cpp
    mobile/miniapp_runtime.cpp
    mobile/miniapp_browser_macos.mm
    "${CMAKE_SOURCE_DIR}/src/presentation/qt/presentation_bridge.cpp"
    "${CMAKE_SOURCE_DIR}/src/presentation/qt/localization_controller.cpp"
    i18n/catalog.cpp
)

# Keep source-catalog maintenance separate from release catalog generation.
# update_translations is explicit developer tooling; normal application builds
# never rewrite the reviewed TS files.
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
    qt_add_lupdate(
        TS_FILES
            ${farcasterTranslationFiles}
        SOURCES
            ${farcasterTranslationSources}
        LUPDATE_TARGET farcaster_client_lupdate
    )
else()
    qt_add_lupdate(farcaster_client
        TS_FILES
            ${farcasterTranslationFiles}
        SOURCES
            ${farcasterTranslationSources}
    )
endif()

# Generate the runtime catalogs explicitly. qsTrId()/qtTrId() require an
# ID-keyed QM catalog. Qt 6.11 infers ID-based catalogs from the TS contents, so
# invoking Qt's imported lrelease executable directly keeps the command line
# deterministic on macOS, Windows, and Linux without the deprecated -idbased
# option.
set(farcasterTranslationQmFiles)
foreach(tsFile IN LISTS farcasterTranslationFiles)
    get_filename_component(tsStem "${tsFile}" NAME_WE)
    set(qmFile "${CMAKE_CURRENT_BINARY_DIR}/${tsStem}.qm")

    add_custom_command(
        OUTPUT "${qmFile}"
        COMMAND Qt6::lrelease
            -fail-on-invalid
            "${CMAKE_CURRENT_SOURCE_DIR}/${tsFile}"
            -qm "${qmFile}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${tsFile}"
            Qt6::lrelease
        COMMENT "Generating ID-based translation catalog ${tsStem}.qm"
        VERBATIM
    )

    list(APPEND farcasterTranslationQmFiles "${qmFile}")
endforeach()

set_source_files_properties(
    ${farcasterTranslationQmFiles}
    PROPERTIES GENERATED TRUE
)

add_custom_target(farcaster_translation_catalogs
    DEPENDS ${farcasterTranslationQmFiles}
)
add_dependencies(farcaster_client farcaster_translation_catalogs)

# Embed exactly the QM outputs produced above. The runtime contract is stable:
#   :/i18n/qml_en_US.qm
#   :/i18n/qml_fa_IR.qm
qt_add_resources(farcaster_client farcaster_translation_resources
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES
        ${farcasterTranslationQmFiles}
)

set(farcasterQmlLintOptions --bare)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    list(APPEND farcasterQmlLintOptions --max-warnings 0)
endif()

add_custom_target(farcaster_client_qmllint_strict
    COMMAND Qt6::qmllint
        ${farcasterQmlLintOptions}
        -I "${QT_QML_OUTPUT_DIRECTORY}"
        -I "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
        -M Farcaster.Client
    DEPENDS farcaster_client
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)

if(FARCASTER_BUILD_TESTS AND BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(farcasterHeadlessPlatform "offscreen")
    if(APPLE)
        # Qt 6.11's macOS offscreen plugin emits a fatal generic-font alias
        # warning before project QML loads; the minimal plugin is warning-clean.
        set(farcasterHeadlessPlatform "minimal")
    endif()

    add_test(NAME ui.smoke COMMAND farcaster_client --smoke-test)
    set_tests_properties(ui.smoke PROPERTIES
        LABELS "ui;smoke"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.localization_en_smoke
        COMMAND farcaster_client --smoke-test --language=en
    )
    set_tests_properties(ui.localization_en_smoke PROPERTIES
        LABELS "ui;smoke;localization;ltr"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.localization_fa_smoke
        COMMAND farcaster_client
            --smoke-test
            --language=fa
            --regional-locale=fa_IR
            --calendar=jalali
            --timestamp-display=calendar
            --time-zone=Asia/Tehran
            --currency=USD
            --additional-currencies=IRT,EUR
    )
    set_tests_properties(ui.localization_fa_smoke PROPERTIES
        LABELS "ui;smoke;localization;rtl"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.appearance_dark_smoke
        COMMAND farcaster_client --smoke-test --theme-mode=dark --color-theme=geny
    )
    set_tests_properties(ui.appearance_dark_smoke PROPERTIES
        LABELS "ui;smoke;appearance;dark"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.appearance_light_smoke
        COMMAND farcaster_client --smoke-test --theme-mode=light --color-theme=ocean --reduce-motion=true
    )
    set_tests_properties(ui.appearance_light_smoke PROPERTIES
        LABELS "ui;smoke;appearance;light"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
        add_test(
            NAME ui.runtime_smoke
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/ui/runtime_smoke.py"
                "$<TARGET_FILE:farcaster_client>"
        )
        set_tests_properties(ui.runtime_smoke PROPERTIES
            LABELS "ui;smoke;integration"
            ENVIRONMENT
                "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
            TIMEOUT 30
        )
    endif()
endif()

if(FARCASTER_BUILD_TESTS AND BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(farcaster_update_controller_tests
        ../tests/ui/update_controller_tests.cpp
        mobile/update_controller.cpp
        mobile/update_controller.hpp)
    target_include_directories(farcaster_update_controller_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile")
    target_link_libraries(farcaster_update_controller_tests PRIVATE
        Qt6::Core Qt6::Network Qt6::Gui)
    add_test(NAME ui.update_controller COMMAND farcaster_update_controller_tests)
    set_tests_properties(ui.update_controller PROPERTIES TIMEOUT 8 LABELS "ui;update;security")

    add_executable(farcaster_media_scroll_tests
        ../tests/ui/media_scroll_tests.cpp
        render/scene_graph_superellipse_item.cpp)
    target_include_directories(farcaster_media_scroll_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/render")
    target_link_libraries(farcaster_media_scroll_tests PRIVATE
        farcaster::presentation Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia)
    target_compile_definitions(farcaster_media_scroll_tests PRIVATE
        MEDIA_COMPONENT_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/components"
        MEDIA_THEME_FILE="${CMAKE_CURRENT_SOURCE_DIR}/theme/Theme.qml")
    add_dependencies(farcaster_media_scroll_tests farcaster_translation_catalogs)
    qt_add_resources(farcaster_media_scroll_tests media_scroll_translations
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${farcasterTranslationQmFiles})
    qt_add_resources(farcaster_media_scroll_tests media_scroll_icons
        PREFIX "/qt/qml/Farcaster/Client/assets/icons/genyleap"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap"
        FILES ${_genyleapIconSystemFiles})
    add_test(NAME ui.media_scroll COMMAND farcaster_media_scroll_tests)
    set_tests_properties(ui.media_scroll PROPERTIES TIMEOUT 45
        LABELS "ui;media;scroll"
        ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
    if(APPLE)
        add_test(NAME ui.media_scroll_metal COMMAND farcaster_media_scroll_tests)
        set_tests_properties(ui.media_scroll_metal PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE
            LABELS "ui;media;scroll;metal"
            ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QSG_RHI_BACKEND=metal;QT_QUICK_BACKEND=;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
    endif()
    add_executable(farcaster_link_preview_shutdown_tests
        ../tests/ui/link_preview_shutdown_tests.cpp
        mobile/link_preview_resolver.cpp)
    target_include_directories(farcaster_link_preview_shutdown_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile")
    target_link_libraries(farcaster_link_preview_shutdown_tests PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME ui.link_preview_shutdown COMMAND farcaster_link_preview_shutdown_tests)
    set_tests_properties(ui.link_preview_shutdown PROPERTIES TIMEOUT 8
        LABELS "ui;network;shutdown")

    add_executable(farcaster_emoji_ui_tests ../tests/ui/emoji_runtime_tests.cpp)
    target_link_libraries(farcaster_emoji_ui_tests PRIVATE farcaster_emoji_runtime farcaster::provider_emoji Qt6::Test Qt6::Quick)
    target_compile_definitions(farcaster_emoji_ui_tests PRIVATE
        EMOJI_COMPONENT_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/components"
        EMOJI_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/lottie")
    qt_add_resources(farcaster_emoji_ui_tests emoji_test_assets
        PREFIX "/qt/qml/Farcaster/Client/assets/emoji/lottie"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/lottie"
        FILES ${_notoEmojiLottieFiles})
    add_test(NAME ui.emoji_runtime COMMAND farcaster_emoji_ui_tests)
    set_tests_properties(ui.emoji_runtime PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
    if(APPLE)
        add_test(NAME ui.emoji_runtime_metal COMMAND farcaster_emoji_ui_tests)
        set_tests_properties(ui.emoji_runtime_metal PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE
            ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QSG_RHI_BACKEND=metal;QT_QUICK_BACKEND=;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
    endif()
endif()
