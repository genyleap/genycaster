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
    components/MiniAppFeaturedCarousel.qml
    components/MiniAppLibraryShelves.qml
    components/MiniAppArtPlaceholder.qml
    components/ContentRefreshIndicator.qml
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
    components/CastActionsMenu.qml
    components/SourceClientBadge.qml
    components/CastSkeleton.qml
    components/CastViews.qml
    components/CastReactions.qml
    components/BookmarksSurface.qml
    components/ContentFilterSheet.qml
    components/BookmarkCollectionChooser.qml
    components/ContextRail.qml
    components/ComposerDrawer.qml
    components/DashedCapsuleOutline.qml
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
    components/GenyProDetailsDrawer.qml
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
    components/ProviderProfilesPanel.qml
    components/NodeManagementPanel.qml
    components/UpdateNoticeDrawer.qml
    components/LocaleFormat.qml
    components/MiniAppPresentationPreview.qml
    components/LanguageSelector.qml
    components/SetupLanguageSelector.qml
    components/SetupStepIndicator.qml
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
    components/GenyCheckRow.qml
    components/DetailSurface.qml
    components/ThreadReplyItem.qml
    components/GenyThreadRail.qml
    components/DirectionAwareLabel.qml
    components/DirectionAwareTextArea.qml
    components/EditorContextMenu.qml
    components/EditorMenuItem.qml
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
    components/GenyMultiSelect.qml
    components/GenyProgressBar.qml
    components/GenySlider.qml
    components/GenyVideoPlayer.qml
    components/GenyModal.qml
    components/GenyScrollBar.qml
    components/ScrollToTopButton.qml
    components/PullToRefresh.qml
    components/GenySpinner.qml
    components/GenySwitch.qml
    components/GenyRadioIndicator.qml
    components/NavItem.qml
    components/NotificationSurface.qml
    components/GenyTabStrip.qml
    components/ProfileListItem.qml
    components/ProgressRing.qml
    components/ProfileLinkRow.qml
    components/ProfileSurface.qml
    components/ProfileProBadge.qml
    components/ProUpgradeNotice.qml
    components/GenyPulsePanel.qml
    components/StreakCard.qml
    components/AccountConnectionsPopup.qml
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
    components/WalletBrowserPicker.qml
    components/RenderingSettingsPopup.qml
    components/SystemStatusCard.qml
    components/TokenSparkline.qml
    components/TokenSemantic.qml
    components/VectorStroke.qml
    components/TokenAddressPreview.qml
    components/TopicBrowseSurface.qml
    components/TopicFilterPicker.qml
    components/VideoPreview.qml
    components/VisualPost.qml
    theme/Theme.qml
)

