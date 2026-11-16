# Parse the edited QML sources with the matching Qt 6.12 qmlformat.
# This is a syntax gate, not a substitute for the full QML module AOT build.
cmake_minimum_required(VERSION 3.24)
find_program(qmlformat_executable NAMES qmlformat
    HINTS "/opt/Qt/current/gcc_64/bin" "$ENV{QT_HOST_PATH}/bin"
          "$ENV{HOME}/Qt/6.12.0/gcc_64/bin")
if(NOT qmlformat_executable)
    message(FATAL_ERROR "Qt qmlformat unavailable")
endif()
set(files
    "ui/components/controls/GenyDrawer.qml"
    "ui/components/controls/GenyAdaptiveDialog.qml"
    "ui/components/controls/GenyCheckRow.qml"
    "ui/components/setup/InitialSetup.qml"
    "ui/components/visual/NeonHalo.qml"
    "ui/components/visual/NeonSurface.qml"
    "ui/components/home/HomeDashboard.qml"
    "ui/components/home/StatsPanel.qml"
    "ui/components/home/MarketPanel.qml"
    "ui/components/navigation/ContextRail.qml"
    "ui/components/feed/FeedMasonryView.qml"
    "ui/components/feed/FeedSurface.qml"
    "ui/components/feed/SemanticFeedItem.qml"
    "ui/components/feed/CastItem.qml"
    "ui/components/profile/ProfileSurface.qml"
    "ui/components/apps/AppsSurface.qml"
    "ui/components/profile/BookmarksSurface.qml"
    "ui/components/feed/NotificationSurface.qml"
    "ui/components/settings/SettingsPopup.qml"
    "ui/components/visual/DrawerBackdrop.qml")
foreach(file IN LISTS files)
    execute_process(
        COMMAND "${qmlformat_executable}" "${CMAKE_CURRENT_LIST_DIR}/../${file}"
        RESULT_VARIABLE code
        OUTPUT_QUIET
        ERROR_VARIABLE diagnostic)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "Invalid QML: ${file}: ${diagnostic}")
    endif()
endforeach()
message(STATUS "Neon QML syntax parsed (${qmlformat_executable})")