set_source_files_properties(theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/LocaleFormat.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/CastViews.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/CastReactions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(components/TokenSemantic.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

if(Qt6Lottie_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.11)
    list(APPEND farcasterQmlFiles components/AnimatedEmojiLottie.qml)
endif()

# Android uses the platform WebView for public Mini App web content. No
# privileged SDK / signer bridge is exposed to that untrusted web surface.
if(ANDROID)
    list(APPEND farcasterQmlFiles components/AndroidMiniAppWebView.qml)
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
if(NOT ANDROID)
    list(REMOVE_ITEM farcasterDiscoveredQmlFiles components/AndroidMiniAppWebView.qml)
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

qt_add_executable(genycaster
    mobile/main.cpp
    mobile/runtime_ui_bindings.cpp
    mobile/data_storage_controller.cpp
    mobile/data_storage_controller.hpp
    mobile/geo_location_resolver.cpp
    mobile/geo_location_resolver.hpp
    mobile/link_preview_resolver.cpp
    mobile/link_preview_resolver.hpp
    mobile/cast_view_controller.cpp
    mobile/cast_view_controller.hpp
    mobile/browser_reaction_controller.cpp
    mobile/browser_reaction_controller.hpp
    mobile/browser_link_metadata_controller.cpp
    mobile/browser_link_metadata_controller.hpp
    mobile/browser_token_preview_controller.cpp
    mobile/browser_token_preview_controller.hpp
    mobile/miniapp_host_controller.cpp
    mobile/miniapp_host_controller.hpp
    mobile/media_web_host_controller.cpp
    mobile/media_web_host_controller.hpp
    mobile/miniapp_catalog_controller.cpp
    mobile/miniapp_catalog_controller.hpp
    mobile/channel_picker_controller.cpp
    mobile/channel_picker_controller.hpp
    mobile/text_selection_settings.cpp
    mobile/text_selection_settings.hpp
    mobile/miniapp_task_manager_controller.cpp
    mobile/miniapp_task_manager_controller.hpp
    mobile/live_insights_controller.cpp
    mobile/live_insights_controller.hpp
    ../src/providers/hypersnap/node_selection_controller.cpp
    mobile/node_selection_controller.hpp
    mobile/provider_profiles_controller.cpp
    mobile/provider_profiles_controller.hpp
    mobile/update_controller.cpp
    mobile/update_controller.hpp
    mobile/remote_image_provider.cpp
    mobile/remote_image_provider.hpp
    mobile/browser_openproof_auth.cpp
    mobile/browser_openproof_auth.hpp
    mobile/openproof_connections_controller.cpp
    mobile/openproof_connections_controller.hpp
    mobile/genypulse_activity_controller.cpp
    mobile/genypulse_activity_controller.hpp
)

target_sources(genycaster
    PRIVATE
        mobile/runtime_composition.cpp
    PRIVATE FILE_SET CXX_MODULES FILES
        mobile/runtime_composition.cppm
)
set_property(TARGET genycaster PROPERTY CXX_SCAN_FOR_MODULES ON)
# macdeployqt consumes a real .app bundle; an ordinary Mach-O executable
# cannot be deployed into a DMG with the Qt plugin and QML payload.
if(APPLE)
    # Keep a real Apple app bundle on both macOS and iOS. Qt generates
    # the correct platform Info.plist for qt_add_executable.
    set_target_properties(genycaster PROPERTIES MACOSX_BUNDLE TRUE)
    if(IOS)
        set_target_properties(genycaster PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.genyleap.genycaster"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    endif()
endif()
# All native packages use a single GenyCaster mark. Generate the binary
# assets with cmake/generate_app_icons.py; ship them so cross-builds do not
# need image conversion tools.
set(_gcIcon "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-512.png")
set_source_files_properties("${_gcIcon}" PROPERTIES QT_RESOURCE_ALIAS "app-icon.png")
qt_add_resources(genycaster genycaster_app_icon PREFIX "/genycaster" FILES "${_gcIcon}")

if(WIN32)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc"
        "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/genycaster.ico\"\n")
    target_sources(genycaster PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc")
    set_target_properties(genycaster PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    if(IOS)
        set(_gcCatalog "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Assets.xcassets")
        target_sources(genycaster PRIVATE "${_gcCatalog}")
        set_source_files_properties("${_gcCatalog}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(genycaster PROPERTIES
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
    else()
        set(_gcMacIcon "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/genycaster.icns")
        set_source_files_properties("${_gcMacIcon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(genycaster PRIVATE "${_gcMacIcon}")
        set_target_properties(genycaster PROPERTIES MACOSX_BUNDLE_ICON_FILE "genycaster.icns")
    endif()
elseif(ANDROID)
    set_target_properties(genycaster PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android")
elseif(UNIX AND NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/genycaster.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/genycaster.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
target_compile_features(genycaster PRIVATE cxx_std_26)
if(TARGET farcaster::platform_custody_recovery)
    target_link_libraries(genycaster PRIVATE farcaster::platform_custody_recovery)
endif()

# GCC 16 can exhaust memory on small Linux builders while emitting DWARF
# for the C++26 runtime module and the unusually large Qt composition root.
# Keep Debug symbols in libraries and UI adapters; omitting DWARF only for
# these three outer-composition translation units changes no code or ABI.
# This is a build-memory workaround, not a release optimization.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT EMSCRIPTEN)
    set_source_files_properties(
        mobile/main.cpp
        mobile/runtime_ui_bindings.cpp
        mobile/runtime_composition.cpp
        mobile/runtime_composition.cppm
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:-g0>"
    )
endif()

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
    # SIWF identity verification is a separate chain-backed authorization
    # operation. It does not select/change the Farcaster write/read provider.
    if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
        target_compile_definitions(farcaster_miniapp_runtime PRIVATE GENYCASTER_MINIAPP_SIWF=1)
        target_link_libraries(farcaster_miniapp_runtime PRIVATE
            farcaster::provider_compatibility_auth
            farcaster::transport_qt_network)
    else()
        target_compile_definitions(farcaster_miniapp_runtime PRIVATE GENYCASTER_MINIAPP_SIWF=0)
    endif()
    farcaster_enable_warnings(farcaster_miniapp_runtime)
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_MINIAPP_HOST=1)
    target_link_libraries(genycaster PRIVATE farcaster_miniapp_runtime)
else()
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_MINIAPP_HOST=0)
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
target_link_libraries(genycaster PRIVATE farcaster_emoji_runtime)

set_target_properties(genycaster
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../bin"
)

# Keep version identity owned by CMake and identical for the application and
# every test that compiles a production source file (notably update_controller.cpp).
# PRIVATE executable definitions do not propagate to standalone test targets.
add_library(genycaster_release_identity INTERFACE)
target_compile_definitions(genycaster_release_identity
    INTERFACE
        GENYCASTER_APP_VERSION="${GENYCASTER_RELEASE_VERSION}"
        GENYCASTER_PRODUCT_NAME="${PROJECT_NAME}"
        GENYCASTER_VERSION_NUMBER="${PROJECT_VERSION}"
        GENYCASTER_RELEASE_CHANNEL="${GENYCASTER_RELEASE_CHANNEL}"
        GENYCASTER_RELEASE_CHANNEL_NUMBER="${GENYCASTER_RELEASE_CHANNEL_NUMBER}"
)
target_link_libraries(genycaster PRIVATE genycaster_release_identity)

target_compile_definitions(genycaster
    PRIVATE
        FARCASTER_QT_QUICK_CONTROLS_STYLE="Basic"
        GENYCASTER_CAST_VIEWS_ENABLED=$<BOOL:${FARCASTER_ENABLE_DERIVED_CAST_VIEWS}>
)

if(ANDROID)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Svg)
    target_link_libraries(genycaster PRIVATE Qt6::Svg)
endif()

if(ANDROID AND TARGET Qt6Keychain::Qt6Keychain)
    # Package the shared QtKeychain backend used by the static signer vault.
    set_property(TARGET genycaster APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS "$<TARGET_FILE:Qt6Keychain::Qt6Keychain>")
endif()

# Qt's TLS backend loads both shared OpenSSL libraries dynamically on Android.
# Include them in the APK, even though Genycaster links only OpenSSL::Crypto.
if(ANDROID AND TARGET OpenSSL::Crypto)
    set_property(TARGET genycaster APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${GENYCASTER_ANDROID_OPENSSL_CRYPTO_LIBRARY}"
        "${GENYCASTER_ANDROID_OPENSSL_SSL_LIBRARY}")
endif()

target_link_libraries(genycaster
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
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    # Qt defaults to a 50 MB initial heap, but the static Qt Quick/QML font
    # resources in this WASM target already require >54 MiB at link time.
    # Configure Qt's target property rather than appending a conflicting -s
    # option: qt_finalize_target adds INITIAL_MEMORY after link options.
    # 128 MB is page-aligned (64 KiB pages); ALLOW_MEMORY_GROWTH remains enabled.
    set_target_properties(genycaster PROPERTIES
        QT_WASM_INITIAL_MEMORY "128MB")

    # IDBFS must populate the virtual app directory before SQLite starts.
    # Emscripten provides the asynchronous IndexedDB file system.
    target_link_options(genycaster PRIVATE
        "-sFORCE_FILESYSTEM=1"
        "-lidbfs.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/persistence-pre.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/miniapp-host-pre.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/browser-hls-pre.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/portrait-orientation-pre.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/protocol-signer-pre.js"
    )

    # The WASM Mini App host must use Farcaster's official origin-filtered
    # Comlink implementation. Bundle it into a first-party asset so browser
    # runtime startup never depends on a third-party CDN.
    find_program(GENYCASTER_NODE_EXECUTABLE node REQUIRED)
    find_program(GENYCASTER_NPM_EXECUTABLE npm REQUIRED)
    set(GENYCASTER_MINIAPP_SDK_STAGE "${CMAKE_CURRENT_BINARY_DIR}/miniapp-host-sdk")
    add_custom_command(TARGET genycaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${GENYCASTER_MINIAPP_SDK_STAGE}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/host-sdk/package.json"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/package.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/host-sdk/package-lock.json"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/package-lock.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/host-sdk/build.mjs"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/build.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/miniapp-host-bridge-entry.mjs"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/entry.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/miniapp-wallet-provider.mjs"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/miniapp-wallet-provider.mjs"
        COMMAND ${GENYCASTER_NPM_EXECUTABLE} ci
            --prefix "${GENYCASTER_MINIAPP_SDK_STAGE}"
            --ignore-scripts --no-audit --no-fund
        COMMAND ${GENYCASTER_NODE_EXECUTABLE}
            "${GENYCASTER_MINIAPP_SDK_STAGE}/build.mjs"
            "${GENYCASTER_MINIAPP_SDK_STAGE}/entry.mjs"
            "$<TARGET_FILE_DIR:genycaster>/miniapp-host-bridge.mjs"
        COMMENT "Bundling pinned Farcaster Mini App host SDK for first-party WASM delivery"
        VERBATIM
    )

    # Package the origin-bound WebCrypto key vault alongside the generated
    # Qt loader. It remains deliberately separate from signer authorization:
    # publishing must not become ready until custody approval and a verified
    # scoped Snapchain registration complete.
    add_custom_command(TARGET genycaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/secure_signer_vault.mjs"
            "$<TARGET_FILE_DIR:genycaster>/secure_signer_vault.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/farcaster_protocol.mjs"
            "$<TARGET_FILE_DIR:genycaster>/farcaster_protocol.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/browser_protocol_signer.mjs"
            "$<TARGET_FILE_DIR:genycaster>/browser_protocol_signer.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/browser_device_session.mjs"
            "$<TARGET_FILE_DIR:genycaster>/browser_device_session.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/browser_account_session.mjs"
            "$<TARGET_FILE_DIR:genycaster>/browser_account_session.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/browser_media_upload.mjs"
            "$<TARGET_FILE_DIR:genycaster>/browser_media_upload.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/farcaster_recovery.mjs"
            "$<TARGET_FILE_DIR:genycaster>/farcaster_recovery.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/openproof_recovery.mjs"
            "$<TARGET_FILE_DIR:genycaster>/openproof_recovery.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/recovery-phrase-dialog.mjs"
            "$<TARGET_FILE_DIR:genycaster>/recovery-phrase-dialog.mjs"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:genycaster>/vendor"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/vendor/noble-secp256k1.mjs"
            "$<TARGET_FILE_DIR:genycaster>/vendor/noble-secp256k1.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/vendor/bip39-english.mjs"
            "$<TARGET_FILE_DIR:genycaster>/vendor/bip39-english.mjs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/vendor/noble-secp256k1.LICENSE"
            "$<TARGET_FILE_DIR:genycaster>/vendor/noble-secp256k1.LICENSE"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/vendor/scure-bip39.LICENSE"
            "$<TARGET_FILE_DIR:genycaster>/vendor/scure-bip39.LICENSE"
        COMMENT "Packaging browser Farcaster recovery, signer, canonical codec and pinned cryptography"
        VERBATIM
    )

    # Replace Qt's generic logo splash with the first-party branded boot
    # container after the target is linked. Keep the standard Qt loader API.
    add_custom_command(TARGET genycaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-shell.html"
            "$<TARGET_FILE_DIR:genycaster>/genycaster.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-loader.js"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-loader.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-sw.js"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-sw.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster.webmanifest"
            "$<TARGET_FILE_DIR:genycaster>/genycaster.webmanifest"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-icon.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-icon.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/favicon.ico"
            "$<TARGET_FILE_DIR:genycaster>/favicon.ico"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/favicon-16x16.png"
            "$<TARGET_FILE_DIR:genycaster>/favicon-16x16.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/favicon-32x32.png"
            "$<TARGET_FILE_DIR:genycaster>/favicon-32x32.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/favicon-48x48.png"
            "$<TARGET_FILE_DIR:genycaster>/favicon-48x48.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-192.png"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-192.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-512.png"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-512.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/genyleap/icons/actions/repost.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-update-icon.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/apple-touch-icon.png"
            "$<TARGET_FILE_DIR:genycaster>/apple-touch-icon.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/apple-touch-icon-precomposed.png"
            "$<TARGET_FILE_DIR:genycaster>/apple-touch-icon-precomposed.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-icon.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-brand.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-wordmark.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-wordmark.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-wordmark-dark.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-wordmark-dark.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/genycaster-boot-icon.svg"
            "$<TARGET_FILE_DIR:genycaster>/genycaster-boot-icon.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/intro-base.svg"
            "$<TARGET_FILE_DIR:genycaster>/intro-base.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/intro-farcaster.svg"
            "$<TARGET_FILE_DIR:genycaster>/intro-farcaster.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/intro-hypersnap.svg"
            "$<TARGET_FILE_DIR:genycaster>/intro-hypersnap.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/wasm/intro-openproof.svg"
            "$<TARGET_FILE_DIR:genycaster>/intro-openproof.svg"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/ci/package_wasm_assets.py"
            --dir "$<TARGET_FILE_DIR:genycaster>"
            --app-version "${GENYCASTER_RELEASE_VERSION}"
            --product-name "${PROJECT_NAME}"
            --version-number "${PROJECT_VERSION}"
            --release-channel "${GENYCASTER_RELEASE_CHANNEL}"
            --release-channel-number "${GENYCASTER_RELEASE_CHANNEL_NUMBER}"
            --release-notes "${GENYCASTER_RELEASE_NOTES_FILE}"
        COMMENT "Installing and hashing Genycaster WebAssembly public assets"
        VERBATIM
    )

    # JS/HTML/worker-only edits should be reflected in the `all` build
    # without forcing an expensive C++/WASM relink. Keep the static staging
    # output and content hash metadata coherent on incremental builds.
    add_custom_target(genycaster_wasm_asset_refresh ALL
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/ci/package_wasm_assets.py"
            --source "${CMAKE_CURRENT_SOURCE_DIR}/wasm"
            --dir "$<TARGET_FILE_DIR:genycaster>"
            --app-version "${GENYCASTER_RELEASE_VERSION}"
            --product-name "${PROJECT_NAME}"
            --version-number "${PROJECT_VERSION}"
            --release-channel "${GENYCASTER_RELEASE_CHANNEL}"
            --release-channel-number "${GENYCASTER_RELEASE_CHANNEL_NUMBER}"
            --release-notes "${GENYCASTER_RELEASE_NOTES_FILE}"
        DEPENDS genycaster
        COMMENT "Refreshing versioned Genycaster WebAssembly static assets"
        VERBATIM
    )
endif()

if(GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER)
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_PUBLIC_DATA_PROVIDER=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_public_data)
else()
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_PUBLIC_DATA_PROVIDER=0)
endif()

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_HYPERSNAP_PROVIDER=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_hypersnap)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_HYPERSNAP_PROVIDER=0)
endif()

if(GENYCASTER_ENABLE_OPENPROOF_AUTH)
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_OPENPROOF_AUTH=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_openproof_auth)
else()
    target_compile_definitions(genycaster PRIVATE GENYCASTER_HAS_OPENPROOF_AUTH=0)
endif()

if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_FARCASTER_AUTH=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_compatibility_auth)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_FARCASTER_AUTH=0)
endif()

if(FARCASTER_ENABLE_PROTOCOL_SIGNING)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_PROTOCOL_SIGNING=1)
    target_link_libraries(genycaster PRIVATE
        farcaster::platform_signer
        farcaster::protocol_write
        farcaster::storage_write
    )
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_PROTOCOL_SIGNING=0)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_READ=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_snapchain_read)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_READ=0)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_SIGNER=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_snapchain_signer)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_SIGNER=0)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_WRITE=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_snapchain_write)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SNAPCHAIN_WRITE=0)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_compatibility_browser_wallet)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER=0)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE AND FARCASTER_ENABLE_MINIAPPS)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_MINIAPP_EVM_WALLET=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_compatibility_browser_wallet)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_MINIAPP_EVM_WALLET=0)
endif()

if(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SIGNER_ATTESTATION=1)
    target_link_libraries(genycaster PRIVATE farcaster::provider_compatibility_signer_attestation)
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_SIGNER_ATTESTATION=0)
endif()

if(FARCASTER_ENABLE_MEDIA_UPLOAD)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_MEDIA_UPLOAD=1)
    target_link_libraries(genycaster PRIVATE
        farcaster::application_media
        farcaster::transport_qt_media
    )
else()
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_MEDIA_UPLOAD=0)
endif()

target_compile_features(genycaster PRIVATE cxx_std_26)
set_property(TARGET genycaster PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(genycaster)

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

qt_add_resources(genycaster genycaster_ui_fonts
    PREFIX "/qt/qml/Farcaster/Client"
    FILES
        ${_genycasterUiFontFiles}
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-Vazirmatn.txt"
)

# Qt WebAssembly cannot access browser/OS emoji fonts. Bundle pinned
# CBDT/CBLC Noto Color Emoji for composed emoji, skin tones and flags.
# Preserve the small monochrome subset as a secondary fallback; both are
# local OFL resources (never download or resolve fonts at runtime).
# BIG_RESOURCES avoids enormous C++ font data initializers.
set(_genycasterLargeResources)
if(EMSCRIPTEN)
    set(_genycasterLargeResources BIG_RESOURCES)
    set(_genycasterEmojiFallback
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/GenycasterEmojiFallback.ttf")
    if(NOT EXISTS "${_genycasterEmojiFallback}")
        message(FATAL_ERROR "Missing lightweight WebAssembly emoji fallback font.")
    endif()
    file(SIZE "${_genycasterEmojiFallback}" _emojiFallbackSize)
    if(_emojiFallbackSize GREATER 1048576)
        message(FATAL_ERROR "WebAssembly emoji fallback exceeds its 1 MiB budget.")
    endif()
    file(SHA256 "${_genycasterEmojiFallback}" _emojiFallbackHash)
    if(NOT _emojiFallbackHash STREQUAL
            "0e9754e6501fd98dbc33d42fc73d4fe1ac1d49ce88654d826b60d0ab5c945530")
        message(FATAL_ERROR "WebAssembly emoji fallback integrity mismatch.")
    endif()
    set_source_files_properties("${_genycasterEmojiFallback}" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/GenycasterEmojiFallback.ttf")
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-NotoEmoji.txt" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/OFL-NotoEmoji.txt")
    set(_genycasterColorEmoji
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/GenycasterColorEmoji.ttf")
    if(NOT EXISTS "${_genycasterColorEmoji}")
        message(FATAL_ERROR "Missing vendored WebAssembly color emoji font.")
    endif()
    file(SIZE "${_genycasterColorEmoji}" _colorEmojiSize)
    if(_colorEmojiSize LESS 9000000 OR _colorEmojiSize GREATER 16000000)
        message(FATAL_ERROR "Color emoji font size is outside expected range.")
    endif()
    file(SHA256 "${_genycasterColorEmoji}" _colorEmojiHash)
    if(NOT _colorEmojiHash STREQUAL
            "15671215ab769fdc7162a045d56fd7d7e477c51b04e6b3c761d914d8fdd6cc44")
        message(FATAL_ERROR "WebAssembly color emoji font integrity mismatch.")
    endif()
    set_source_files_properties("${_genycasterColorEmoji}" PROPERTIES
        QT_RESOURCE_ALIAS "assets/fonts/GenycasterColorEmoji.ttf")
    qt_add_resources(genycaster genycaster_emoji_fallback
        ${_genycasterLargeResources}
        PREFIX "/qt/qml/Farcaster/Client"
        FILES "${_genycasterEmojiFallback}"
              "${_genycasterColorEmoji}"
              "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-NotoEmoji.txt")
endif()

# Additional script coverage is optional for developer/source builds. Production
# packages may embed pinned Noto families, but configuring Genycaster must never
# require network access. When a script font is unavailable the runtime selects a
# native platform family with shaping support (Kohinoor/PingFang/Apple SD Gothic
# on macOS, Nirmala/Yu Gothic/Malgun/YaHei on Windows, Noto families on Linux).
#
# Set GENYCASTER_REQUIRE_SCRIPT_FONTS=ON in release/CI packaging if missing
# Devanagari/CJK resources should be treated as a packaging failure.
option(GENYCASTER_FETCH_SCRIPT_FONTS
    "Best-effort fetch of pinned open-source Devanagari/CJK fonts" ${EMSCRIPTEN})
option(GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS
    "Permit configure-time network access for optional script fonts" ${EMSCRIPTEN})
option(GENYCASTER_REQUIRE_SCRIPT_FONTS
    "Require Devanagari/CJK fonts to be embedded (recommended for release packaging)" ${EMSCRIPTEN})
if(EMSCRIPTEN)
    # Existing Qt Creator WASM build trees may carry cached OFF values from
    # before the script-font requirement. Never silently ship missing glyphs.
    # An explicit local GENYCASTER_SCRIPT_FONT_DIR bypasses downloading.
    set(GENYCASTER_REQUIRE_SCRIPT_FONTS ON CACHE BOOL
        "WASM needs bundled Devanagari/CJK scripts" FORCE)
    set(GENYCASTER_FETCH_SCRIPT_FONTS ON CACHE BOOL
        "WASM fetches pinned fonts when they are not supplied locally" FORCE)
    set(GENYCASTER_ALLOW_SCRIPT_FONT_DOWNLOADS ON CACHE BOOL
        "WASM permits script-font downloads for deterministic packaging" FORCE)
endif()
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
                EXPECTED_HASH SHA256=14ec4af41f27482216d1c2229f417ff9b1425e1babb014e57d1d40d03229853e
                TLS_VERIFY ON TIMEOUT 90 INACTIVITY_TIMEOUT 30
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
                EXPECTED_HASH SHA256=b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a
                TLS_VERIFY ON TIMEOUT 180 INACTIVITY_TIMEOUT 45
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

# Noto's 18.6 MiB TTC has ten faces. This app uses JP/KR/SC only.
# Remove seven unused faces, retaining the entire 44k+ Unicode cmap
# and the proper locale glyph forms in each of the three remaining faces.
option(GENYCASTER_OPTIMIZE_CJK_COLLECTION
    "Remove unused Noto CJK faces without dropping glyphs" ${EMSCRIPTEN})
set(GENYCASTER_CJK_FONTTOOLS_PYTHON "" CACHE FILEPATH
    "Python with fontTools for lossless CJK TTC release compaction")
if(EMSCRIPTEN AND GENYCASTER_OPTIMIZE_CJK_COLLECTION
   AND _genycasterCjkFontPath AND EXISTS "${_genycasterCjkFontPath}")
    if(GENYCASTER_CJK_FONTTOOLS_PYTHON)
        set(_cjkOptimizerPython "${GENYCASTER_CJK_FONTTOOLS_PYTHON}")
    else()
        find_package(Python3 COMPONENTS Interpreter QUIET)
        set(_cjkOptimizerPython "${Python3_EXECUTABLE}")
    endif()
    if(_cjkOptimizerPython)
        execute_process(
            COMMAND "${_cjkOptimizerPython}" -c "import fontTools.ttLib"
            RESULT_VARIABLE _fonttoolsAvailable OUTPUT_QUIET ERROR_QUIET)
        if(_fonttoolsAvailable EQUAL 0)
            set(_optimizedCjkPath
                "${CMAKE_BINARY_DIR}/_deps/genycaster-script-fonts-optimized/NotoSansCJK-Regular.ttc")
            execute_process(
                COMMAND "${_cjkOptimizerPython}"
                        "${PROJECT_SOURCE_DIR}/ci/optimize_cjk_collection.py"
                        "${_genycasterCjkFontPath}" "${_optimizedCjkPath}"
                RESULT_VARIABLE _cjkOptimizationResult
                OUTPUT_VARIABLE _cjkOptimizationOutput
                ERROR_VARIABLE _cjkOptimizationError)
            if(NOT _cjkOptimizationResult EQUAL 0)
                message(FATAL_ERROR
                    "CJK font optimization failed: ${_cjkOptimizationError}")
            endif()
            message(STATUS "${_cjkOptimizationOutput}")
            set(_genycasterCjkFontPath "${_optimizedCjkPath}")
        else()
            message(STATUS "fontTools unavailable; embedding original Noto CJK collection")
        endif()
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
    qt_add_resources(genycaster genycaster_script_fonts
        ${_genycasterLargeResources}
        PREFIX "/qt/qml/Farcaster/Client"
        FILES ${_genycasterScriptFontFiles})
endif()

# The WASM font resources use Qt's BIG_RESOURCES two-pass object embedding
# above, so even the large multilingual font bypasses generated hex-array
# C++ compilation. All application code retains normal Release optimization.

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
qt_add_resources(genycaster genycaster_icon_fonts
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
qt_add_resources(genycaster genyleap_icon_system
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
qt_add_resources(genycaster noto_emoji_lottie
    PREFIX "/qt/qml/Farcaster/Client/assets/emoji/lottie"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/lottie"
    FILES ${_notoEmojiLottieFiles}
)
qt_add_resources(genycaster noto_emoji_metadata
    PREFIX "/qt/qml/Farcaster/Client/assets/emoji"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/bootstrap_manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/emoji/LICENSE.NotoAnimatedEmoji.txt"
)

qt_add_resources(genycaster openproof_public_endpoints
    PREFIX "/genycaster-config"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/config"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/config/public_endpoints.json"
)

# One structured multilingual JSON source for all targets; GitHub Markdown is generated.
qt_add_resources(genycaster genycaster_release_notes
    PREFIX "/genycaster-release-notes"
    BASE "${CMAKE_SOURCE_DIR}/release-notes"
    FILES "${GENYCASTER_RELEASE_NOTES_FILE}"
)

if(ANDROID)
    find_package(Qt6 6.12 REQUIRED COMPONENTS WebView)
    target_link_libraries(genycaster PRIVATE Qt6::WebView)
endif()

qt_add_qml_module(genycaster
    URI Farcaster.Client
    VERSION 1.0
    QML_FILES
        ${farcasterQmlFiles}
        components/ProCastPolicy.js
    SOURCES
        render/scene_graph_stroke_item.cpp
        render/scene_graph_stroke_item.hpp
        render/scene_graph_superellipse_item.cpp
        render/scene_graph_superellipse_item.hpp
        mobile/feed_rows_model.cpp
        mobile/feed_rows_model.hpp
    RESOURCES
        shaders/setup_glass_g.frag.qsb
        shaders/genycaster_logo_sdf.png
        shaders/genycaster_logo_normals.png
        wasm/genycaster-icon.svg
        wasm/setup-mist.svg
        wasm/genycaster-wordmark.svg
        wasm/genycaster-wordmark-dark.svg
)

target_include_directories(genycaster
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/render"
        # The generated QML type registrar conditionally includes the native
        # item-model header by basename. Expose this Qt/MOC boundary explicitly.
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile"
)

# VectorImage track-matte composition needs Qt 6.11+ and the Qt Lottie vector-format plugin.
# Older/renderer-free SDKs retain the Unicode fallback without loading this type.
find_package(Qt6 QUIET COMPONENTS Lottie)
if(Qt6Lottie_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.11)
    target_compile_definitions(genycaster PRIVATE FARCASTER_HAS_LOTTIE=1)
    qt_import_plugins(genycaster INCLUDE Qt6::QLottieVectorImagePlugin)
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
    mobile/runtime_ui_bindings.cpp
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
    # Optional read provider's brand-specific translator metadata is owned by
    # the provider adapter, not the provider-neutral UI catalog.
    "${CMAKE_SOURCE_DIR}/src/providers/hypersnap/qt_translation_source.cpp"
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
        LUPDATE_TARGET genycaster_lupdate
    )
else()
    qt_add_lupdate(genycaster
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
add_dependencies(genycaster farcaster_translation_catalogs)

# Embed exactly the QM outputs produced above. The runtime contract is stable:
#   :/i18n/qml_en_US.qm
#   :/i18n/qml_fa_IR.qm
qt_add_resources(genycaster farcaster_translation_resources
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES
        ${farcasterTranslationQmFiles}
)

set(farcasterQmlLintOptions --bare)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    list(APPEND farcasterQmlLintOptions --max-warnings 0)
endif()

add_custom_target(genycaster_qmllint_strict
    COMMAND Qt6::qmllint
        ${farcasterQmlLintOptions}
        -I "${QT_QML_OUTPUT_DIRECTORY}"
        -I "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
        -M Farcaster.Client
    DEPENDS genycaster
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)

if(FARCASTER_BUILD_TESTS AND BUILD_TESTING)
    # A standalone real-Qt network/QObject test, independent of the giant
    # executable and its C++ module graph. The local test server makes request
    # batching, exact cast counts and unknown values deterministic.
    qt_add_executable(farcaster_cast_views_tests
        tests/cast_view_controller_tests.cpp
        mobile/cast_view_controller.cpp
        mobile/cast_view_controller.hpp)
    target_compile_features(farcaster_cast_views_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_cast_views_tests PRIVATE Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_cast_views_tests)
    add_test(NAME ui.cast_views_contract COMMAND farcaster_cast_views_tests)
    set_tests_properties(ui.cast_views_contract PROPERTIES
        LABELS "ui;analytics;qt"
        TIMEOUT 15)

    # Actual Qt HTTP/lifecycle fixture for the browser reaction adapter.
    # QML no longer owns or parses network replies.
    qt_add_executable(farcaster_browser_reactions_tests
        tests/browser_reaction_controller_tests.cpp
        mobile/browser_reaction_controller.cpp
        mobile/browser_reaction_controller.hpp)
    target_compile_features(farcaster_browser_reactions_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_browser_reactions_tests PRIVATE Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_browser_reactions_tests)
    add_test(NAME ui.browser_reactions_contract COMMAND farcaster_browser_reactions_tests)
    set_tests_properties(ui.browser_reactions_contract PROPERTIES
        LABELS "ui;reactions;qt;wasm"
        TIMEOUT 15)

    # Browser-safe link/miniapp metadata relay with actual bounded HTTP tests.
    qt_add_executable(farcaster_browser_link_metadata_tests
        tests/browser_link_metadata_controller_tests.cpp
        mobile/browser_link_metadata_controller.cpp
        mobile/browser_link_metadata_controller.hpp)
    target_compile_features(farcaster_browser_link_metadata_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_browser_link_metadata_tests PRIVATE Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_browser_link_metadata_tests)
    add_test(NAME ui.browser_link_metadata_contract
        COMMAND farcaster_browser_link_metadata_tests)
    set_tests_properties(ui.browser_link_metadata_contract PROPERTIES
        LABELS "ui;metadata;qt;wasm"
        TIMEOUT 15)

    qt_add_executable(farcaster_provider_translation_tests
        tests/provider_translation_catalog_tests.cpp)
    target_compile_features(farcaster_provider_translation_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_provider_translation_tests PRIVATE Qt6::Core)
    target_compile_definitions(farcaster_provider_translation_tests PRIVATE
        GENYCASTER_QM_EN="${CMAKE_CURRENT_BINARY_DIR}/qml_en_US.qm"
        GENYCASTER_QM_FA="${CMAKE_CURRENT_BINARY_DIR}/qml_fa_IR.qm")
    add_dependencies(farcaster_provider_translation_tests farcaster_translation_catalogs)
    farcaster_enable_warnings(farcaster_provider_translation_tests)
    add_test(NAME ui.provider_translation_contract COMMAND farcaster_provider_translation_tests)
    set_tests_properties(ui.provider_translation_contract PROPERTIES
        LABELS "ui;i18n;provider;qt"
        TIMEOUT 15)

    qt_add_executable(farcaster_provider_profiles_tests
        tests/provider_profiles_controller_tests.cpp
        mobile/provider_profiles_controller.cpp
        mobile/provider_profiles_controller.hpp)
    target_compile_features(farcaster_provider_profiles_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_provider_profiles_tests PRIVATE Qt6::Core Qt6::Network Qt6::Gui)
    target_compile_definitions(farcaster_provider_profiles_tests PRIVATE
        FARCASTER_HAS_SNAPCHAIN_READ=1
        FARCASTER_HAS_HYPERSNAP_PROVIDER=1
        FARCASTER_HAS_SNAPCHAIN_WRITE=1)
    farcaster_enable_warnings(farcaster_provider_profiles_tests)
    add_test(NAME ui.provider_profiles_contract COMMAND farcaster_provider_profiles_tests)
    set_tests_properties(ui.provider_profiles_contract PROPERTIES
        LABELS "ui;provider;qt;offline" TIMEOUT 15)

    qt_add_executable(farcaster_node_selection_tests
        tests/node_selection_controller_tests.cpp
        ../src/providers/hypersnap/node_selection_controller.cpp
        mobile/node_selection_controller.hpp)
    target_compile_features(farcaster_node_selection_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_node_selection_tests PRIVATE Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_node_selection_tests)
    add_test(NAME ui.node_selection_contract COMMAND farcaster_node_selection_tests)
    set_tests_properties(ui.node_selection_contract PROPERTIES
        LABELS "ui;provider;qt;offline"
        TIMEOUT 15)

    qt_add_executable(farcaster_browser_token_tests
        tests/browser_token_preview_controller_tests.cpp
        mobile/browser_token_preview_controller.cpp
        mobile/browser_token_preview_controller.hpp)
    target_compile_features(farcaster_browser_token_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_browser_token_tests PRIVATE Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_browser_token_tests)
    add_test(NAME ui.browser_token_contract COMMAND farcaster_browser_token_tests)
    set_tests_properties(ui.browser_token_contract PROPERTIES
        LABELS "ui;market;qt;wasm"
        TIMEOUT 15)

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(farcasterHeadlessPlatform "offscreen")
    if(APPLE)
        # Qt 6.11's macOS offscreen plugin emits a fatal generic-font alias
        # warning before project QML loads; the minimal plugin is warning-clean.
        set(farcasterHeadlessPlatform "minimal")
    endif()

    add_test(NAME ui.smoke COMMAND genycaster --smoke-test)
    set_tests_properties(ui.smoke PROPERTIES
        LABELS "ui;smoke"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.localization_en_smoke
        COMMAND genycaster --smoke-test --language=en
    )
    set_tests_properties(ui.localization_en_smoke PROPERTIES
        LABELS "ui;smoke;localization;ltr"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.localization_fa_smoke
        COMMAND genycaster
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
        COMMAND genycaster --smoke-test --theme-mode=dark --color-theme=geny
    )
    set_tests_properties(ui.appearance_dark_smoke PROPERTIES
        LABELS "ui;smoke;appearance;dark"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )

    add_test(
        NAME ui.appearance_light_smoke
        COMMAND genycaster --smoke-test --theme-mode=light --color-theme=ocean --reduce-motion=true
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
                "$<TARGET_FILE:genycaster>"
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
    add_executable(farcaster_openproof_connections_tests
        ../tests/ui/openproof_connections_tests.cpp
        mobile/openproof_connections_controller.cpp
        mobile/openproof_connections_controller.hpp
        mobile/browser_openproof_auth.cpp
        mobile/browser_openproof_auth.hpp)
    target_include_directories(farcaster_openproof_connections_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile")
    target_link_libraries(farcaster_openproof_connections_tests PRIVATE
        Qt6::Core Qt6::Network Qt6::Gui)
    farcaster_enable_warnings(farcaster_openproof_connections_tests)
    qt_add_resources(farcaster_openproof_connections_tests openproof_connections_test_config
        PREFIX "/genycaster-config"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/config"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/public_endpoints.json")
    add_test(NAME ui.openproof_connections COMMAND farcaster_openproof_connections_tests)
    set_tests_properties(ui.openproof_connections PROPERTIES TIMEOUT 10
        LABELS "ui;openproof;connections;security")

    add_executable(farcaster_channel_picker_controller_tests
        ../tests/ui/channel_picker_controller_tests.cpp
        mobile/channel_picker_controller.cpp
        mobile/channel_picker_controller.hpp)
    target_include_directories(farcaster_channel_picker_controller_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile")
    target_link_libraries(farcaster_channel_picker_controller_tests PRIVATE
        Qt6::Core Qt6::Network)
    farcaster_enable_warnings(farcaster_channel_picker_controller_tests)
    add_test(NAME ui.channel_picker_controller COMMAND farcaster_channel_picker_controller_tests)
    set_tests_properties(ui.channel_picker_controller PROPERTIES TIMEOUT 12
        LABELS "ui;channels;selection;network")

    add_executable(farcaster_update_controller_tests
        ../tests/ui/update_controller_tests.cpp
        mobile/update_controller.cpp
        mobile/update_controller.hpp)
    target_include_directories(farcaster_update_controller_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile")
    target_link_libraries(farcaster_update_controller_tests PRIVATE
        genycaster_release_identity
        Qt6::Core Qt6::Network Qt6::Gui)
    add_test(NAME ui.update_controller COMMAND farcaster_update_controller_tests)
    set_tests_properties(ui.update_controller PROPERTIES TIMEOUT 8 LABELS "ui;update;security")

    add_executable(farcaster_browser_wheel_fallback_tests
        ../tests/ui/browser_wheel_fallback_tests.cpp)
    target_link_libraries(farcaster_browser_wheel_fallback_tests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    add_test(NAME ui.browser_wheel_fallback COMMAND farcaster_browser_wheel_fallback_tests)
    set_tests_properties(ui.browser_wheel_fallback PROPERTIES TIMEOUT 15
        LABELS "ui;scroll;wasm"
        ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software")

    add_executable(farcaster_feed_rows_model_tests
        tests/feed_rows_model_tests.cpp
        mobile/feed_rows_model.cpp
        mobile/feed_rows_model.hpp)
    target_compile_features(farcaster_feed_rows_model_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_feed_rows_model_tests PRIVATE
        Qt6::Core Qt6::Qml Qt6::Test)
    farcaster_enable_warnings(farcaster_feed_rows_model_tests)
    add_test(NAME ui.feed_rows_model COMMAND farcaster_feed_rows_model_tests)
    set_tests_properties(ui.feed_rows_model PROPERTIES
        TIMEOUT 30 LABELS "ui;qt;model;read-pipeline")

    add_executable(farcaster_media_scroll_tests
        ../tests/ui/media_scroll_tests.cpp
        mobile/feed_rows_model.cpp
        mobile/feed_rows_model.hpp
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
    # Parse the complete native startup QML dependency chain with a real
    # QQmlEngine. qmlcache compilation alone misses unknown attached objects
    # such as Layout.minimumWidth without import QtQuick.Layouts.
    add_test(NAME ui.qml_import_smoke COMMAND farcaster_media_scroll_tests)
    set_tests_properties(ui.qml_import_smoke PROPERTIES TIMEOUT 20
        LABELS "ui;smoke;qml;native"
        ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1;GENYCASTER_QML_IMPORT_SMOKE_ONLY=1")
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
